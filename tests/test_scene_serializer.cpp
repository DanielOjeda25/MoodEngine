// Tests de SceneSerializer — core (AUDIT-3 split). GridMap round-trip,
// archivos mal formados / version futura / tile count mismatch, save
// format, MeshRenderer + Environment, upgrade de versiones v1/v6.
//
// Splits siblings (misma familia):
//   - test_scene_serializer_lighting_physics.cpp (Light + RigidBody)
//   - test_scene_serializer_gameplay.cpp         (Dialog + Pickup +
//                                                  Animator + Inventory)
// Helpers compartidos: test_scene_serializer_helpers.h.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/JsonHelpers.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"
#include "test_scene_serializer_helpers.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

using namespace Mood;
using Mood::SceneSerializerTests::nullFactory;
using Mood::SceneSerializerTests::tempPath;

TEST_CASE("SceneSerializer: round-trip de un mapa 3x3 con texturas distintas") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId grid  = assets.loadTexture("textures/grid.png");
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");

    GridMap original(3u, 3u, 2.5f);
    original.setTile(0, 0, TileType::SolidWall, grid);
    original.setTile(2, 0, TileType::SolidWall, brick);
    original.setTile(1, 1, TileType::SolidWall, grid);

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
    std::filesystem::remove(missing);
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
        j["height"]   = 2;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", ""}},
            {{"type", "empty"}, {"texture", ""}}
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
    const MaterialAssetId brickMat = assets.loadMaterialFromTexture(brick);

    Scene scene;
    {
        Entity a = scene.createEntity("Mesh_pyramid");
        auto& ta = a.getComponent<TransformComponent>();
        ta.position = glm::vec3(1.0f, 2.0f, 3.0f);
        ta.rotationEuler = glm::vec3(45.0f, 0.0f, 0.0f);
        ta.scale = glm::vec3(2.0f);
        a.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{brickMat, brickMat});

        Entity t = scene.createEntity("Tile_0_0");
        t.addComponent<MeshRendererComponent>(MeshAssetId{0}, MaterialAssetId{0});

        Entity z = scene.createEntity("vacio");
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("entities_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);

    const auto& se = loaded->entities[0];
    CHECK(se.tag == "Mesh_pyramid");
    CHECK(se.position.x == doctest::Approx(1.0f));
    CHECK(se.position.y == doctest::Approx(2.0f));
    CHECK(se.position.z == doctest::Approx(3.0f));
    CHECK(se.rotationEuler.x == doctest::Approx(45.0f));
    CHECK(se.scale.x == doctest::Approx(2.0f));
    REQUIRE(se.meshRenderer.has_value());
    CHECK(se.meshRenderer->meshPath == "__missing_cube");
    CHECK(se.meshRenderer->materials.size() == 2);
    // Hito 17: el material auto-generado a partir de una textura se
    // persiste como el path de la textura subyacente para back-compat
    // con el .moodmap v6.
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
    const auto path = tempPath("v1_legacy.moodmap");
    {
        nlohmann::json j;
        j["version"]  = 1;
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

TEST_CASE("SceneSerializer: v6 con texture path en 'materials' carga con upgrader (Hito 17)") {
    // El SavedMeshRenderer es agnostico (vector<string>); el upgrader que
    // envuelve texturas en materiales auto-generados vive en EditorProjectActions
    // (loadProject). Aca solo verificamos que el .moodmap v6 carga sin explotar
    // y que el path queda como string en el SavedEntity.
    const auto path = tempPath("v6_materials_as_textures.moodmap");
    {
        nlohmann::json j;
        j["version"]  = 6;
        j["name"]     = "legacy_v6";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        nlohmann::json je;
        je["tag"] = "Mesh_v6";
        je["transform"] = {
            {"position", nlohmann::json::array({0.0f, 0.0f, 0.0f})},
            {"rotationEuler", nlohmann::json::array({0.0f, 0.0f, 0.0f})},
            {"scale", nlohmann::json::array({1.0f, 1.0f, 1.0f})}};
        je["mesh_renderer"] = {
            {"mesh_path", "__missing_cube"},
            {"materials", nlohmann::json::array({"textures/brick.png"})}};
        j["entities"] = nlohmann::json::array({je});
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    REQUIRE(r.has_value());
    REQUIRE(r->entities.size() == 1);
    REQUIRE(r->entities[0].meshRenderer.has_value());
    CHECK(r->entities[0].meshRenderer->materials.size() == 1);
    CHECK(r->entities[0].meshRenderer->materials[0] == "textures/brick.png");
    std::filesystem::remove(path);
}
