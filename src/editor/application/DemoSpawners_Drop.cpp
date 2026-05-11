// F2H24: drops del viewport (textura, mesh, material, script). Cada
// drop hace pick + asignacion + push command para undo. Lifecycle de
// prefab vive en DemoSpawners_Prefab.cpp.

#include "editor/application/EditorApplication.h"
#include "editor/application/DemoSpawners_Internal.h"

#include "core/Log.h"
#include "editor/commands/EditBrushFaceMaterialCommand.h"
#include "editor/commands/EditBrushMaterialCommand.h"
#include "editor/commands/EditMeshRendererMaterialCommand.h"
#include "editor/commands/EditScriptComponentCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/SetTileCommand.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/scene/queries/ViewportPick.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Mood {

namespace {

// F2H34: aplica `matId` a TODAS las caras seleccionadas del brush
// `brushEnt` cuando el editor esta en Face Mode con seleccion activa
// del mismo brush. Devuelve un command listo para pushear (o nullptr
// si no aplica — la llamadora debe caer al flow object-mode).
//
// Reusa slot existente si el material ya esta en bc.materials. Si es
// nuevo, hace UN solo push_back y todas las caras apuntan a ese slot.
//
// Pre: bc no mutado todavia. Post (si retorna != nullptr): bc.materials
// posiblemente con un slot mas, todas las caras del set apuntan al slot
// correspondiente, bc.dirty = true.
struct FaceDropResult {
    std::unique_ptr<EditBrushFaceMaterialCommand> cmd;
    u32 slotUsed = 0;
    size_t numFaces = 0;
};

FaceDropResult tryAssignMaterialToSelectedFaces(
    Scene* scene, Entity brushEnt, const std::string& tag,
    BrushComponent& bc, MaterialAssetId matId,
    const SelectionSet& selSet, EditorSubMode subMode,
    const std::string& label) {
    FaceDropResult r;
    if (subMode != EditorSubMode::Face) return r;
    if (!static_cast<bool>(selSet.active)) return r;
    if (selSet.active.handle() != brushEnt.handle()) return r;
    if (selSet.selectedFaceIndices.empty()) return r;

    // Validar que todas las caras seleccionadas estan en rango. Si una
    // sola esta corrupta, fallback a object-mode (sin mutar nada).
    std::vector<u32> faceIndices;
    faceIndices.reserve(selSet.selectedFaceIndices.size());
    for (i32 fi : selSet.selectedFaceIndices) {
        if (fi < 0 || static_cast<u32>(fi) >= bc.brush.faces.size()) {
            return r;
        }
        faceIndices.push_back(static_cast<u32>(fi));
    }

    // Snapshot pre.
    std::vector<MaterialAssetId> oldMaterials = bc.materials;
    std::vector<u32> oldFaceMatIndices;
    oldFaceMatIndices.reserve(faceIndices.size());
    for (u32 fi : faceIndices) {
        oldFaceMatIndices.push_back(bc.brush.faces[fi].materialIndex);
    }

    // Buscar/crear slot del material (un solo push_back si es nuevo;
    // todas las caras del set apuntan al mismo slot).
    u32 slot = 0;
    bool found = false;
    for (u32 i = 0; i < bc.materials.size(); ++i) {
        if (bc.materials[i] == matId) { slot = i; found = true; break; }
    }
    if (!found) {
        slot = static_cast<u32>(bc.materials.size());
        bc.materials.push_back(matId);
    }

    // Asignar slot a todas las caras + snapshot post.
    std::vector<u32> newFaceMatIndices(faceIndices.size(), slot);
    for (u32 fi : faceIndices) {
        bc.brush.faces[fi].materialIndex = slot;
    }
    bc.dirty = true;

    std::vector<MaterialAssetId> newMaterials = bc.materials;
    r.numFaces = faceIndices.size();
    r.slotUsed = slot;
    r.cmd = std::make_unique<EditBrushFaceMaterialCommand>(
        scene, tag, std::move(faceIndices),
        std::move(oldMaterials), std::move(oldFaceMatIndices),
        std::move(newMaterials), std::move(newFaceMatIndices),
        label);
    return r;
}

} // namespace

