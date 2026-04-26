// Tests de SceneSerializer: round-trip completo de un mapa pequeno,
// manejo de archivos inexistentes / mal formados / version futura.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/JsonHelpers.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/world/GridMap.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;

namespace {

class NullTexture : public ITexture {
public:
    explicit NullTexture(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
    const std::string& path() const { return m_p; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTexture>(p); };
}

/// Ruta temporal dentro del temp del sistema. Cada test usa un suffix
/// distinto y borra el archivo al terminar para no ensuciar entre
/// corridas.
std::filesystem::path tempPath(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_test_") + suffix);
}

} // namespace

TEST_CASE("SceneSerializer: round-trip de un mapa 3x3 con texturas distintas") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId grid  = assets.loadTexture("textures/grid.png");
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");

    GridMap original(3u, 3u, 2.5f);
    original.setTile(0, 0, TileType::SolidWall, grid);
    original.setTile(2, 0, TileType::SolidWall, brick);
    original.setTile(1, 1, TileType::SolidWall, grid);
    // resto queda Empty con texture = 0 (missing)

    const auto path = tempPath("map_roundtrip.moodmap");
    SceneSerializer::save(original, "prueba", nullptr, assets, path);
    REQUIRE(std::filesystem::exists(path));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == "prueba");
    CHECK(loaded->map.width() == original.width());
    CHECK(loaded->map.height() == original.height());
    CHECK(loaded->map.tileSize() == doctest::Approx(original.tileSize()));

    for (u32 y = 0; y < original.height(); ++y) {
        for (u32 x = 0; x < original.width(); ++x) {
            CAPTURE(x); CAPTURE(y);
            CHECK(loaded->map.tileAt(x, y) == original.tileAt(x, y));
            CHECK(loaded->map.tileTextureAt(x, y) == original.tileTextureAt(x, y));
        }
    }

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load archivo inexistente devuelve nullopt") {
    AssetManager assets("assets", nullFactory());
    const auto missing = tempPath("no_existe.moodmap");
    std::filesystem::remove(missing); // por las dudas
    const auto r = SceneSerializer::load(missing, assets);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("SceneSerializer: load JSON mal formado devuelve nullopt") {
    const auto path = tempPath("broken.moodmap");
    {
        std::ofstream out(path);
        out << "{ esto no es json valido";
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load version futura devuelve nullopt") {
    const auto path = tempPath("future.moodmap");
    {
        nlohmann::json j;
        j["version"]  = k_MoodmapFormatVersion + 1;
        j["name"]     = "from_future";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load tiles count mismatch devuelve nullopt") {
    const auto path = tempPath("mismatch.moodmap");
    {
        nlohmann::json j;
        j["version"]  = k_MoodmapFormatVersion;
        j["name"]     = "bad";
        j["width"]    = 2;
        j["height"]   = 2; // esperaria 4 tiles
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", ""}},
            {{"type", "empty"}, {"texture", ""}}
            // faltan 2 tiles
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: save escribe un JSON parseable con los campos clave") {
    AssetManager assets("assets", nullFactory());
    GridMap m(2u, 1u, 3.0f);
    m.setTile(0, 0, TileType::SolidWall, assets.loadTexture("textures/grid.png"));

    const auto path = tempPath("parseable.moodmap");
    SceneSerializer::save(m, "inspeccion", nullptr, assets, path);

    nlohmann::json j;
    {
        std::ifstream in(path);
        in >> j;
    }
    CHECK(j.at("version") == k_MoodmapFormatVersion);
    CHECK(j.at("name") == "inspeccion");
    CHECK(j.at("width") == 2);
    CHECK(j.at("height") == 1);
    REQUIRE(j.at("tiles").size() == 2);
    CHECK(j.at("tiles")[0].at("type") == "solid_wall");
    CHECK(j.at("tiles")[0].at("texture") == "textures/grid.png");
    CHECK(j.at("tiles")[1].at("type") == "empty");

    std::filesystem::remove(path);
}

// --- Round-trip de entidades no-tile (Hito 10 Bloque 6) ---

TEST_CASE("SceneSerializer: round-trip entidades con MeshRenderer (Hito 10)") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");

    // Scene con 2 entidades no-tile + una tile-like (que NO deberia
    // serializarse por el filtro de prefijo "Tile_").
    Scene scene;
    {
        Entity a = scene.createEntity("Mesh_pyramid");
        auto& ta = a.getComponent<TransformComponent>();
        ta.position = glm::vec3(1.0f, 2.0f, 3.0f);
        ta.rotationEuler = glm::vec3(45.0f, 0.0f, 0.0f);
        ta.scale = glm::vec3(2.0f);
        a.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<TextureAssetId>{brick, brick});

        // Una tile-like que debe ser FILTRADA (solo por prefijo del tag).
        Entity t = scene.createEntity("Tile_0_0");
        t.addComponent<MeshRendererComponent>(MeshAssetId{0}, TextureAssetId{0});

        // Sin MeshRenderer: tambien filtrada (no tiene nada para serializar).
        Entity z = scene.createEntity("vacio");
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("entities_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1); // solo "Mesh_pyramid" entra

    const auto& se = loaded->entities[0];
    CHECK(se.tag == "Mesh_pyramid");
    CHECK(se.position.x == doctest::Approx(1.0f));
    CHECK(se.position.y == doctest::Approx(2.0f));
    CHECK(se.position.z == doctest::Approx(3.0f));
    CHECK(se.rotationEuler.x == doctest::Approx(45.0f));
    CHECK(se.scale.x == doctest::Approx(2.0f));
    REQUIRE(se.meshRenderer.has_value());
    // El mesh id 0 es el cubo fallback; path logico = "__missing_cube".
    CHECK(se.meshRenderer->meshPath == "__missing_cube");
    CHECK(se.meshRenderer->materials.size() == 2);
    CHECK(se.meshRenderer->materials[0] == "textures/brick.png");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de EnvironmentComponent (Hito 15)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    Entity env = scene.createEntity("Environment");
    EnvironmentComponent env_data{};
    env_data.skyboxPath     = "skyboxes/sky_night";
    env_data.fogMode        = 2; // Exp
    env_data.fogColor       = glm::vec3(0.8f, 0.4f, 0.1f);
    env_data.fogDensity     = 0.05f;
    env_data.fogLinearStart = 7.5f;
    env_data.fogLinearEnd   = 42.0f;
    env_data.exposure       = 1.5f;
    env_data.tonemapMode    = 1; // Reinhard
    env.addComponent<EnvironmentComponent>(env_data);

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("env_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo_env", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);

    const auto& se = loaded->entities[0];
    CHECK(se.tag == "Environment");
    REQUIRE(se.environment.has_value());
    const auto& s = *se.environment;
    CHECK(s.skyboxPath     == "skyboxes/sky_night");
    CHECK(s.fogMode        == "exp");
    CHECK(s.fogColor.x     == doctest::Approx(0.8f));
    CHECK(s.fogColor.y     == doctest::Approx(0.4f));
    CHECK(s.fogColor.z     == doctest::Approx(0.1f));
    CHECK(s.fogDensity     == doctest::Approx(0.05f));
    CHECK(s.fogLinearStart == doctest::Approx(7.5f));
    CHECK(s.fogLinearEnd   == doctest::Approx(42.0f));
    CHECK(s.exposure       == doctest::Approx(1.5f));
    CHECK(s.tonemapMode    == "reinhard");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: archivo v1 (sin 'entities') se carga con lista vacia") {
    // Simula un .moodmap viejo: sin campo `entities`. Debe seguir cargando.
    const auto path = tempPath("v1_legacy.moodmap");
    {
        nlohmann::json j;
        j["version"]  = 1; // version anterior
        j["name"]     = "legacy";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    REQUIRE(r.has_value());
    CHECK(r->entities.empty());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de LightComponent (Hito 11)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    // Una entidad con Light Point + Transform; debe persistir todos los campos.
    Entity p = scene.createEntity("Luz_demo");
    {
        auto& t = p.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 4.0f, 0.0f);
        LightComponent lc{};
        lc.type      = LightComponent::Type::Point;
        lc.color     = glm::vec3(1.0f, 0.95f, 0.85f);
        lc.intensity = 1.5f;
        lc.radius    = 12.0f;
        lc.enabled   = true;
        p.addComponent<LightComponent>(lc);
    }
    // Una entidad Directional sin Transform-relevante (la dir va en el componente).
    Entity d = scene.createEntity("Sol");
    {
        LightComponent lc{};
        lc.type      = LightComponent::Type::Directional;
        lc.direction = glm::vec3(0.0f, -1.0f, 0.3f);
        lc.color     = glm::vec3(1.0f, 0.8f, 0.6f);
        lc.intensity = 0.5f;
        lc.enabled   = true;
        d.addComponent<LightComponent>(lc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("light_roundtrip.moodmap");
    SceneSerializer::save(empty, "lit", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    // Buscar por tag (el orden de iteracion del registry no es estable).
    const SavedEntity* sePoint = nullptr;
    const SavedEntity* seDir   = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "Luz_demo") sePoint = &se;
        if (se.tag == "Sol")      seDir   = &se;
    }
    REQUIRE(sePoint != nullptr);
    REQUIRE(seDir   != nullptr);
    REQUIRE(sePoint->light.has_value());
    REQUIRE(seDir->light.has_value());

    CHECK(sePoint->light->type == "point");
    CHECK(sePoint->light->intensity == doctest::Approx(1.5f));
    CHECK(sePoint->light->radius    == doctest::Approx(12.0f));
    CHECK(sePoint->light->color.r   == doctest::Approx(1.0f));
    CHECK(sePoint->light->color.g   == doctest::Approx(0.95f));
    CHECK(sePoint->light->enabled);

    CHECK(seDir->light->type == "directional");
    CHECK(seDir->light->direction.y == doctest::Approx(-1.0f));
    CHECK(seDir->light->intensity   == doctest::Approx(0.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de RigidBodyComponent (Hito 12)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity box = scene.createEntity("CajaFisica");
    {
        auto& t = box.getComponent<TransformComponent>();
        t.position = glm::vec3(2.5f, 6.0f, -1.5f);
        t.scale = glm::vec3(1.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f, 0.75f, 0.5f);
        rb.mass = 5.0f;
        box.addComponent<RigidBodyComponent>(rb);
    }

    Entity floor = scene.createEntity("Piso");
    {
        auto& t = floor.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.shape = RigidBodyComponent::Shape::Sphere;
        rb.halfExtents = glm::vec3(2.0f);
        floor.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("rigidbody_roundtrip.moodmap");
    SceneSerializer::save(empty, "physics", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    const SavedEntity* seBox = nullptr;
    const SavedEntity* seFloor = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "CajaFisica") seBox = &se;
        if (se.tag == "Piso")       seFloor = &se;
    }
    REQUIRE(seBox   != nullptr);
    REQUIRE(seFloor != nullptr);
    REQUIRE(seBox->rigidBody.has_value());
    REQUIRE(seFloor->rigidBody.has_value());

    CHECK(seBox->rigidBody->type  == "dynamic");
    CHECK(seBox->rigidBody->shape == "box");
    CHECK(seBox->rigidBody->mass  == doctest::Approx(5.0f));
    CHECK(seBox->rigidBody->halfExtents.x == doctest::Approx(0.5f));
    CHECK(seBox->rigidBody->halfExtents.y == doctest::Approx(0.75f));

    CHECK(seFloor->rigidBody->type  == "static");
    CHECK(seFloor->rigidBody->shape == "sphere");
    CHECK(seFloor->rigidBody->halfExtents.x == doctest::Approx(2.0f));

    std::filesystem::remove(path);
}
