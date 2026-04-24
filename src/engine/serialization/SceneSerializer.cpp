#include "engine/serialization/SceneSerializer.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/JsonHelpers.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace Mood {

using json = nlohmann::json;

namespace {

// Prefijo de tag de entidades creadas por `rebuildSceneFromMap`. No se
// serializan porque se reconstruyen del GridMap tras cargar.
constexpr const char* k_tilePrefix = "Tile_";

bool isTileTag(const std::string& tag) {
    return tag.rfind(k_tilePrefix, 0) == 0;
}

json serializeEntity(Entity e, const AssetManager& assets) {
    json je;
    const auto& tag = e.getComponent<TagComponent>();
    je["tag"] = tag.name;

    const auto& t = e.getComponent<TransformComponent>();
    json jt;
    jt["position"]      = t.position;
    jt["rotationEuler"] = t.rotationEuler;
    jt["scale"]         = t.scale;
    je["transform"] = jt;

    if (e.hasComponent<MeshRendererComponent>()) {
        const auto& mr = e.getComponent<MeshRendererComponent>();
        json jmr;
        jmr["mesh_path"] = assets.meshPathOf(mr.mesh);
        jmr["materials"] = json::array();
        for (TextureAssetId texId : mr.materials) {
            jmr["materials"].push_back(assets.pathOf(texId));
        }
        je["mesh_renderer"] = jmr;
    }
    return je;
}

SavedEntity parseEntity(const json& je) {
    SavedEntity se;
    se.tag = je.value("tag", std::string{});
    if (je.contains("transform")) {
        const auto& jt = je.at("transform");
        se.position      = jt.value("position",      glm::vec3{0.0f});
        se.rotationEuler = jt.value("rotationEuler", glm::vec3{0.0f});
        se.scale         = jt.value("scale",         glm::vec3{1.0f});
    }
    if (je.contains("mesh_renderer")) {
        const auto& jmr = je.at("mesh_renderer");
        SavedMeshRenderer mr;
        mr.meshPath = jmr.value("mesh_path", std::string{});
        if (jmr.contains("materials")) {
            for (const auto& m : jmr.at("materials")) {
                mr.materials.push_back(m.get<std::string>());
            }
        }
        se.meshRenderer = std::move(mr);
    }
    return se;
}

} // namespace

void SceneSerializer::save(const GridMap& map, const std::string& name,
                           const Scene* scene, const AssetManager& assets,
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

    // Entidades no-tile (Hito 10 Bloque 6). Filtro por prefijo de tag:
    // las Tile_X_Y se reconstruyen del grid al cargar, asi que no se
    // persisten aqui (evita duplicacion + inconsistencias si el grid cambia).
    j["entities"] = json::array();
    if (scene != nullptr) {
        auto* mutableScene = const_cast<Scene*>(scene); // forEach requiere non-const
        mutableScene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
            if (isTileTag(tag.name)) return;
            // Solo serializamos entidades con MeshRenderer (primer pase).
            // Script/Audio/etc. quedan fuera de scope del Hito 10.
            if (!e.hasComponent<MeshRendererComponent>()) return;
            j["entities"].push_back(serializeEntity(e, assets));
        });
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("SceneSerializer::save: no se pudo abrir '" +
                                 path.generic_string() + "' para escritura");
    }
    out << j.dump(2); // indent de 2 para que sea legible a mano
    Log::assets()->info("Mapa guardado: {} ({} tiles solidos, {} entidades)",
                         path.generic_string(), map.solidCount(),
                         j["entities"].size());
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

        // Entidades no-tile (campo nuevo en v2; opcional para compat con v1).
        std::vector<SavedEntity> entities;
        if (j.contains("entities") && j.at("entities").is_array()) {
            for (const auto& je : j.at("entities")) {
                entities.push_back(parseEntity(je));
            }
        }

        Log::assets()->info(
            "Mapa cargado: {} ({} tiles solidos, {} entidades)",
            path.generic_string(), map.solidCount(), entities.size());
        return SavedMap{name, std::move(map), std::move(entities)};
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "SceneSerializer::load: falla semantica en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }
}

} // namespace Mood