// F2H50 Bloque B: al spawnear un mesh skinneado, autoadjuntar todos los
// clips de animacion standalone hermanos del rig (`<rig>/anim_*.fbx`) al
// `AnimatorComponent.externalClips`. Convencion engine-grade:
//
//   assets/characters/<name>/<name>.fbx         <- rig (mesh + skin)
//   assets/characters/<name>/anim_<alias>.fbx   <- clip standalone
//
// El alias se deriva del filename stem (sin prefijo `anim_`). Si "idle"
// esta presente, se setea como `clipName` por default para que el char
// arranque animado en lugar del primer clip embedded (que en Mixamo
// "With Skin" suele ser el T-pose estatico llamado `mixamo_com`).
//
// El scan no es exclusivo de Mixamo ni de `assets/characters/`: funciona
// con CUALQUIER mesh cuya carpeta padre tenga `anim_*.fbx` siblings. Para
// chars sin esa estructura (Fox.glb, Kenney props), `externalClips` queda
// vacio y el comportamiento es identico al pre-F2H50 (primer embedded).
void detail::attachSiblingAnimClips(AssetManager& am,
                                       const std::string& meshLogicalPath,
                                       AnimatorComponent& outAnim) {
    namespace fs = std::filesystem;
    const fs::path meshLogical(meshLogicalPath);
    const fs::path fsDir = fs::path("assets") / meshLogical.parent_path();

    std::error_code ec;
    auto it = fs::directory_iterator(fsDir, ec);
    if (ec) return;

    constexpr std::string_view k_prefix = "anim_";
    int attachedCount = 0;
    for (const auto& entry : it) {
        if (!entry.is_regular_file()) continue;
        const fs::path& p = entry.path();

        // Filtrar .fbx (case-insensitive).
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (ext != ".fbx") continue;

        // Prefijo `anim_` (case-insensitive).
        const std::string stem = p.stem().string();
        std::string stemLower = stem;
        std::transform(stemLower.begin(), stemLower.end(), stemLower.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (stemLower.rfind(k_prefix, 0) != 0) continue;

        // Alias = stem sin prefijo (preserva el case del filename).
        std::string alias = stem.substr(k_prefix.size());
        if (alias.empty()) continue;

        // Logical path para el AssetManager.
        const std::string clipLogicalPath =
            (meshLogical.parent_path() / p.filename()).generic_string();

        const AnimationClipAssetId id =
            am.loadAnimationClip(clipLogicalPath);
        if (id == am.missingAnimationClipId()) continue;

        outAnim.externalClips.emplace_back(std::move(alias), id);
        ++attachedCount;
    }

    // Orden estable por alias (determinismo en serializacion + UI).
    std::sort(outAnim.externalClips.begin(), outAnim.externalClips.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Default a "idle" si esta entre los attached. Asi el char arranca
    // animado en lugar del T-pose embedded del Mixamo "With Skin".
    for (const auto& [alias, id] : outAnim.externalClips) {
        if (alias == "idle") {
            outAnim.clipName = "idle";
            break;
        }
    }

    if (attachedCount > 0) {
        Log::editor()->info(
            "MeshDrop: auto-attached {} sibling anim_*.fbx -> '{}' "
            "(clipName='{}')",
            attachedCount, meshLogicalPath, outAnim.clipName);
    }
}

void EditorApplication::processViewportTextureDrop() {
    const ViewportPanel::TextureDrop drop = m_ui.viewport().consumeTextureDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    // F2H15: prioridad 1 — si hay un BrushComponent bajo el cursor,
    // asignar la textura al brush (crear material wrapper). Esto
    // evita que el drop sobre un brush genere un tile-pared en el
    // grid de abajo.
    const auto texId = static_cast<TextureAssetId>(drop.textureId);
    if (m_scene && m_assetManager) {
        ScenePickResult brushHit = pickEntity(*m_scene, view, projection,
            glm::vec2(drop.ndcX, drop.ndcY),
            m_assetManager.get());
        if (brushHit && brushHit.entity.hasComponent<BrushComponent>()) {
            const MaterialAssetId newMat =
                m_assetManager->createMaterialFromTexture(texId);
            auto& bc = brushHit.entity.getComponent<BrushComponent>();
            const std::string tag =
                brushHit.entity.hasComponent<TagComponent>()
                    ? brushHit.entity.getComponent<TagComponent>().name
                    : std::string{"(sin tag)"};

            // F2H17 + F2H34: si Face Mode + N caras seleccionadas del
            // MISMO brush, asignar material a TODAS (un solo push_back
            // del slot, todas las caras apuntan ahi). Sino, fallback al
            // Object Mode (slot 0, F2H16).
            const SelectionSet& selSet = m_ui.selectionSet();
            FaceDropResult faceRes = tryAssignMaterialToSelectedFaces(
                m_scene.get(), brushHit.entity, tag, bc, newMat,
                selSet, m_subMode,
                selSet.selectedFaceIndices.size() > 1
                    ? "Asignar textura a caras"
                    : "Asignar textura a cara");
            if (faceRes.cmd) {
                m_history.push(std::move(faceRes.cmd));
                m_ui.setSelectedEntity(brushHit.entity);
                Log::editor()->info(
                    "Drop textura id={} -> brush '{}' {} cara(s) (slot {})",
                    drop.textureId, tag, faceRes.numFaces, faceRes.slotUsed);
                markDirty();
                return;
            }

            // Object Mode: asignar a slot 0 (material "global" del brush).
            const MaterialAssetId oldMat = bc.materials.empty()
                ? 0u : bc.materials[0];
            if (bc.materials.empty()) bc.materials.push_back(newMat);
            else bc.materials[0] = newMat;
            bc.dirty = true;
            m_history.push(std::make_unique<EditBrushMaterialCommand>(
                m_scene.get(), tag, oldMat, newMat,
                "Asignar textura a brush"));

            m_ui.setSelectedEntity(brushHit.entity);
            Log::editor()->info(
                "Drop textura id={} -> brush '{}' (material {})",
                drop.textureId, tag, newMat);
            markDirty();
            return;
        }
    }

    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(drop.ndcX, drop.ndcY));
    if (!hit.hit) return;

    // F2H16: snapshot pre-mutacion + push command.
    const TileType oldType = m_map.tileAt(hit.tileX, hit.tileY);
    const TextureAssetId oldTex = m_map.tileTextureAt(hit.tileX, hit.tileY);

    auto sync = [this](u32 tx, u32 ty, TextureAssetId tex) {
        updateTileEntity(tx, ty, tex);
    };

    // Aplicar y push.
    m_map.setTile(hit.tileX, hit.tileY, TileType::SolidWall, texId);
    updateTileEntity(hit.tileX, hit.tileY, texId);
    m_history.push(std::make_unique<SetTileCommand>(
        &m_map, std::move(sync), hit.tileX, hit.tileY,
        oldType, oldTex,
        TileType::SolidWall, texId,
        "Pintar tile"));

    Log::editor()->info("Drop textura id={} -> tile ({}, {})",
                         drop.textureId, hit.tileX, hit.tileY);
    markDirty();
}

void EditorApplication::processViewportMeshDrop() {
    const ViewportPanel::MeshDrop mdrop = m_ui.viewport().consumeMeshDrop();
    if (!(mdrop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);
    const TilePickResult hit = pickTile(m_map, mapWorldOrigin(), view, projection,
                                        glm::vec2(mdrop.ndcX, mdrop.ndcY));
    if (!hit.hit) return;

    const auto meshId = static_cast<MeshAssetId>(mdrop.meshId);
    const MeshAsset* asset = m_assetManager->getMesh(meshId);
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    // Nombre incremental por si spawneamos varios del mismo mesh.
    const std::string meshName = m_assetManager->meshPathOf(meshId);
    Entity e = m_scene->createEntity("Mesh_" + meshName);
    auto& t = e.getComponent<TransformComponent>();

    // Aplicar la rotacion de import primero — el AABB rotado nos da
    // la altura correcta para autoscale + floor offset (CesiumMan
    // tipo Z-up tiene altura en Z; si usaramos aabb.y crudo seria el
    // ancho de brazos y los calculos de abajo darian basura).
    const glm::vec3 importEuler = (asset != nullptr)
        ? asset->importRotationEuler : glm::vec3(0.0f);
    t.rotationEuler = importEuler;

    // Auto-escala bidireccional (Hito 28 G):
    //  - Meshes en cm-units (Fox.glb bind pose ~150 unidades, height >3m
    //    en metros): downscale a ~1.5m de altura.
    //  - Meshes ultra-chicos (height <0.1m, probablemente cm-units que
    //    quedaron sub-metro tras un import): upscale a ~1.5m.
    //  - Meshes en su escala nativa razonable (0.1m..3m: Kenney barriles
    //    de 0.27m, props normales, cubos): SIN autoscale. Antes un
    //    Kenney barril recibia scale 5.4x (1.5/0.276) y se veia grueso
    //    contra los tiles del mundo. Ahora se respeta el authoring.
    //  Si el dev quiere ajustar la escala despues del drop, usa el
    //  gizmo de scale (Ctrl+R) — y desde Hito 27 es undoable.
    f32 autoScale = 1.0f;
    detail::WorldYBounds wy{};
    if (asset != nullptr) {
        wy = detail::rotatedAabbWorldY(asset->aabbMin, asset->aabbMax, importEuler);
        const f32 height = wy.maxY - wy.minY;
        if (height > 3.0f) {
            autoScale = 1.5f / height;
        } else if (height > 0.001f && height < 0.1f) {
            autoScale = 1.5f / height;
        }
    }
    t.scale = glm::vec3(autoScale);

    // Posicion: XZ en el centro del tile bajo el cursor, Y al ras del
    // piso (world y=0). Anclamos el bottom del AABB rotado al piso.
    const f32 yFloorOffset = -autoScale * wy.minY;
    t.position = glm::vec3(
        origin.x + (static_cast<f32>(hit.tileX) + 0.5f) * tileSize,
        origin.y + yFloorOffset,
        origin.z + (static_cast<f32>(hit.tileY) + 0.5f) * tileSize);

    // Hito 26: usa las texturas extraidas del archivo si las hay
    // (Kenney Survival Kit, Fox.glb, etc). Cada drop tiene materiales
    // instance unicos para no contagiar edits.
    auto mats = m_assetManager->createMaterialsForMesh(meshId);
    e.addComponent<MeshRendererComponent>(meshId, std::move(mats));

    // Hito 19: si el mesh tiene esqueleto, auto-agregar Animator +
    // Skeleton para que se anime al instante (sin esto el mesh queda
    // en bind pose estatico, lo cual es confuso para el usuario que
    // espera ver el zorro caminar). F2H50 Bloque B: ademas escanea
    // `anim_*.fbx` hermanos del rig y los enchufa al
    // `AnimatorComponent.externalClips`, defaulteando a `idle` si esta.
    // Esto vale para CUALQUIER rig (no solo Mixamo) — convencion por
    // filename, no por carpeta.
    if (asset != nullptr && asset->hasSkeleton()) {
        AnimatorComponent anim{};
        anim.clipName = "";   // primer clip (override si auto-attach encuentra "idle")
        anim.playing  = true;
        anim.loop     = true;
        detail::attachSiblingAnimClips(*m_assetManager, meshName, anim);
        e.addComponent<AnimatorComponent>(anim);
        e.addComponent<SkeletonComponent>(SkeletonComponent{});
    }

    Log::editor()->info(
        "Drop mesh '{}' -> tile ({}, {}), autoscale={:.4f}{}",
        meshName, hit.tileX, hit.tileY, autoScale,
        (asset && asset->hasSkeleton()) ? " (skinned + animator)" : "");
    pushCreatedEntities({e}, "Drop mesh '" + meshName + "'");
}

void EditorApplication::processViewportMaterialDrop() {
    const ViewportPanel::MaterialDrop drop = m_ui.viewport().consumeMaterialDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    // Pick por entidad (usa los AABBs de meshes — mismo flow que el
    // click-select). Asignar material al PRIMER slot del MeshRenderer.
    ScenePickResult hit = pickEntity(*m_scene, view, projection,
        glm::vec2(drop.ndcX, drop.ndcY),
        m_assetManager.get());
    if (!hit) {
        Log::editor()->info("Drop material: no hay entidad bajo el cursor");
        return;
    }

    const auto matId = static_cast<MaterialAssetId>(drop.materialId);

    const std::string tagName = hit.entity.hasComponent<TagComponent>()
        ? hit.entity.getComponent<TagComponent>().name
        : std::string{"(sin tag)"};

    // F2H15: si la entidad bajo el cursor tiene BrushComponent,
    // asignar el material al brush (no al MeshRenderer). Brushes
    // CSG no tienen MeshRendererComponent.
    if (hit.entity.hasComponent<BrushComponent>()) {
        auto& bc = hit.entity.getComponent<BrushComponent>();

        // F2H17 + F2H34: si Face Mode + N caras seleccionadas del MISMO
        // brush, asignar material a TODAS. Sino, fallback a slot 0
        // (Object Mode).
        const SelectionSet& selSet = m_ui.selectionSet();
        FaceDropResult faceRes = tryAssignMaterialToSelectedFaces(
            m_scene.get(), hit.entity, tagName, bc, matId,
            selSet, m_subMode,
            selSet.selectedFaceIndices.size() > 1
                ? "Asignar material a caras"
                : "Asignar material a cara");
        if (faceRes.cmd) {
            m_history.push(std::move(faceRes.cmd));
            m_ui.setSelectedEntity(hit.entity);
            Log::editor()->info(
                "Drop material id {} -> brush '{}' {} cara(s) (slot {})",
                drop.materialId, tagName, faceRes.numFaces, faceRes.slotUsed);
            markDirty();
            return;
        }

        const MaterialAssetId oldMat = bc.materials.empty()
            ? 0u : bc.materials[0];
        if (bc.materials.empty()) bc.materials.push_back(matId);
        else bc.materials[0] = matId;
        bc.dirty = true;
        m_history.push(std::make_unique<EditBrushMaterialCommand>(
            m_scene.get(), tagName, oldMat, matId,
            "Asignar material a brush"));
        m_ui.setSelectedEntity(hit.entity);
        Log::editor()->info("Drop material id {} -> brush '{}'",
                              drop.materialId, tagName);
        markDirty();
        return;
    }

    if (!hit.entity.hasComponent<MeshRendererComponent>()) {
        Log::editor()->info("Drop material: la entidad no tiene MeshRenderer");
        return;
    }

    auto& mr = hit.entity.getComponent<MeshRendererComponent>();
    // F2H16: snapshot pre del slot 0 + push command.
    const MaterialAssetId oldMatMr = mr.materials.empty()
        ? 0u : mr.materials[0];
    if (mr.materials.empty()) {
        mr.materials.push_back(matId);
    } else {
        mr.materials[0] = matId;
    }
    // F2H16: push command. tagName ya capturado arriba.
    m_history.push(std::make_unique<EditMeshRendererMaterialCommand>(
        m_scene.get(), tagName, /*slotIndex=*/0u, oldMatMr, matId,
        "Asignar material a mesh"));
    m_ui.setSelectedEntity(hit.entity);
    Log::editor()->info("Drop material id {} -> entidad '{}'",
                         drop.materialId, tagName);
    markDirty();
}

void EditorApplication::processViewportScriptDrop() {
    ViewportPanel::ScriptDrop drop = m_ui.viewport().consumeScriptDrop();
    if (!(drop.pending && m_mode == EditorMode::Editor && m_scene)) return;

    const float aspect = viewportAspect();
    const glm::mat4 view = m_editorCamera.viewMatrix();
    const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

    ScenePickResult hit = pickEntity(*m_scene, view, projection,
        glm::vec2(drop.ndcX, drop.ndcY),
        m_assetManager.get());
    if (!hit) {
        Log::editor()->info("Drop script: no hay entidad bajo el cursor");
        return;
    }

    // F2H19: snapshot pre + apply + push EditScriptComponentCommand.
    // Si la entidad ya tenia ScriptComponent, capturamos su path para
    // que undo lo restaure. Si no tenia, undo remueve el componente.
    const std::string fullPath = std::string("assets/") + drop.scriptPath;
    const std::string tagName = hit.entity.hasComponent<TagComponent>()
        ? hit.entity.getComponent<TagComponent>().name
        : std::string{"(sin tag)"};
    const bool hadComponent = hit.entity.hasComponent<ScriptComponent>();
    std::string oldPath;
    if (hadComponent) {
        oldPath = hit.entity.getComponent<ScriptComponent>().path;
    }
    if (hadComponent) {
        auto& sc = hit.entity.getComponent<ScriptComponent>();
        sc.path      = fullPath;
        sc.loaded    = false; // forzar recarga via mtime check
        sc.lastError.clear();
    } else {
        hit.entity.addComponent<ScriptComponent>(fullPath);
    }
    m_history.push(std::make_unique<EditScriptComponentCommand>(
        m_scene.get(), tagName, hadComponent,
        std::move(oldPath), fullPath,
        "Asignar script a entidad"));
    m_ui.setSelectedEntity(hit.entity);

    Log::editor()->info("Drop script '{}' -> entidad '{}'",
                         drop.scriptPath, tagName);
    markDirty();
}

} // namespace Mood
