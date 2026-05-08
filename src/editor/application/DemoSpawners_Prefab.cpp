// F2H24: handlers de prefab — guardar entidad como .moodprefab +
// instanciar prefab al hacer drop en el viewport.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/scene/queries/ViewportPick.h"
#include "engine/scene/serialization/PrefabSerializer.h"

#include <portable-file-dialogs.h>

#include <filesystem>
#include <string>
#include <utility>

namespace Mood {

void EditorApplication::processSavePrefabRequest() {
    if (!m_ui.consumeSavePrefabRequest()) return;
    Entity sel = m_ui.selectedEntity();
    if (!static_cast<bool>(sel)) {
        Log::editor()->warn("Guardar prefab: no hay entidad seleccionada");
        return;
    }
    if (!m_assetManager) {
        Log::editor()->warn("Guardar prefab: AssetManager no disponible");
        return;
    }

    // Default dir = <cwd>/assets/prefabs/ (creado al vuelo si no existe).
    // Esa es la carpeta que el AssetBrowser escanea.
    const auto prefabsDir =
        std::filesystem::current_path() / "assets" / "prefabs";
    std::error_code mkec;
    std::filesystem::create_directories(prefabsDir, mkec);

    // Sanitizar el filename: tags pueden tener caracteres invalidos para
    // Windows shell (`/` aparece p.ej. en tags tipo "Mesh_meshes/pyramid.obj_1"
    // del drop de mesh). Si el path default tiene un caracter invalido,
    // pfd::save_file en Windows devuelve "" SIN mostrar el dialogo —
    // el usuario ve "no aparece nada". Reemplazamos `<>:"/\|?*` por `_`.
    auto sanitize = [](std::string s) {
        for (char& c : s) {
            switch (c) {
                case '<': case '>': case ':': case '"':
                case '/': case '\\': case '|':
                case '?': case '*': c = '_'; break;
                default: break;
            }
        }
        return s;
    };
    const std::string baseTag =
        sel.hasComponent<TagComponent>()
            ? sel.getComponent<TagComponent>().name
            : std::string{"untitled"};
    const std::string defaultName = sanitize(baseTag) + ".moodprefab";

    // pfd::save_file en Windows usa el `default_path` como SetFolder +
    // SetFileName. Cuando el path tiene FILE+DIR, pfd espera separadores
    // nativos (backslashes en Windows); `generic_string()` los convierte a
    // forward slashes y el shell de Windows interpreta el path como
    // invalido => devuelve "" sin mostrar el dialogo.
    const auto sel_result = pfd::save_file(
        "Guardar como prefab",
        (prefabsDir / defaultName).string(),
        {"Prefabs MoodEngine (*.moodprefab)", "*.moodprefab"},
        pfd::opt::none).result();
    if (sel_result.empty()) {
        Log::editor()->info("Guardar prefab: cancelado");
        return;
    }
    auto outPath = std::filesystem::path(sel_result);
    if (outPath.extension() != ".moodprefab") {
        outPath += ".moodprefab";
    }
    try {
        const std::string prefabName = outPath.stem().generic_string();
        PrefabSerializer::save(sel, prefabName, *m_assetManager, outPath);

        // Path logico para el PrefabLinkComponent: relativo al cwd si el
        // archivo cayo en `assets/prefabs/`, si no, dejamos solo el filename
        // como referencia (loguea un warn para que el usuario sepa que el
        // browser no lo va a listar).
        std::error_code rec;
        const auto absOut = std::filesystem::absolute(outPath, rec);
        const auto absRoot = std::filesystem::absolute(
            std::filesystem::current_path(), rec);
        const auto rel = std::filesystem::relative(absOut, absRoot, rec);
        std::string logicalPath = rel.generic_string();
        const bool insideAssets =
            !rec && !logicalPath.empty()
                 && logicalPath.rfind("assets/prefabs/", 0) == 0;
        if (insideAssets) {
            // logical path para AssetManager: sacar el prefijo "assets/"
            // para que matchee con loadPrefab().
            logicalPath = logicalPath.substr(std::string("assets/").size());
        } else {
            Log::editor()->warn(
                "Guardar prefab: '{}' fuera de assets/prefabs/. "
                "El AssetBrowser solo lista prefabs en esa carpeta.",
                outPath.generic_string());
            logicalPath = outPath.filename().generic_string();
        }
        sel.addComponent<PrefabLinkComponent>(logicalPath);

        // Refrescar el AssetBrowser para que el nuevo prefab aparezca sin
        // reabrir el editor.
        m_ui.assetBrowser().rescan();
        m_ui.setStatusMessage("Prefab guardado: " + prefabName);
        markDirty();
    } catch (const std::exception& e) {
        Log::editor()->warn("Guardar prefab fallo: {}", e.what());
        pfd::message("MoodEngine",
            std::string("Error al guardar prefab: ") + e.what(),
            pfd::choice::ok, pfd::icon::error);
    }
}

void EditorApplication::processViewportPrefabDrop() {
    const ViewportPanel::PrefabDrop pdrop = m_ui.viewport().consumePrefabDrop();
    if (!(pdrop.pending && m_mode == EditorMode::Editor && m_scene && m_assetManager)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(pdrop.ndcX, pdrop.ndcY));
    const auto pid = static_cast<PrefabAssetId>(pdrop.prefabId);
    const SavedPrefab* sp = m_assetManager->getPrefab(pid);
    if (sp == nullptr || !hit.hit || pid == m_assetManager->missingPrefabId()) return;

    const std::string prefabPath = m_assetManager->prefabPathOf(pid);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    const glm::vec3 spawnPos(
        origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize,
        origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);

    // Tag del root: usamos el tag guardado en el prefab + un sufijo
    // numerico simple para distinguir multiples instancias (no garantiza
    // unicidad — entt::null permite duplicados).
    static u32 s_prefabSpawnCounter = 0;
    ++s_prefabSpawnCounter;
    const std::string instanceTag = (sp->root.tag.empty()
        ? std::string("Prefab")
        : sp->root.tag) + "_" + std::to_string(s_prefabSpawnCounter);

    Entity e = m_scene->createEntity(instanceTag);
    auto& t = e.getComponent<TransformComponent>();
    // Posicion = world-point del pick. Rotacion / scale del prefab se
    // preservan (override local viene despues via Inspector).
    t.position      = spawnPos;
    t.rotationEuler = sp->root.rotationEuler;
    t.scale         = sp->root.scale;

    if (sp->root.meshRenderer.has_value()) {
        const auto& mr = *sp->root.meshRenderer;
        const MeshAssetId meshId = m_assetManager->loadMesh(mr.meshPath);
        // Hito 17: cada slot del prefab persiste como `material_path` (puede
        // ser un .material o vacio). Empty -> default material slot 0.
        std::vector<MaterialAssetId> mats;
        mats.reserve(mr.materials.size());
        for (const auto& matPath : mr.materials) {
            mats.push_back(matPath.empty()
                ? m_assetManager->missingMaterialId()
                : m_assetManager->loadMaterial(matPath));
        }
        e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
    }
    if (sp->root.light.has_value()) {
        const auto& l = *sp->root.light;
        LightComponent lc;
        lc.type = (l.type == "directional")
            ? LightComponent::Type::Directional
            : LightComponent::Type::Point;
        lc.color     = l.color;
        lc.intensity = l.intensity;
        lc.radius    = l.radius;
        lc.direction = l.direction;
        lc.enabled   = l.enabled;
        lc.castShadows = l.castShadows; // Hito 16
        e.addComponent<LightComponent>(lc);
    }
    if (sp->root.rigidBody.has_value()) {
        const auto& r = *sp->root.rigidBody;
        RigidBodyComponent rb;
        if      (r.type == "static")    rb.type = RigidBodyComponent::Type::Static;
        else if (r.type == "kinematic") rb.type = RigidBodyComponent::Type::Kinematic;
        else                            rb.type = RigidBodyComponent::Type::Dynamic;
        if      (r.shape == "sphere")  rb.shape = RigidBodyComponent::Shape::Sphere;
        else if (r.shape == "capsule") rb.shape = RigidBodyComponent::Shape::Capsule;
        else                           rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = r.halfExtents;
        rb.mass        = r.mass;
        e.addComponent<RigidBodyComponent>(rb);
    }
    // Link al prefab — clave para futuro "revert/apply" (no implementado
    // en Hito 14 pero el breadcrumb queda).
    e.addComponent<PrefabLinkComponent>(prefabPath);

    m_ui.setSelectedEntity(e);
    Log::editor()->info("Drop prefab '{}' -> tile ({}, {})",
                         prefabPath, hit.tileX, hit.tileY);
    pushCreatedEntities({e}, "Drop prefab '" + prefabPath + "'");
}

} // namespace Mood
