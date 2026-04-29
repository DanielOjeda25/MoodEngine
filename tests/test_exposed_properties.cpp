// Tests del binding `engine.exposed(name, default)` (Hito 24) + round-trip
// de overrides en `.moodmap`.
//
// Cubrimos:
//   1. Binding semantics: registro de metadata, default sin override,
//      override gana cuando esta seteado.
//   2. Inferencia de tipo: number / bool / string / vec3 (table de 3).
//      Tipos no soportados (function, etc.) se ignoran con warning.
//   3. Persistencia: SavedEntity con script + overrides round-trip por
//      JSON via SceneSerializer.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scripting/ExposedProperty.h"
#include "engine/scripting/LuaBindings.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/world/GridMap.h"

#include <sol/sol.hpp>

#include <filesystem>
#include <memory>
#include <string>

using namespace Mood;

namespace {

class NullTextureExposed : public ITexture {
public:
    explicit NullTextureExposed(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullFactoryExposed() {
    return [](const std::string& p) {
        return std::make_unique<NullTextureExposed>(p);
    };
}

std::filesystem::path tempPathExposed(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_test_exposed_") + suffix);
}

} // namespace

TEST_CASE("engine.exposed: registra metadata + devuelve default sin override") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    lua.safe_script("v = engine.exposed('speed', 45.0)");

    REQUIRE(sc.exposedProps.size() == 1);
    CHECK(sc.exposedProps[0].name == "speed");
    CHECK(sc.exposedProps[0].type == ExposedType::Number);
    CHECK(std::get<f32>(sc.exposedProps[0].defaultValue) == doctest::Approx(45.0f));
    CHECK(lua["v"].get<double>() == doctest::Approx(45.0));
}

TEST_CASE("engine.exposed: el override gana sobre el default") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});
    sc.overrides["speed"] = 90.0f;

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    lua.safe_script("v = engine.exposed('speed', 45.0)");

    CHECK(lua["v"].get<double>() == doctest::Approx(90.0));
    // El default registrado sigue siendo el del script (lo usa "Reset"
    // del Inspector), no el override.
    CHECK(std::get<f32>(sc.exposedProps[0].defaultValue) == doctest::Approx(45.0f));
}

TEST_CASE("engine.exposed: dedupe por nombre en multiples llamadas") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    lua.safe_script("a = engine.exposed('speed', 1.0)\n"
                    "b = engine.exposed('speed', 2.0)\n");

    CHECK(sc.exposedProps.size() == 1);
    // La segunda llamada actualiza el default (caso del dev cambiando
    // el script sin reiniciar).
    CHECK(std::get<f32>(sc.exposedProps[0].defaultValue) == doctest::Approx(2.0f));
}

TEST_CASE("engine.exposed: inferencia de tipo (bool / string / vec3)") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    lua.safe_script(
        "b = engine.exposed('active', true)\n"
        "s = engine.exposed('name', 'boss')\n"
        "v = engine.exposed('color', {1.0, 0.5, 0.2})\n");

    REQUIRE(sc.exposedProps.size() == 3);
    // Orden de inserción es el orden de las llamadas.
    CHECK(sc.exposedProps[0].type == ExposedType::Bool);
    CHECK(sc.exposedProps[1].type == ExposedType::String);
    CHECK(sc.exposedProps[2].type == ExposedType::Vec3);

    CHECK(lua["b"].get<bool>() == true);
    CHECK(lua["s"].get<std::string>() == "boss");

    sol::table t = lua["v"];
    CHECK(t.size() == 3);
    CHECK(t.get<double>(1) == doctest::Approx(1.0));
    CHECK(t.get<double>(2) == doctest::Approx(0.5));
    CHECK(t.get<double>(3) == doctest::Approx(0.2));
}

TEST_CASE("engine.exposed: tipo no soportado se ignora pero devuelve el default") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    // Funcion no es serializable: el binding loguea warn y devuelve el
    // default tal cual al script (que igual lo recibe como function).
    lua.safe_script("f = engine.exposed('cb', function() return 1 end)");

    CHECK(sc.exposedProps.empty()); // no se registra
    CHECK(lua["f"].get<sol::function>().valid());
}

TEST_CASE("engine.exposed: override de vec3 llega al script como tabla {x,y,z}") {
    Scene scene;
    Entity e = scene.createEntity("victim");
    auto& sc = e.addComponent<ScriptComponent>(std::string{"<inline>"});
    sc.overrides["color"] = glm::vec3(0.0f, 1.0f, 0.0f);

    sol::state lua;
    setupLuaBindings(lua, e, &sc);

    lua.safe_script("v = engine.exposed('color', {1.0, 0.0, 0.0})");

    sol::table t = lua["v"];
    CHECK(t.get<double>(1) == doctest::Approx(0.0));
    CHECK(t.get<double>(2) == doctest::Approx(1.0));
    CHECK(t.get<double>(3) == doctest::Approx(0.0));
}

TEST_CASE("SceneSerializer: round-trip de script.path + overrides en .moodmap") {
    AssetManager assets("assets", nullFactoryExposed());

    GridMap map(2u, 2u, 1.0f);
    Scene scene;
    Entity e = scene.createEntity("rotator_demo");
    e.getComponent<TransformComponent>().position = glm::vec3(3.0f, 0.0f, 5.0f);
    auto& sc = e.addComponent<ScriptComponent>(
        std::string{"assets/scripts/rotator.lua"});
    sc.overrides["speed"]  = 60.0f;
    sc.overrides["active"] = true;
    sc.overrides["label"]  = std::string{"main_boss"};
    sc.overrides["tint"]   = glm::vec3(1.0f, 0.5f, 0.25f);

    const auto path = tempPathExposed("script_overrides.moodmap");
    SceneSerializer::save(map, "exposed_test", &scene, assets, path);
    REQUIRE(std::filesystem::exists(path));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);

    const SavedEntity& se = loaded->entities[0];
    REQUIRE(se.script.has_value());
    CHECK(se.script->path == "assets/scripts/rotator.lua");

    REQUIRE(se.script->overrides.size() == 4);
    CHECK(std::get<f32>(se.script->overrides.at("speed"))
          == doctest::Approx(60.0f));
    CHECK(std::get<bool>(se.script->overrides.at("active")) == true);
    CHECK(std::get<std::string>(se.script->overrides.at("label")) == "main_boss");
    const glm::vec3 tint = std::get<glm::vec3>(se.script->overrides.at("tint"));
    CHECK(tint.x == doctest::Approx(1.0f));
    CHECK(tint.y == doctest::Approx(0.5f));
    CHECK(tint.z == doctest::Approx(0.25f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: ScriptComponent sin path no se serializa") {
    AssetManager assets("assets", nullFactoryExposed());

    GridMap map(1u, 1u, 1.0f);
    Scene scene;
    // Entidad con Light (componente serializable) + ScriptComponent vacio.
    // El light la hace persistible; el script con path vacio NO debe
    // aparecer en el JSON (back-compat con archivos viejos sin script).
    Entity e = scene.createEntity("just_light");
    e.addComponent<LightComponent>();
    e.addComponent<ScriptComponent>(std::string{});

    const auto path = tempPathExposed("empty_script.moodmap");
    SceneSerializer::save(map, "empty_script", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);
    CHECK_FALSE(loaded->entities[0].script.has_value());

    std::filesystem::remove(path);
}
