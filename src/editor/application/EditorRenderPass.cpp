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

#include "editor/application/EditorApplication.h"

#include "core/math/AABB.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"  // F2H44: AABB del mesh para outline
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/scene/components/BrushComponent.h"  // F2H13
#include "engine/scene/components/Components.h"
#include "engine/world/csg/Brush.h"  // F2H17: collectFaceWorldPolygon
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

    // F2H28: si el workspace activo es "Editor de mapas", renderear los
    // 3 viewports ortograficos (Top/Front/Side) con shader wireframe
    // dedicado. Skip en otros workspaces para no pagar el ~3x costo
    // CPU del render extra.
    const auto& activeWs = m_ui.workspaceManager().activeWorkspace();
    // F2H44: comparacion contra ID ASCII (`map_editor`), no contra el
    // label visible que cambia con el idioma.
    if (activeWs.name == "map_editor") {
        const std::vector<Entity>& selected = m_ui.selectionSet().selected;
        const f32 snapStep = static_cast<f32>(m_hammerSnapStep);
        OrthoViewportPanel* orthoPanels[3] = {
            &m_ui.orthoTop(),
            &m_ui.orthoFront(),
            &m_ui.orthoSide(),
        };
        for (usize i = 0; i < 3; ++i) {
            OrthoViewportPanel* op = orthoPanels[i];
            const u32 oW = op->desiredWidth();
            const u32 oH = op->desiredHeight();
            if (oW == 0 || oH == 0) continue;
            const f32 oAspect = static_cast<f32>(oW) / static_cast<f32>(oH);
            const glm::mat4 oView = op->camera().viewMatrix();
            const glm::mat4 oProj = op->camera().projMatrix(oAspect);
            // F2H29 Bloque C: re-encolar el preview AABB del block tool
            // en el debugRenderer ANTES del renderOrthoView. El flush
            // de adentro lo dibuja con las matrices del orto. Cada
            // iteracion re-queues porque el flush limpia la cola.
            if (m_orthoBlockSession.active &&
                m_orthoBlockSession.previewValid && m_sceneRenderer) {
                // F2H29 Bloque C: celeste GMod (paleta Valve+GMod
                // del workspace; mismo RGB que el wireframe regular
                // de los brushes en SceneRenderer_Ortho.cpp).
                const glm::vec3 gmodCelesteRGB(108.0f / 255.0f,
                                                193.0f / 255.0f,
                                                229.0f / 255.0f);
                m_sceneRenderer->debugRenderer().drawAabb(
                    AABB{m_orthoBlockSession.previewMin,
                         m_orthoBlockSession.previewMax},
                    gmodCelesteRGB);
            }
            // F2H31 Bloque D: frustum de la 3D cam dibujado en cada
            // orto. 4 lineas amarillas claras desde la pos de la cam
            // a las 4 esquinas de un "rect de mira" a distancia
            // representativa (4 world units) — ayuda al dev a ubicar
            // donde mira la perspectiva sin alt-tabbear al 3D viewport.
            if (m_mode == EditorMode::Editor && m_sceneRenderer) {
                const glm::vec3 camPos = m_editorCamera.position();
                const glm::mat4 vMat = m_editorCamera.viewMatrix();
                // Camera basis en world via la transpose del 3x3
                // superior-izquierdo del view matrix.
                const glm::vec3 right = glm::vec3(
                    vMat[0][0], vMat[1][0], vMat[2][0]);
                const glm::vec3 up = glm::vec3(
                    vMat[0][1], vMat[1][1], vMat[2][1]);
                const glm::vec3 fwd = -glm::vec3(
                    vMat[0][2], vMat[1][2], vMat[2][2]);
                const f32 fovYrad =
                    m_editorCamera.fovDeg() * 3.1415926f / 180.0f;
                const f32 distance = 4.0f;
                const f32 halfH = distance * std::tan(fovYrad * 0.5f);
                // Aspect del 3D viewport (no del orto).
                const f32 perspAspect = (panelH > 0)
                    ? static_cast<f32>(panelW) / static_cast<f32>(panelH)
                    : 1.0f;
                const f32 halfW = halfH * perspAspect;
                const glm::vec3 npCenter = camPos + fwd * distance;
                const glm::vec3 cTL = npCenter - right * halfW + up * halfH;
                const glm::vec3 cTR = npCenter + right * halfW + up * halfH;
                const glm::vec3 cBR = npCenter + right * halfW - up * halfH;
                const glm::vec3 cBL = npCenter - right * halfW - up * halfH;
                const glm::vec3 frusColor(1.0f, 0.95f, 0.4f);
                auto& dbgR = m_sceneRenderer->debugRenderer();
                // Rect de mira (4 lados).
                dbgR.drawLine(cTL, cTR, frusColor);
                dbgR.drawLine(cTR, cBR, frusColor);
                dbgR.drawLine(cBR, cBL, frusColor);
                dbgR.drawLine(cBL, cTL, frusColor);
                // Lineas desde la posicion de la cam al rect (forma el
                // tronco). Mas tenue para que no compita con el rect.
                const glm::vec3 frusEdge(0.6f, 0.55f, 0.2f);
                dbgR.drawLine(camPos, cTL, frusEdge);
                dbgR.drawLine(camPos, cTR, frusEdge);
                dbgR.drawLine(camPos, cBR, frusEdge);
                dbgR.drawLine(camPos, cBL, frusEdge);
            }
            // F2H35 Bloque D (orto): point entities (Light/Audio/Trigger/
            // Camera/Particle sin mesh) se renderean como un cubo
            // wireframe MUY pequeno del color del tipo en los 3 ortos
            // (estilo Hammer Source clasico: la forma es solo "marker
            // de posicion" y la diferenciacion real va al label del
            // tag en Bloque E proximo). Tamano absoluto chico
            // proporcional al snap pero clampeado, no escala con zoom.
            if (m_mode == EditorMode::Editor && m_sceneRenderer) {
                auto& dbgR = m_sceneRenderer->debugRenderer();
                constexpr f32 r = 0.4f;
                // F2H35 fix: feedback de seleccion para point entities
                // en orto. Pre-fix los brushes/meshes cambiaban a
                // naranja Hammer al seleccionarlos pero los point
                // entities seguian del color del tipo — el dev no veia
                // que estaba seleccionado. Ahora el cubo se pinta
                // naranja `selectedColor` cuando esta en el SelectionSet
                // (mismo color que SceneRenderer_Ortho usa para
                // brushes/meshes selectos). Same-frame, sin acoplamiento
                // al ECS porque chequeamos contra el SelectionSet
                // expuesto via EditorUI.
                const auto& selSet = m_ui.selectionSet();
                const glm::vec3 selectedColor(1.0f, 0.6f, 0.2f);
                m_scene->forEach<TransformComponent>(
                    [&](Entity e, TransformComponent& t) {
                        const bool hasMesh =
                            e.hasComponent<MeshRendererComponent>();
                        const bool hasBrush =
                            e.hasComponent<BrushComponent>();
                        if (hasMesh || hasBrush) return;
                        glm::vec3 col;
                        if (e.hasComponent<LightComponent>()) {
                            col = glm::vec3(1.00f, 0.88f, 0.40f);
                        } else if (e.hasComponent<AudioSourceComponent>()) {
                            col = glm::vec3(1.00f, 0.55f, 0.15f);
                        } else if (e.hasComponent<TriggerComponent>()) {
                            col = glm::vec3(0.30f, 0.85f, 0.35f);
                        } else if (e.hasComponent<CameraComponent>()) {
                            col = glm::vec3(0.45f, 0.65f, 1.00f);
                        } else if (e.hasComponent<ParticleEmitterComponent>()) {
                            col = glm::vec3(0.85f, 0.45f, 0.95f);
                        } else {
                            return;
                        }
                        // F2H35: si esta en el SelectionSet, override a
                        // naranja Hammer (consistente con los brushes/
                        // meshes selectos).
                        bool isSelected = false;
                        for (const Entity& s : selSet.selected) {
                            if (s && s.handle() == e.handle()) {
                                isSelected = true;
                                break;
                            }
                        }
                        if (isSelected) col = selectedColor;
                        const glm::vec3 he(r);
                        dbgR.drawAabb(AABB{t.position - he,
                                            t.position + he}, col);
                    });
            }
            // F2H32 Bloque B: clip tool preview. p1 marker + linea de
            // p1 al cursor (rubber band) si solo hay p1; o linea
            // p1->p2 si ambos. Color amarillo claro consistente con
            // el frustum y marquee. Solo en la orto donde se hizo el
            // primer click.
            if (m_clipTool.active &&
                m_clipTool.orthoIdx == static_cast<int>(i) &&
                m_sceneRenderer) {
                auto& dbgR = m_sceneRenderer->debugRenderer();
                const glm::vec3 clipColor(1.0f, 0.85f, 0.0f);
                const f32 clipMarkerHalf = std::max(snapStep * 0.05f, 0.05f);
                const glm::vec3 chm(clipMarkerHalf);
                if (m_clipTool.hasP1) {
                    dbgR.drawAabb(AABB{m_clipTool.p1World - chm,
                                        m_clipTool.p1World + chm}, clipColor);
                    glm::vec3 endPt = m_clipTool.p1World;
                    if (m_clipTool.hasP2) {
                        endPt = m_clipTool.p2World;
                        dbgR.drawAabb(AABB{m_clipTool.p2World - chm,
                                            m_clipTool.p2World + chm}, clipColor);
                    } else {
                        // Rubber band hasta el cursor live.
                        const auto& lc = op->liveCursor();
                        if (lc.hovered) {
                            endPt = op->camera().worldFromNdc(
                                lc.ndcX, lc.ndcY, oAspect);
                            endPt = snapToVertexOrGrid(endPt,
                                                        op->camera(), oAspect);
                        }
                    }
                    dbgR.drawLine(m_clipTool.p1World, endPt, clipColor);
                }
            }
            // F2H31 Bloque B: rectangulo amarillo del marquee select.
            // Se dibuja solo en la orto donde arranco el drag. 4 lineas
            // sobre el plano del view (worldFromNdc en las 4 esquinas).
            if (m_orthoMarquee.active &&
                m_orthoMarquee.orthoIdx == static_cast<int>(i) &&
                m_sceneRenderer) {
                const auto& ds = op->dragState();
                if (ds.active) {
                    const glm::vec3 marqColor(1.0f, 0.85f, 0.0f);
                    const f32 ndcMinX = std::min(ds.ndcStartX, ds.ndcCurX);
                    const f32 ndcMaxX = std::max(ds.ndcStartX, ds.ndcCurX);
                    const f32 ndcMinY = std::min(ds.ndcStartY, ds.ndcCurY);
                    const f32 ndcMaxY = std::max(ds.ndcStartY, ds.ndcCurY);
                    const auto& cam = op->camera();
                    const glm::vec3 c00 = cam.worldFromNdc(ndcMinX, ndcMinY, oAspect);
                    const glm::vec3 c10 = cam.worldFromNdc(ndcMaxX, ndcMinY, oAspect);
                    const glm::vec3 c11 = cam.worldFromNdc(ndcMaxX, ndcMaxY, oAspect);
                    const glm::vec3 c01 = cam.worldFromNdc(ndcMinX, ndcMaxY, oAspect);
                    auto& dbgR = m_sceneRenderer->debugRenderer();
                    dbgR.drawLine(c00, c10, marqColor);
                    dbgR.drawLine(c10, c11, marqColor);
                    dbgR.drawLine(c11, c01, marqColor);
                    dbgR.drawLine(c01, c00, marqColor);
                }
            }
            // F2H30 Bloque C: re-encolar el preview del pincel poligonal
            // en los ortos (incluye rubber band del ultimo vertex al
            // cursor live de la orto lockeada).
            if (m_polyDraw.active && m_polyDraw.pointsWorld.size() > 0
                && m_sceneRenderer) {
                auto& dbgR = m_sceneRenderer->debugRenderer();
                const glm::vec3 polyColor(1.0f, 1.0f, 1.0f);
                const f32 snap = static_cast<f32>(m_hammerSnapStep);
                const f32 markerHalf = std::max(snap * 0.05f, 0.05f);
                const glm::vec3 he(markerHalf);
                for (usize k = 0; k < m_polyDraw.pointsWorld.size(); ++k) {
                    const glm::vec3& p = m_polyDraw.pointsWorld[k];
                    dbgR.drawAabb(AABB{p - he, p + he}, polyColor);
                    if (k + 1 < m_polyDraw.pointsWorld.size()) {
                        dbgR.drawLine(p, m_polyDraw.pointsWorld[k + 1],
                                       polyColor);
                    }
                }
                if (m_polyDraw.pointsWorld.size() >= 3) {
                    const glm::vec3 closeColor(0.5f, 0.5f, 0.5f);
                    dbgR.drawLine(m_polyDraw.pointsWorld.back(),
                                   m_polyDraw.pointsWorld.front(),
                                   closeColor);
                }
                // Rubber band desde ultimo vertex al cursor.
                if (m_polyDraw.orthoIdx >= 0) {
                    OrthoViewportPanel* lockedOp =
                        orthoPanels[m_polyDraw.orthoIdx];
                    const auto& lc = lockedOp->liveCursor();
                    if (lc.hovered && lockedOp->desiredHeight() > 0) {
                        const f32 oA = static_cast<f32>(lockedOp->desiredWidth())
                                      / static_cast<f32>(lockedOp->desiredHeight());
                        glm::vec3 cursorWorld =
                            lockedOp->camera().worldFromNdc(
                                lc.ndcX, lc.ndcY, oA);
                        // F2H31 Bloque C: el rubber band tambien snap-
                        // to-vertex si el toggle esta on (el dev ve a
                        // donde se va a "pegar" antes de clickear).
                        cursorWorld = snapToVertexOrGrid(cursorWorld,
                                                          lockedOp->camera(),
                                                          oA);
                        const glm::vec3 rubberColor(108.0f / 255.0f,
                                                      193.0f / 255.0f,
                                                      229.0f / 255.0f);
                        dbgR.drawLine(m_polyDraw.pointsWorld.back(),
                                       cursorWorld, rubberColor);
                        dbgR.drawAabb(AABB{cursorWorld - he, cursorWorld + he},
                                       rubberColor);
                    }
                }
            }
            // F2H30 Bloque B: re-encolar markers de vertex/edge del
            // brush active si subMode == Vertex/Edge. Sin esto el dev
            // ve los markers solo en perspectiva pero NO en los ortos
            // (donde justamente arrastra para editar).
            if ((m_subMode == EditorSubMode::Vertex ||
                 m_subMode == EditorSubMode::Edge) && m_sceneRenderer) {
                const SelectionSet& set = m_ui.selectionSet();
                if (set.active &&
                    set.active.hasComponent<BrushComponent>() &&
                    set.active.hasComponent<TransformComponent>()) {
                    const auto& bc = set.active.getComponent<BrushComponent>();
                    const auto& tf = set.active.getComponent<TransformComponent>();
                    const glm::mat4 wm = tf.worldMatrix();
                    const auto verts = Csg::enumerateBrushVertices(bc.brush);
                    const f32 snap = static_cast<f32>(m_hammerSnapStep);
                    const glm::vec3 markerColor(1.0f, 1.0f, 1.0f);
                    auto& dbgR = m_sceneRenderer->debugRenderer();
                    if (m_subMode == EditorSubMode::Vertex) {
                        const f32 half = std::max(snap * 0.05f, 0.05f);
                        const glm::vec3 he(half);
                        for (const auto& v : verts) {
                            const glm::vec3 wp = glm::vec3(
                                wm * glm::vec4(v.localPos, 1.0f));
                            dbgR.drawAabb(AABB{wp - he, wp + he},
                                            markerColor);
                        }
                    } else {
                        const usize nf = bc.brush.faces.size();
                        for (u32 fi = 0; fi < static_cast<u32>(nf); ++fi) {
                            for (u32 fj = fi + 1; fj < static_cast<u32>(nf); ++fj) {
                                int idxA = -1, idxB = -1;
                                for (usize v = 0; v < verts.size(); ++v) {
                                    bool hasA = false, hasB = false;
                                    for (u32 p : verts[v].planeIndices) {
                                        if (p == fi) hasA = true;
                                        if (p == fj) hasB = true;
                                    }
                                    if (!hasA || !hasB) continue;
                                    if (idxA < 0) idxA = static_cast<int>(v);
                                    else if (idxB < 0) idxB = static_cast<int>(v);
                                    else { idxA = -1; break; }
                                }
                                if (idxA < 0 || idxB < 0) continue;
                                const glm::vec3 a = glm::vec3(
                                    wm * glm::vec4(verts[static_cast<usize>(idxA)].localPos, 1.0f));
                                const glm::vec3 b = glm::vec3(
                                    wm * glm::vec4(verts[static_cast<usize>(idxB)].localPos, 1.0f));
                                dbgR.drawLine(a, b, markerColor);
                            }
                        }
                    }
                }
            }
            m_sceneRenderer->renderOrthoView(*m_scene, *m_assetManager,
                                              oView, oProj,
                                              op->camera().panOffset,
                                              op->camera().worldHeight,
                                              snapStep,
                                              oW, oH, i, selected);
            // Conectar el FBO al panel ANTES del proximo onImGuiRender +
            // sincronizar el snap para el label "Grid: Nu" arriba-derecha.
            op->setFramebuffer(m_sceneRenderer->orthoFb(i));
            op->setSnapStep(m_hammerSnapStep);
            // F2H35 Bloque E: scene + flag de labels para que el panel
            // dibuje los labels point entity como overlay 2D.
            op->setScene(m_scene.get());
            op->setShowEntityLabels(m_ui.showEntityLabels());
        }
    }
}


// NOTE: drawEditorScene3DOverlay vive en `EditorRenderPass_Overlay.cpp`
// (extraido en AUDIT-2, 2026-05-17).

} // namespace Mood
