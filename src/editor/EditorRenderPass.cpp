// `EditorApplication::renderSceneToViewport(dt)` — orquestador de
// frame del editor (Hito 21 Bloque 2 reescrito).
//
// El pipeline 3D real (sky + PBR + shadow + light grid + post-process)
// vive en `engine/render/SceneRenderer`. Aca solo:
//   1) Elegimos camara segun modo (Editor orbital / Play FPS) y armamos
//      view + projection.
//   2) Pedimos al SceneRenderer que pinte la escena al scene FB.
//   3) Agregamos el overlay 3D del editor (tile picking + drag
//      highlights + OBB de seleccion + F1 debug AABBs) usando el debug
//      renderer que el SceneRenderer expone.
//   4) Cerramos el frame: SceneRenderer flushea el debug, unbindea el
//      scene FB y aplica post-process al viewport FB.
//
// Lo del player es lo mismo pero sin paso 3.

#include "editor/EditorApplication.h"

#include "core/math/AABB.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/scene/queries/ViewportPick.h"

#include <glm/ext/matrix_transform.hpp>  // glm::translate, glm::rotate (Hito 40 B)
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>         // glm::radians
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Mood {

void EditorApplication::renderSceneToViewport(f32 dt) {
    (void)dt;

    if (!m_sceneRenderer) return;

    const u32 panelW = m_ui.viewport().desiredWidth();
    const u32 panelH = m_ui.viewport().desiredHeight();

    // Camara activa segun modo.
    const f32 aspect = (panelH > 0)
        ? static_cast<f32>(panelW) / static_cast<f32>(panelH)
        : 1.0f;
    glm::mat4 view, projection;
    glm::vec3 cameraPos;
    if (m_mode == EditorMode::Play) {
        view       = m_playCamera.viewMatrix();
        projection = m_playCamera.projectionMatrix(aspect);
        cameraPos  = m_playCamera.position();
    } else {
        view       = m_editorCamera.viewMatrix();
        projection = m_editorCamera.projectionMatrix(aspect);
        cameraPos  = m_editorCamera.position();
    }

    if (!m_scene) return;
    m_sceneRenderer->renderScene(*m_scene, *m_assetManager,
                                  view, projection, aspect, cameraPos,
                                  panelW, panelH);

    // Overlay 3D del editor: tile pick + drag highlights + OBB seleccion +
    // F1 debug. Solo aplica con scene FB todavia bindeado por SceneRenderer
    // (entre renderScene y endFrame). El player NO ejecuta este paso.
    drawEditorScene3DOverlay(view, projection, mapWorldOrigin());

    m_sceneRenderer->endFrame();
}

void EditorApplication::drawEditorScene3DOverlay(const glm::mat4& view,
                                                   const glm::mat4& projection,
                                                   const glm::vec3& worldOrigin) {
    if (!m_sceneRenderer) return;
    OpenGLDebugRenderer& dbg = m_sceneRenderer->debugRenderer();

    // Tile picking: solo en Editor Mode con cursor sobre la imagen.
    // Suprimido durante drag del gizmo (cyan se superpone con flechas).
    TilePickResult hovered{};
    if (m_mode == EditorMode::Editor && m_ui.viewport().imageHovered() &&
        !m_gizmo.active) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                            m_ui.viewport().mouseNdcY());
        hovered = pickTile(m_map, worldOrigin, view, projection, ndc);
    }
    m_hoveredTile = hovered;

    // F1 debug: AABBs de tiles solidos + AABB del player en Play Mode.
    if (m_debugDraw) {
        const glm::vec3 tileColor(1.0f, 0.85f, 0.15f);
        for (u32 ty = 0; ty < m_map.height(); ++ty) {
            for (u32 tx = 0; tx < m_map.width(); ++tx) {
                if (!m_map.isSolid(tx, ty)) continue;
                const AABB local = m_map.aabbOfTile(tx, ty);
                const AABB world{worldOrigin + local.min,
                                  worldOrigin + local.max};
                dbg.drawAabb(world, tileColor);
            }
        }
        if (m_mode == EditorMode::Play) {
            const glm::vec3 playerColor(0.2f, 1.0f, 0.4f);
            const glm::vec3 pos = m_playCamera.position();
            dbg.drawAabb(
                AABB{pos - k_playerHalfExtents, pos + k_playerHalfExtents},
                playerColor);
        }

        // Hito 23 Bloque 4: path activo de cada NavAgent como
        // polyline cyan. El waypoint actual (`pathIndex`) se marca
        // con un AABB chiquito brillante para distinguirlo.
        if (m_scene) {
            const glm::vec3 pathColor(0.2f, 0.9f, 1.0f);   // cyan
            const glm::vec3 cursorColor(0.0f, 1.0f, 1.0f); // cyan brillante
            const f32 tileSize = m_map.tileSize();
            auto tileToWorld = [&](const glm::ivec2& t) {
                return glm::vec3(
                    worldOrigin.x + (static_cast<f32>(t.x) + 0.5f) * tileSize,
                    worldOrigin.y + 0.5f * tileSize,
                    worldOrigin.z + (static_cast<f32>(t.y) + 0.5f) * tileSize);
            };
            m_scene->forEach<NavAgentComponent>(
                [&](Entity, NavAgentComponent& nav) {
                    if (nav.path.size() < 2) return;
                    for (usize i = 0; i + 1 < nav.path.size(); ++i) {
                        dbg.drawLine(tileToWorld(nav.path[i]),
                                      tileToWorld(nav.path[i + 1]),
                                      pathColor);
                    }
                    if (nav.pathIndex < nav.path.size()) {
                        const glm::vec3 c = tileToWorld(nav.path[nav.pathIndex]);
                        const glm::vec3 he(0.25f);
                        dbg.drawAabb(AABB{c - he, c + he}, cursorColor);
                    }
                });

            // Hito 40 B: OBB de cada TriggerComponent. Color verde
            // brillante. Construye los 8 vertices en local space y los
            // proyecta con position + rotation del Transform (ignora
            // scale para mantener `halfExtents` en metros directos —
            // mismo invariante del TriggerSystem). 12 aristas dibujadas
            // como `drawLine`. La rotation hace que se vea como un OBB
            // (no AABB) cuando la entidad esta rotada.
            const glm::vec3 trigColor(0.2f, 1.0f, 0.4f);
            m_scene->forEach<TransformComponent, TriggerComponent>(
                [&](Entity, TransformComponent& tf, TriggerComponent& tr) {
                    glm::mat4 m(1.0f);
                    m = glm::translate(m, tf.position);
                    m = glm::rotate(m, glm::radians(tf.rotationEuler.y),
                                     glm::vec3(0, 1, 0));
                    m = glm::rotate(m, glm::radians(tf.rotationEuler.x),
                                     glm::vec3(1, 0, 0));
                    m = glm::rotate(m, glm::radians(tf.rotationEuler.z),
                                     glm::vec3(0, 0, 1));
                    const glm::vec3& he = tr.halfExtents;
                    glm::vec3 corners[8];
                    int idx = 0;
                    for (int xi = -1; xi <= 1; xi += 2)
                    for (int yi = -1; yi <= 1; yi += 2)
                    for (int zi = -1; zi <= 1; zi += 2) {
                        const glm::vec4 local(
                            static_cast<f32>(xi) * he.x,
                            static_cast<f32>(yi) * he.y,
                            static_cast<f32>(zi) * he.z,
                            1.0f);
                        corners[idx++] = glm::vec3(m * local);
                    }
                    // 12 aristas del cubo. Indices: bit 0=x, bit 1=y,
                    // bit 2=z. Aristas conectan corners que difieren en
                    // exactamente 1 bit.
                    for (int i = 0; i < 8; ++i) {
                        for (int b = 0; b < 3; ++b) {
                            const int j = i ^ (1 << b);
                            if (j > i) {
                                dbg.drawLine(corners[i], corners[j], trigColor);
                            }
                        }
                    }
                });
        }
    }

    // Highlights de drag-and-drop. Distinguen segun el tipo de payload:
    //   - Texture / Mesh / Prefab apuntan a un TILE -> cubo cyan sobre
    //     el tile bajo el cursor (raycast vs grid).
    //   - Material apunta a una ENTIDAD -> OBB amarillo sobre el mesh
    //     bajo el cursor (raycast vs Scene).
    using DragKind = ViewportPanel::AssetDragKind;
    const DragKind dragKind = m_ui.viewport().assetDragKind();
    const bool dragIsTile =
        dragKind == DragKind::Texture
        || dragKind == DragKind::Mesh
        || dragKind == DragKind::Prefab;
    if (hovered.hit && dragIsTile) {
        const glm::vec3 hoverColor(0.2f, 0.9f, 1.0f); // cyan
        const AABB local = m_map.aabbOfTile(hovered.tileX, hovered.tileY);
        const AABB world{worldOrigin + local.min, worldOrigin + local.max};
        dbg.drawAabb(world, hoverColor);
    }
    // Script drop tambien apunta a una entidad — mismo highlight OBB
    // amarillo que Material para mantener consistencia visual (Hito 22
    // Bloque 2). Para entidades sin mesh (Light/Audio) el OBB no se
    // dibuja porque el branch interno verifica `MeshRendererComponent`.
    const bool dragTargetsEntity =
        dragKind == DragKind::Material || dragKind == DragKind::Script;
    if (dragTargetsEntity && m_scene
        && m_mode == EditorMode::Editor
        && m_ui.viewport().imageHovered()) {
        const glm::vec2 ndc(m_ui.viewport().mouseNdcX(),
                             m_ui.viewport().mouseNdcY());
        ScenePickResult ehit = pickEntity(*m_scene, view, projection, ndc,
                                            m_assetManager.get());
        if (ehit && ehit.entity.hasComponent<TransformComponent>()
                  && ehit.entity.hasComponent<MeshRendererComponent>()) {
            const auto& tf = ehit.entity.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();
            constexpr glm::vec3 kCorners[8] = {
                {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
                { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
                {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
                { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(kCorners[i], 1.0f);
                w[i] = glm::vec3(p);
            }
            constexpr int kEdges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}};
            const glm::vec3 dropColor(1.0f, 0.95f, 0.15f);
            for (const auto& e : kEdges) {
                dbg.drawLine(w[e[0]], w[e[1]], dropColor);
            }
        }
    }

    // Outline 3D de la entidad seleccionada (OBB naranja estilo
    // Blender/Unity). Para entidades sin mesh (Light/Audio) no se
    // dibuja: el icono 2D ya las marca con halo cyan en el overlay
    // del ViewportPanel.
    if (m_scene && m_mode == EditorMode::Editor) {
        Entity sel = m_ui.selectedEntity();
        if (sel && sel.hasComponent<TransformComponent>() &&
            sel.hasComponent<MeshRendererComponent>()) {
            const auto& tf = sel.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();
            constexpr glm::vec3 kCorners[8] = {
                {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
                { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
                {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
                { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(kCorners[i], 1.0f);
                w[i] = glm::vec3(p);
            }
            constexpr int kEdges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}};
            const glm::vec3 selColor(1.0f, 0.55f, 0.05f); // naranja Blender
            for (const auto& e : kEdges) {
                dbg.drawLine(w[e[0]], w[e[1]], selColor);
            }
        }
    }
}

} // namespace Mood
