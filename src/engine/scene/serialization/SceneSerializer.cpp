#include "engine/scene/serialization/SceneSerializer.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/VisGroup.h"  // F2H33: visgroupIdOf
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/EntitySerializer.h"
#include "engine/scene/serialization/JsonHelpers.h"
#include "engine/scene/serialization/TilePersistence.h"
#include "engine/world/csg/CompileMap.h"  // F2H26: buildSavedCompiledMeshFromScene
#include "engine/world/grid/GridMap.h"

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

// Wrappers locales que delegan al EntitySerializer compartido. Mismo nombre
// que la implementacion legacy del Hito 12 para minimizar el diff con el
// resto del archivo (ver `EntitySerializer.{h,cpp}`).
inline json serializeEntity(Entity e, const AssetManager& assets) {
    return serializeEntityToJson(e, assets);
}
inline SavedEntity parseEntity(const json& j) {
    return parseEntityFromJson(j);
}

// F2H11: helpers de (de)serializacion de SavedBrush.

json serializeBrush(Entity e, const AssetManager& assets) {
    auto& tag = e.getComponent<TagComponent>();
    auto& t   = e.getComponent<TransformComponent>();
    auto& bc  = e.getComponent<BrushComponent>();

    json out;
    out["tag"] = tag.name;
    out["transform"] = {
        {"position",      t.position},
        {"rotationEuler", t.rotationEuler},
        {"scale",         t.scale},
    };
    // Material por path logico. Slot 0 (default sentinel) se serializa
    // como "" para que el archivo no tenga un path interno raro.
    // F2H17: serializar TODOS los slots de material como array.
    // Back-compat v11: si solo hay 1 slot escribimos tambien el
    // campo legacy "material" (string) para que readers v11 puedan
    // leerlo. v12 readers prefieren "materials".
    json materialsArr = json::array();
    for (MaterialAssetId mid : bc.materials) {
        materialsArr.push_back(
            (mid == 0) ? std::string{} : assets.materialPathOf(mid));
    }
    out["materials"] = std::move(materialsArr);
    // Compat v11: tambien escribir "material" como path del slot 0
    // (puede ayudar a herramientas externas que lean v11).
    const MaterialAssetId mat0 = bc.materials.empty() ? 0u : bc.materials[0];
    out["material"] = (mat0 == 0)
        ? std::string{} : assets.materialPathOf(mat0);

    out["faces"] = json::array();
    for (const auto& face : bc.brush.faces) {
        json jf = {
            {"normal",        face.plane.normal},
            {"distance",      face.plane.distance},
            {"materialIndex", face.materialIndex},
        };
        // F2H15: UV params per-cara (v11). Siempre escribimos los
        // 6 campos para que el round-trip sea exacto. Faces v10
        // sin estos campos se leen con defaults en parseBrush.
        jf["uAxis"]       = face.uAxis;
        jf["vAxis"]       = face.vAxis;
        jf["uvOffset"]    = face.uvOffset;
        jf["uvScale"]     = face.uvScale;
        jf["uvRotation"]  = face.uvRotation;
        jf["lockToWorld"] = face.lockToWorld;
        out["faces"].push_back(std::move(jf));
    }
    // F2H33 (v14): visgroupId opcional. Solo se emite si el brush
    // pertenece a un grupo (componente presente con groupId != 0).
    const u64 vgId = visgroupIdOf(e);
    if (vgId != 0) {
        out["visgroupId"] = vgId;
    }
    return out;
}

/// @brief F2H26: serializa un SavedCompiledSubmesh a JSON. Layout
///        PBR de 11 floats por vertex; indices ya expandidos.
json serializeCompiledSubmesh(const SavedCompiledSubmesh& sub) {
    json out;
    out["materialPath"] = sub.materialPath;
    out["vertices"]     = sub.vertices;
    return out;
}

/// @brief F2H26: parsea un SavedCompiledSubmesh desde JSON. No tira si
///        falta el array; devuelve un submesh vacio.
SavedCompiledSubmesh parseCompiledSubmesh(const json& j) {
    SavedCompiledSubmesh sub;
    sub.materialPath = j.value("materialPath", std::string{});
    if (j.contains("vertices") && j.at("vertices").is_array()) {
        sub.vertices = j.at("vertices").get<std::vector<f32>>();
    }
    return sub;
}

