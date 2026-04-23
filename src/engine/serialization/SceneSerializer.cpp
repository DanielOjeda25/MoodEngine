#include "engine/serialization/SceneSerializer.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/serialization/JsonHelpers.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace Mood {

using json = nlohmann::json;

// Se reusa el canal `assets` por ahora; cuando agreguemos un canal
// dedicado `project` (Bloque 3) migramos estos logs.

void SceneSerializer::save(const GridMap& map, const std::string& name,
                           const AssetManager& assets,
                           const std::filesystem::path& path) {
    json j;
    j["version"]  = k_MoodmapFormatVersion;
    j["name"]     = name;
    j["width"]    = map.width();
    j["height"]   = map.height();
    j["tileSize"] = map.tileSize();

    // Tiles en row-major: y*width + x, mismo orden que GridMap::m_tiles.
    j["tiles"] = json::array();
    for (u32 y = 0; y < map.height(); ++y) {
        for (u32 x = 0; x < map.width(); ++x) {
            json tile;
            tile["type"]    = map.tileAt(x, y);
            tile["texture"] = assets.pathOf(map.tileTextureAt(x, y));
            j["tiles"].push_back(std::move(tile));
        }
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("SceneSerializer::save: no se pudo abrir '" +
                                 path.generic_string() + "' para escritura");
    }
    out << j.dump(2); // indent de 2 para que sea legible a mano
    Log::assets()->info("Mapa guardado: {} ({} tiles solidos)",
                                 path.generic_string(), map.solidCount());
}

std::optional<SavedMap> SceneSerializer::load(const std::filesystem::path& path,
                                              AssetManager& assets) {
    std::ifstream in(path);
    if (!in.is_open()) {
        Log::assets()->warn(
            "SceneSerializer::load: no se pudo abrir '{}'", path.generic_string());
        return std::nullopt;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "SceneSerializer::load: JSON invalido en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }

    try {
        checkFormatVersion(j, k_MoodmapFormatVersion, "moodmap");

        const std::string name = j.value("name", std::string{});
        const u32 width    = j.at("width").get<u32>();
        const u32 height   = j.at("height").get<u32>();
        const f32 tileSize = j.at("tileSize").get<f32>();
        const auto& tiles  = j.at("tiles");
        if (tiles.size() != static_cast<size_t>(width) * height) {
            throw std::runtime_error("cantidad de tiles no coincide con width*height");
        }

        GridMap map(width, height, tileSize);
        size_t i = 0;
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x, ++i) {
                const auto& t = tiles[i];
                const TileType type = t.at("type").get<TileType>();
                const std::string texPath = t.value("texture", std::string{});
                TextureAssetId texId = assets.missingTextureId();
                if (!texPath.empty()) {
                    texId = assets.loadTexture(texPath);
                }
                map.setTile(x, y, type, texId);
            }
        }

        Log::assets()->info(
            "Mapa cargado: {} ({} tiles solidos)",
            path.generic_string(), map.solidCount());
        return SavedMap{name, std::move(map)};
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "SceneSerializer::load: falla semantica en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }
}

} // namespace Mood
