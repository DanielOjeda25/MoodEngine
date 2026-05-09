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
    if (activeWs.name == "Editor de mapas") {
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

    // F2H13: outline 3D de TODAS las entidades del SelectionSet.
    // Color naranja para la `active`, gris para las demas selected.
    // Para entidades sin mesh / brush (Light / Audio) no se dibuja —
    // el icono 2D ya las marca con halo cyan en el overlay 2D.
    if (m_scene && m_mode == EditorMode::Editor) {
        constexpr int kEdges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}};
        const glm::vec3 activeColor(1.00f, 0.35f, 0.00f); // naranja Blender saturado
        const glm::vec3 selColor   (0.95f, 0.95f, 0.20f); // amarillo claro (resalta sobre cielo gris-azul)

        const SelectionSet& set = m_ui.selectionSet();
        // F2H17 + F2H33: en Face Mode con cara(s) seleccionada(s),
        // NO dibujamos el outline del brush activo (caeria encima
        // del highlight de cara y los pixeles se superponen
        // perdiendo claridad). Solo se dibujan los outlines de las
        // caras seleccionadas.
        const bool faceModeWithSelectedFace =
            (m_subMode == EditorSubMode::Face) &&
            static_cast<bool>(set.active) &&
            !set.selectedFaceIndices.empty();

        for (const Entity& sel : set.selected) {
            if (!sel || !sel.hasComponent<TransformComponent>()) continue;
            const bool isActive = static_cast<bool>(set.active)
                && set.active.handle() == sel.handle();
            // Skip el outline del brush active cuando hay cara seleccionada
            // en Face Mode.
            if (isActive && faceModeWithSelectedFace) continue;
            const auto& tf = sel.getComponent<TransformComponent>();
            const glm::mat4 model = tf.worldMatrix();

            // Corners locales: para mesh = cubo unitario (compromise
            // historico, no usa el AABB real del MeshAsset);
            // para brush = AABB local del brush (preciso, F2H11
            // ya lo computa).
            glm::vec3 localMin(-0.5f), localMax(0.5f);
            if (sel.hasComponent<BrushComponent>()) {
                const auto& bc = sel.getComponent<BrushComponent>();
                if (bc.brush.localAabb.isValid()) {
                    localMin = bc.brush.localAabb.min;
                    localMax = bc.brush.localAabb.max;
                }
            } else if (!sel.hasComponent<MeshRendererComponent>()) {
                continue;  // sin mesh ni brush -> no outline
            }

            const glm::vec3 corners[8] = {
                {localMin.x, localMin.y, localMin.z},
                {localMax.x, localMin.y, localMin.z},
                {localMax.x, localMax.y, localMin.z},
                {localMin.x, localMax.y, localMin.z},
                {localMin.x, localMin.y, localMax.z},
                {localMax.x, localMin.y, localMax.z},
                {localMax.x, localMax.y, localMax.z},
                {localMin.x, localMax.y, localMax.z}};
            glm::vec3 w[8];
            for (int i = 0; i < 8; ++i) {
                const glm::vec4 p = model * glm::vec4(corners[i], 1.0f);
                w[i] = glm::vec3(p);
            }

            const glm::vec3& color = isActive ? activeColor : selColor;
            for (const auto& e : kEdges) {
                dbg.drawLine(w[e[0]], w[e[1]], color);
            }
        }

        // F2H17 + F2H33: highlight de cara(s) seleccionada(s) en
        // Face Mode estilo Blender — capa SEMI-TRANSPARENTE encima
        // de cada cara + outline brillante alrededor.
        //   - Caras "secundarias" (todas menos la ultima clickeada):
        //     naranja (~Half-Life).
        //   - Cara "active" (back de selectedFaceIndices, = primary
        //     para single-face ops): amarilla, mas brillante. Asi el
        //     dev distingue cual es la primaria cuando hay multi-
        //     seleccion via Shift+click.
        if (m_subMode == EditorSubMode::Face &&
            static_cast<bool>(set.active) &&
            set.active.hasComponent<BrushComponent>() &&
            set.active.hasComponent<TransformComponent>() &&
            !set.selectedFaceIndices.empty()) {
            const auto& bc = set.active.getComponent<BrushComponent>();
            const auto& tf = set.active.getComponent<TransformComponent>();
            const i32 activeIdx = set.activeFaceIndex();
            // F2H17: la capa rellena solo se dibuja cuando el dev
            // NO esta editando UV params — durante un drag de slider
            // la capa tapa la textura. Outline siempre.
            const bool editingUV = m_ui.inspector().isEditingBrushUV();
            const glm::mat4 worldMat = tf.worldMatrix();
            for (i32 faceIdxSigned : set.selectedFaceIndices) {
                if (faceIdxSigned < 0) continue;
                const u32 faceIdx = static_cast<u32>(faceIdxSigned);
                if (faceIdx >= bc.brush.faces.size()) continue;
                const bool isActive = (faceIdxSigned == activeIdx);
                // F2H33: amarillo (active/primary) vs naranja (otras).
                const glm::vec3 outlineColor = isActive
                    ? glm::vec3(1.00f, 0.95f, 0.10f)
                    : glm::vec3(1.00f, 0.50f, 0.00f);
                const glm::vec4 fillColor = isActive
                    ? glm::vec4(1.00f, 0.95f, 0.10f, 0.50f)
                    : glm::vec4(1.00f, 0.55f, 0.10f, 0.40f);
                const auto poly = Csg::collectFaceWorldPolygon(
                    bc.brush, faceIdx, worldMat);
                const usize n = poly.size();
                if (n < 3) continue;
                if (!editingUV) {
                    glm::vec3 centroid(0.0f);
                    for (const auto& v : poly) centroid += v;
                    centroid /= static_cast<f32>(n);
                    for (usize i = 0; i < n; ++i) {
                        dbg.drawTriangle(centroid, poly[i],
                                          poly[(i + 1) % n], fillColor);
                    }
                }
                for (usize i = 0; i < n; ++i) {
                    dbg.drawLine(poly[i], poly[(i + 1) % n], outlineColor);
                }
            }
        }

        // F2H35 Bloque F: preview hover en Face Mode. Cada frame, si el
        // cursor esta sobre el viewport perspectivo, calcular pickFace
        // contra el brush hovered y dibujar el outline de esa cara en
        // blanco tenue. El dev VE que cara va a seleccionar antes de
        // clickear — resuelve la queja "no se que cara voy a pickear
        // hasta que clickeo, requiere clicks ciegos hasta acertar".
        // Skip si la cara ya esta seleccionada (sin valor agregado y
        // su outline brillante ya gana la atencion).
        if (m_subMode == EditorSubMode::Face &&
            m_mode == EditorMode::Editor &&
            m_ui.viewport().imageHovered() &&
            m_scene && m_assetManager) {
            const float ndcX = m_ui.viewport().mouseNdcX();
            const float ndcY = m_ui.viewport().mouseNdcY();
            ScenePickResult hoverHit = pickEntity(*m_scene, view,
                projection, glm::vec2(ndcX, ndcY), m_assetManager.get());
            if (hoverHit && hoverHit.entity.hasComponent<BrushComponent>() &&
                hoverHit.entity.hasComponent<TransformComponent>()) {
                const auto& bc =
                    hoverHit.entity.getComponent<BrushComponent>();
                const auto& tf =
                    hoverHit.entity.getComponent<TransformComponent>();
                // Reconstruir el rayo desde la cam (mismo flow del
                // click handler).
                const glm::mat4 invVP = glm::inverse(projection * view);
                const glm::vec4 nearH = invVP *
                    glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                const glm::vec4 farH = invVP *
                    glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                if (nearH.w != 0.0f && farH.w != 0.0f) {
                    const glm::vec3 origin = glm::vec3(nearH) / nearH.w;
                    const glm::vec3 dir = glm::normalize(
                        glm::vec3(farH) / farH.w - origin);
                    const auto faceHit = Csg::pickFace(
                        bc.brush, origin, dir, tf.worldMatrix());
                    if (faceHit.has_value()) {
                        const i32 idx = static_cast<i32>(*faceHit);
                        // Skip si la cara hovered ya esta en el set
                        // (su highlight brillante ya gana atencion).
                        const bool alreadySelected =
                            (set.active.handle() ==
                             hoverHit.entity.handle()) &&
                            std::find(set.selectedFaceIndices.begin(),
                                      set.selectedFaceIndices.end(),
                                      idx) != set.selectedFaceIndices.end();
                        if (!alreadySelected) {
                            const auto poly = Csg::collectFaceWorldPolygon(
                                bc.brush, static_cast<u32>(idx),
                                tf.worldMatrix());
                            const usize n = poly.size();
                            if (n >= 3) {
                                // F2H35 fix: cyan saturado para que se
                                // vea sobre cualquier textura (incluido
                                // fondos blancos/claros donde el blanco
                                // tenue inicial desaparecia). Contrasta
                                // con amarillo (active) y naranja
                                // (secondary selected) — el dev distingue
                                // hover-preview vs ya-seleccionado.
                                const glm::vec3 hoverColor(
                                    0.10f, 0.95f, 1.00f);
                                for (usize i = 0; i < n; ++i) {
                                    dbg.drawLine(poly[i],
                                                  poly[(i + 1) % n],
                                                  hoverColor);
                                }
                            }
                        }
                    }
                }
            }
        }

        // F2H30 Bloque C: preview del pincel poligonal — lineas entre
        // vertices agregados + markers blancos en cada uno + RUBBER
        // BAND del ultimo vertex al cursor live (snappeado). El user
        // ve el polígono moviendose mientras decide donde clickear.
        if (m_polyDraw.active && m_polyDraw.pointsWorld.size() > 0) {
            const glm::vec3 polyColor(1.0f, 1.0f, 1.0f);   // blanco
            const f32 snap = static_cast<f32>(m_hammerSnapStep);
            const f32 markerHalf = std::max(snap * 0.05f, 0.05f);
            const glm::vec3 he(markerHalf);
            for (usize i = 0; i < m_polyDraw.pointsWorld.size(); ++i) {
                const glm::vec3& p = m_polyDraw.pointsWorld[i];
                dbg.drawAabb(AABB{p - he, p + he}, polyColor);
                if (i + 1 < m_polyDraw.pointsWorld.size()) {
                    dbg.drawLine(p, m_polyDraw.pointsWorld[i + 1], polyColor);
                }
            }
            // Linea de cierre (preview del cierre cuando >= 3 puntos):
            // dibuja en otro tono para que el dev vea que falta cerrar.
            if (m_polyDraw.pointsWorld.size() >= 3) {
                const glm::vec3 closeColor(0.5f, 0.5f, 0.5f); // gris
                dbg.drawLine(m_polyDraw.pointsWorld.back(),
                              m_polyDraw.pointsWorld.front(), closeColor);
            }
            // Rubber band: linea celeste del ultimo vertex al cursor
            // live de la orto lockeada. Solo si hay locked ortho Y
            // cursor hovered.
            if (m_polyDraw.orthoIdx >= 0) {
                OrthoViewportPanel* ops[3] = {
                    &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
                };
                OrthoViewportPanel* op = ops[m_polyDraw.orthoIdx];
                const auto& lc = op->liveCursor();
                if (lc.hovered && op->desiredHeight() > 0) {
                    const f32 oA = static_cast<f32>(op->desiredWidth())
                                  / static_cast<f32>(op->desiredHeight());
                    glm::vec3 cursorWorld = op->camera().worldFromNdc(
                        lc.ndcX, lc.ndcY, oA);
                    // F2H31 Bloque C: rubber band perspectivo tambien
                    // sigue al snap-to-vertex.
                    cursorWorld = snapToVertexOrGrid(cursorWorld,
                                                      op->camera(), oA);
                    const glm::vec3 rubberColor(108.0f / 255.0f,
                                                  193.0f / 255.0f,
                                                  229.0f / 255.0f); // celeste GMod
                    dbg.drawLine(m_polyDraw.pointsWorld.back(),
                                  cursorWorld, rubberColor);
                    // Marker fantasma en el cursor (para que se vea
                    // donde caeria el proximo vertex).
                    dbg.drawAabb(AABB{cursorWorld - he, cursorWorld + he},
                                  rubberColor);
                }
            }
        }

        // F2H30 Bloque B: visualizacion de vertices/edges del brush
        // active en sub-mode Vertex/Edge (estilo Blender). Sin esto el
        // dev no sabe DONDE clickear para mover. Markers en blanco
        // (visibles sobre cualquier fondo); tamano proporcional al
        // snapStep para que escale con el zoom del workspace.
        if ((m_subMode == EditorSubMode::Vertex ||
             m_subMode == EditorSubMode::Edge) &&
            static_cast<bool>(set.active) &&
            set.active.hasComponent<BrushComponent>() &&
            set.active.hasComponent<TransformComponent>()) {
            const auto& bc = set.active.getComponent<BrushComponent>();
            const auto& tf = set.active.getComponent<TransformComponent>();
            const glm::mat4 wm = tf.worldMatrix();
            const auto verts = Csg::enumerateBrushVertices(bc.brush);
            const f32 snap = static_cast<f32>(m_hammerSnapStep);
            const glm::vec3 markerColor(1.0f, 1.0f, 1.0f);  // blanco

            if (m_subMode == EditorSubMode::Vertex) {
                // Markers de vertex: pequenos cubos en cada esquina.
                const f32 half = std::max(snap * 0.05f, 0.05f);
                const glm::vec3 he(half);
                for (const auto& v : verts) {
                    const glm::vec3 wp = glm::vec3(
                        wm * glm::vec4(v.localPos, 1.0f));
                    dbg.drawAabb(AABB{wp - he, wp + he}, markerColor);
                }
            } else {
                // Edge mode: lineas brillantes en cada arista (par de
                // caras que comparten exactamente 2 vertices).
                const usize n = bc.brush.faces.size();
                for (u32 i = 0; i < static_cast<u32>(n); ++i) {
                    for (u32 j = i + 1; j < static_cast<u32>(n); ++j) {
                        int idxA = -1, idxB = -1;
                        for (usize v = 0; v < verts.size(); ++v) {
                            bool hasA = false, hasB = false;
                            for (u32 p : verts[v].planeIndices) {
                                if (p == i) hasA = true;
                                if (p == j) hasB = true;
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
                        dbg.drawLine(a, b, markerColor);
                    }
                }
            }
        }
    }
}

} // namespace Mood
