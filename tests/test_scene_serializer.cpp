// Tests de SceneSerializer: round-trip completo de un mapa pequeno,
// manejo de archivos inexistentes / mal formados / version futura.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
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
    SceneSerializer::save(original, "prueba", assets, path);
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
    SceneSerializer::save(m, "inspeccion", assets, path);

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
