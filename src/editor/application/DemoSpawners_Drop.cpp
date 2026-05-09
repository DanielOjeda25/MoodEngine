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

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

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

            // F2H17: si Face Mode + cara seleccionada del MISMO brush,
            // asignar material a esa cara (agregar slot si no existe).
            // Sino, comportamiento global F2H16 (slot 0).
            const SelectionSet& selSet = m_ui.selectionSet();
            const bool faceMode =
                m_subMode == EditorSubMode::Face &&
                static_cast<bool>(selSet.active) &&
                selSet.active.handle() == brushHit.entity.handle() &&
                selSet.activeFaceIndex() >= 0 &&
                static_cast<u32>(selSet.activeFaceIndex()) < bc.brush.faces.size();

            if (faceMode) {
                // F2H19: snapshot pre + push EditBrushFaceMaterialCommand.
                // Buscar si newMat ya esta en bc.materials. Si si,
                // reusar slot. Si no, agregar slot nuevo.
                const u32 faceIdx = static_cast<u32>(selSet.activeFaceIndex());
                std::vector<MaterialAssetId> oldMaterials = bc.materials;
                const u32 oldFaceMatIndex =
                    bc.brush.faces[faceIdx].materialIndex;

                u32 slot = 0;
                bool found = false;
                for (u32 i = 0; i < bc.materials.size(); ++i) {
                    if (bc.materials[i] == newMat) {
                        slot = i; found = true; break;
                    }
                }
                if (!found) {
                    slot = static_cast<u32>(bc.materials.size());
                    bc.materials.push_back(newMat);
                }
                bc.brush.faces[faceIdx].materialIndex = slot;
                bc.dirty = true;

                std::vector<MaterialAssetId> newMaterials = bc.materials;
                m_history.push(std::make_unique<EditBrushFaceMaterialCommand>(
                    m_scene.get(), tag, faceIdx,
                    std::move(oldMaterials), oldFaceMatIndex,
                    std::move(newMaterials), slot,
                    "Asignar textura a cara"));

                m_ui.setSelectedEntity(brushHit.entity);
                Log::editor()->info(
                    "Drop textura id={} -> brush '{}' cara {} (slot {})",
                    drop.textureId, tag, faceIdx, slot);
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

    // Hito 19: si el mesh tiene esqueleto + animaciones, auto-agregar
    // Animator + Skeleton para que se anime al instante. Sin esto el
    // mesh queda en bind pose estatico, lo cual es confuso para el
    // usuario que espera ver el zorro caminar.
    if (asset != nullptr && asset->hasSkeleton() && !asset->animations.empty()) {
        AnimatorComponent anim{};
        anim.clipName = "";   // primer clip
        anim.playing  = true;
        anim.loop     = true;
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

        // F2H17: si Face Mode + cara seleccionada del mismo brush,
        // asignar material a esa cara. Sino, slot 0 (Object Mode).
        const SelectionSet& selSet = m_ui.selectionSet();
        const bool faceMode =
            m_subMode == EditorSubMode::Face &&
            static_cast<bool>(selSet.active) &&
            selSet.active.handle() == hit.entity.handle() &&
            selSet.activeFaceIndex() >= 0 &&
            static_cast<u32>(selSet.activeFaceIndex()) < bc.brush.faces.size();

        if (faceMode) {
            // F2H19: snapshot pre + push EditBrushFaceMaterialCommand.
            const u32 faceIdx = static_cast<u32>(selSet.activeFaceIndex());
            std::vector<MaterialAssetId> oldMaterials = bc.materials;
            const u32 oldFaceMatIndex =
                bc.brush.faces[faceIdx].materialIndex;

            u32 slot = 0;
            bool found = false;
            for (u32 i = 0; i < bc.materials.size(); ++i) {
                if (bc.materials[i] == matId) {
                    slot = i; found = true; break;
                }
            }
            if (!found) {
                slot = static_cast<u32>(bc.materials.size());
                bc.materials.push_back(matId);
            }
            bc.brush.faces[faceIdx].materialIndex = slot;
            bc.dirty = true;

            std::vector<MaterialAssetId> newMaterials = bc.materials;
            m_history.push(std::make_unique<EditBrushFaceMaterialCommand>(
                m_scene.get(), tagName, faceIdx,
                std::move(oldMaterials), oldFaceMatIndex,
                std::move(newMaterials), slot,
                "Asignar material a cara"));

            m_ui.setSelectedEntity(hit.entity);
            Log::editor()->info(
                "Drop material id {} -> brush '{}' cara {} (slot {})",
                drop.materialId, tagName, faceIdx, slot);
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