SavedBrush parseBrush(const json& j) {
    SavedBrush sb;
    sb.tag = j.value("tag", std::string{});
    if (j.contains("transform")) {
        const auto& jt = j.at("transform");
        sb.position      = jt.value("position",      glm::vec3(0.0f));
        sb.rotationEuler = jt.value("rotationEuler", glm::vec3(0.0f));
        sb.scale         = jt.value("scale",         glm::vec3(1.0f));
    }
    sb.materialPath = j.value("material", std::string{});
    // F2H17 (v12): leer array materialPaths si existe; sino sintetizar
    // del materialPath legacy.
    if (j.contains("materials") && j.at("materials").is_array()) {
        for (const auto& jm : j.at("materials")) {
            sb.materialPaths.push_back(jm.is_string() ? jm.get<std::string>()
                                                       : std::string{});
        }
    } else if (!sb.materialPath.empty()) {
        sb.materialPaths.push_back(sb.materialPath);
    } else {
        sb.materialPaths.push_back(std::string{});  // 1 slot vacio (default)
    }
    if (j.contains("faces") && j.at("faces").is_array()) {
        for (const auto& jf : j.at("faces")) {
            SavedBrushFace face;
            face.normal        = jf.value("normal", glm::vec3(0, 1, 0));
            face.distance      = jf.value("distance", 0.0f);
            face.materialIndex = jf.value("materialIndex", 0u);
            // F2H15: UV params (v11). Faces v10 caen a los defaults
            // del struct (uAxis/vAxis canonicos +X/+Y, scale 1,
            // offset 0, rotation 0, lockToWorld false). El loader
            // recomputa uAxis/vAxis auto desde la normal si los
            // que llegan son los default canonicos — eso garantiza
            // que faces v10 cargan con el mismo tangent basis que
            // las primitivas generan en F2H15.
            face.uAxis       = jf.value("uAxis",      face.uAxis);
            face.vAxis       = jf.value("vAxis",      face.vAxis);
            face.uvOffset    = jf.value("uvOffset",   face.uvOffset);
            face.uvScale     = jf.value("uvScale",    face.uvScale);
            face.uvRotation  = jf.value("uvRotation", face.uvRotation);
            face.lockToWorld = jf.value("lockToWorld", face.lockToWorld);
            sb.faces.push_back(face);
        }
    }
    // F2H33 (v14): visgroupId opcional, default 0 (sin grupo).
    sb.visgroupId = j.value("visgroupId", u64{0});
    return sb;
}

} // namespace

void SceneSerializer::save(const GridMap& map, const std::string& name,
                           const Scene* scene, const AssetManager& assets,
                           const std::filesystem::path& path,
                           const SavedCompiledMesh* compiledMesh) {
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

    // Entidades no-tile (Hito 10 Bloque 6). Tiles default (Tile_X_Y sin
    // modificaciones) se reconstruyen del grid al cargar y NO se persisten.
    // Tiles MODIFICADOS por el user (estirados, con material custom, con
    // componentes extras) SI se persisten — el bug previo era filtrarlos
    // por prefijo Tile_* siempre, lo que destruia los cambios en el round-trip.
    // El helper isTileModified() decide caso por caso.
    j["entities"] = json::array();
    if (scene != nullptr) {
        auto* mutableScene = const_cast<Scene*>(scene); // forEach requiere non-const
        mutableScene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
            // Tiles del grid: persistir solo si fueron modificados.
            if (isTileTag(tag.name) && !isTileModified(e, map, assets)) {
                return;
            }
            // F2H11: entidades con BrushComponent se persisten en el
            // array `brushes`, NO en `entities`. Saltearlas aqui evita
            // doble-persistencia y entradas en `entities` que el loader
            // no sabria interpretar (no tienen mesh, light, etc.).
            if (e.hasComponent<BrushComponent>()) return;
            // Persistimos cualquier entidad con al menos un componente
            // serializable: MeshRenderer (Hito 10), Light (Hito 11),
            // RigidBody (Hito 12), Environment (Hito 15), Script con
            // path no-vacio (Hito 24) o ParticleEmitter (Hito 29).
            // Audio sigue fuera de scope.
            const bool hasMr  = e.hasComponent<MeshRendererComponent>();
            const bool hasLi  = e.hasComponent<LightComponent>();
            const bool hasRb  = e.hasComponent<RigidBodyComponent>();
            const bool hasEnv = e.hasComponent<EnvironmentComponent>();
            const bool hasScript = e.hasComponent<ScriptComponent>()
                && !e.getComponent<ScriptComponent>().path.empty();
            const bool hasPe  = e.hasComponent<ParticleEmitterComponent>();
            // F2H51: InventoryComponent es serializable standalone — un
            // chest/container puede no tener mesh visible.
            const bool hasInv = e.hasComponent<InventoryComponent>();
            // F2H67: VehicleComponent puede aparecer en una entity con
            // MeshRenderer (caso normal) o standalone si el visual va a
            // child-entities. Tambien las entities-placeholder de las
            // wheels (tags Wheel_FL/FR/RL/RR) son standalone -- las
            // marcamos para persistir aunque solo tengan Tag+Transform.
            const bool hasVeh = e.hasComponent<VehicleComponent>();
            const bool isWheelTag =
                tag.name == "Wheel_FL" || tag.name == "Wheel_FR" ||
                tag.name == "Wheel_RL" || tag.name == "Wheel_RR";
            if (!hasMr && !hasLi && !hasRb && !hasEnv && !hasScript
                && !hasPe && !hasInv && !hasVeh && !isWheelTag) return;
            j["entities"].push_back(serializeEntity(e, assets));
        });
    }

    // F2H11: brushes CSG. Cada entidad con BrushComponent se persiste
    // como un objeto en el array top-level `brushes` (paralelo a
    // `entities`). Las entidades brush NO entran en el array
    // `entities` porque su contenido (planos, no mesh asset) no encaja
    // en el flow estandar de EntitySerializer.
    j["brushes"] = json::array();
    if (scene != nullptr) {
        auto* mutableScene = const_cast<Scene*>(scene);
        mutableScene->forEach<BrushComponent>([&](Entity e, BrushComponent&) {
            if (!e.hasComponent<TransformComponent>()) return;
            j["brushes"].push_back(serializeBrush(e, assets));
        });
    }

    // F2H26: persistir mesh compilada si el caller la suministra. El
    // Player la usa al cargar para saltarse el procesamiento CSG.
    if (compiledMesh != nullptr) {
        json jcm;
        jcm["submeshes"] = json::array();
        for (const auto& sub : compiledMesh->submeshes) {
            jcm["submeshes"].push_back(serializeCompiledSubmesh(sub));
        }
        j["compiledMesh"] = std::move(jcm);
    }

    // F2H33 (v14): array `visgroups` con los grupos planos del scene.
    // Solo se emite si hay al menos uno — mapas sin grupos no ensucian
    // el JSON con el array vacio.
    if (scene != nullptr && !scene->visgroups().empty()) {
        json jvgs = json::array();
        for (const VisGroup& vg : scene->visgroups()) {
            json jvg;
            jvg["id"] = vg.id;
            jvg["name"] = vg.name;
            jvg["color"] = vg.color;
            if (vg.hidden) jvg["hidden"] = true;  // omitido si false
            jvgs.push_back(std::move(jvg));
        }
        j["visgroups"] = std::move(jvgs);
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

        // F2H11: brushes CSG (campo nuevo en v10; opcional para
        // compat con v9 y anteriores).
        std::vector<SavedBrush> brushes;
        if (j.contains("brushes") && j.at("brushes").is_array()) {
            for (const auto& jb : j.at("brushes")) {
                brushes.push_back(parseBrush(jb));
            }
        }

        // F2H26: mesh compilada (campo nuevo en v13; opcional). Si no
        // existe, mapas v12 siguen leyendo igual (Player cae al
        // fallback de procesar brushes en runtime).
        std::optional<SavedCompiledMesh> compiledMesh;
        if (j.contains("compiledMesh") && j.at("compiledMesh").is_object()) {
            const auto& jcm = j.at("compiledMesh");
            SavedCompiledMesh cm;
            if (jcm.contains("submeshes") && jcm.at("submeshes").is_array()) {
                for (const auto& js : jcm.at("submeshes")) {
                    cm.submeshes.push_back(parseCompiledSubmesh(js));
                }
            }
            compiledMesh = std::move(cm);
        }

        // F2H33 (v14): array `visgroups`. Mapas v13 sin el array =
        // vector vacio (todas las entities sin membership component al
        // aplicar al scene).
        std::vector<VisGroup> visgroups;
        if (j.contains("visgroups") && j.at("visgroups").is_array()) {
            for (const auto& jvg : j.at("visgroups")) {
                VisGroup vg;
                vg.id     = jvg.value("id",     u64{0});
                vg.name   = jvg.value("name",   std::string{});
                vg.color  = jvg.value("color",  glm::vec3{0.7f, 0.7f, 0.7f});
                vg.hidden = jvg.value("hidden", false);
                if (vg.id != 0) visgroups.push_back(std::move(vg));
            }
        }

        Log::assets()->info(
            "Mapa cargado: {} ({} tiles solidos, {} entidades, {} brushes, "
            "{} visgroups{})",
            path.generic_string(), map.solidCount(), entities.size(),
            brushes.size(), visgroups.size(),
            compiledMesh.has_value() ? ", compiledMesh" : "");
        SavedMap result{name, std::move(map), std::move(entities),
                         std::move(brushes), std::move(compiledMesh),
                         std::move(visgroups)};
        return result;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "SceneSerializer::load: falla semantica en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }
}

SavedCompiledMesh buildSavedCompiledMeshFromScene(Scene& scene,
                                                    AssetManager& assets) {
    // F2H26: pipeline editor -> compiledMesh persistido. Pasos:
    //   1) Collect: itera la scene -> vector<BrushSource>.
    //   2) Compile: aplica F2H20 (cull exacto) + F2H25 (cull overlap
    //      parcial) + weld global. Resultado: 1 submesh por path
    //      logico de material, vertices+indices CCW.
    //   3) Convert: cada CompiledSubmesh al layout PBR interleaved
    //      (11 floats por vertex, sin EBO).
    SavedCompiledMesh out;
    auto sources = Csg::collectBrushSourcesFromScene(scene, assets);
    if (sources.empty()) return out;
    auto compiled = Csg::compileMap(sources);
    out.submeshes.reserve(compiled.submeshes.size());
    for (const auto& sub : compiled.submeshes) {
        SavedCompiledSubmesh saved;
        saved.materialPath = sub.materialPath;
        saved.vertices = Csg::compiledSubmeshToInterleaved(sub);
        out.submeshes.push_back(std::move(saved));
    }
    return out;
}

} // namespace Mood
