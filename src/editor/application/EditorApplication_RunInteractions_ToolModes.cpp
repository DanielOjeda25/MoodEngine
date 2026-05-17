// EditorApplication::processOrthoToolModes — modos de herramienta de
// los viewports ortograficos: block tool, marquee select, vertex/edge
// edit.
//
// Extraido de `EditorApplication_RunInteractions.cpp` en AUDIT-2
// (2026-05-17) para sub-splittear ese archivo (1065 LOC, sobre hard cap).
// Boundary natural: las secciones 2.4d/2.4f/2.4e operan cuando el dev
// tiene activo un tool de edicion (CreateBlock / Select-marquee /
// Vertex-Edge edit), mientras que las anteriores (2.4 / 2.4a-poly /
// 2.4a-clip / 2.4b / 2.4c) son operaciones de pick/select/drag estandar.
//
// Llamado desde `processViewportInteractions()` tras las secciones
// pick/select/drag.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "core/math/Plane.h"
#include "editor/commands/EditBrushGeometryCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/panels/scene/OrthoViewportPanel.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/world/csg/Brush.h"

#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <cmath>

namespace Mood {

void EditorApplication::processOrthoToolModes() {
        // 2.4d) F2H29 Bloque C: block tool en orto. Activa cuando el
        //       drag arranca en empty space (sin brush bajo el cursor).
        //       Durante el drag dibuja un AABB cyan via debugRenderer
        //       (visible en perspectiva 3D); al soltar materializa un
        //       Box brush si las dims superan `m_hammerSnapStep` en
        //       ambos ejes del view. Snap aplica a las ESQUINAS (no al
        //       delta) -> brush queda alineado al grid. La altura sobre
        //       el eje perpendicular es default `snap * 4`.
        //       F2H30 Bloque B: solo aplica si subMode == Object — en
        //       Vertex/Edge mode no se crean brushes nuevos al drag.
        //       F2H30 Bloque C: tambien skipea si pincel poligonal
        //       activo (el block tool y el pincel comparten el LMB
        //       en empty space — el pincel tiene prioridad).
        //       F2H31 Bloque B: solo si m_mapTool == CreateBlock.
        if (m_mode == EditorMode::Editor && m_scene &&
            m_subMode == EditorSubMode::Object &&
            !m_polyDraw.active &&
            m_mapTool == MapTool::CreateBlock &&
            m_orthoBlockSession.active) {
            OrthoViewportPanel* blockOrthoPanels[3] = {
                &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
            };
            OrthoViewportPanel* op =
                blockOrthoPanels[m_orthoBlockSession.orthoIdx];
            const auto ds = op->consumeDragEnded();
            const u32 oW = op->desiredWidth();
            const u32 oH = op->desiredHeight();
            if (oW > 0 && oH > 0) {
                const f32 oAspect = static_cast<f32>(oW) /
                                     static_cast<f32>(oH);
                const auto& cam = op->camera();
                const glm::vec3 ws = cam.worldFromNdc(
                    ds.ndcStartX, ds.ndcStartY, oAspect);
                const glm::vec3 wc = cam.worldFromNdc(
                    ds.ndcCurX, ds.ndcCurY, oAspect);
                // F2H31 Bloque C: snap-to-vertex toma prioridad si el
                // toggle esta on; sino fallback al grid. Snap se aplica
                // a las ESQUINAS (no al delta) para que el brush quede
                // alineado al grid global o pegado al vertex vecino.
                const f32 snap = static_cast<f32>(m_hammerSnapStep);
                const glm::vec3 wsSnap = snapToVertexOrGrid(ws, cam, oAspect);
                const glm::vec3 wcSnap = snapToVertexOrGrid(wc, cam, oAspect);
                glm::vec3 minPt = glm::min(wsSnap, wcSnap);
                glm::vec3 maxPt = glm::max(wsSnap, wcSnap);
                // Inflar el eje perpendicular del view a `snap * 4`
                // centrado en 0. forwardAxis tiene magnitud 1 en
                // exactamente uno de los 3 ejes, los otros 0.
                const f32 defaultHeight = (snap > 0.0f) ? (snap * 4.0f)
                                                          : 4.0f;
                const glm::vec3 perpHalf = glm::abs(cam.forwardAxis())
                                            * (defaultHeight * 0.5f);
                minPt -= perpHalf;
                maxPt += perpHalf;
                // Preview AABB cyan: lo encolamos en perspectiva
                // (este drawAabb se flushea con view+proj de la cam
                // 3D en endFrame) Y guardamos los puntos en la sesion
                // para que EditorRenderPass los re-encole antes de
                // cada renderOrthoView (el flush adentro de
                // renderOrthoView lo dibuja con las matrices del orto).
                m_orthoBlockSession.previewMin = minPt;
                m_orthoBlockSession.previewMax = maxPt;
                m_orthoBlockSession.previewValid = true;
                if (m_sceneRenderer) {
                    // F2H29 Bloque C: celeste GMod (mismo color del
                    // wireframe regular en SceneRenderer_Ortho.cpp).
                    // Coherente con la paleta Valve+GMod del workspace.
                    const glm::vec3 gmodCelesteRGB(108.0f / 255.0f,
                                                    193.0f / 255.0f,
                                                    229.0f / 255.0f);
                    m_sceneRenderer->debugRenderer().drawAabb(
                        AABB{minPt, maxPt}, gmodCelesteRGB);
                }
                if (ds.justEnded) {
                    const glm::vec3 dims = maxPt - minPt;
                    // Validar tamano minimo en los 2 ejes del plano de
                    // view (ignorar eje perpendicular que ya tiene
                    // height default).
                    const f32 widthOnRight =
                        std::abs(glm::dot(dims, cam.rightAxis()));
                    const f32 widthOnUp =
                        std::abs(glm::dot(dims, cam.upAxis()));
                    const f32 minSize = (snap > 0.0f) ? snap : 1.0f;
                    if (widthOnRight >= minSize &&
                        widthOnUp >= minSize) {
                        const glm::vec3 center = 0.5f * (minPt + maxPt);
                        glm::mat4 transform =
                            glm::translate(glm::mat4(1.0f), center);
                        transform = glm::scale(transform, dims);
                        spawnBoxBrushAt(transform);
                        Log::editor()->info(
                            "[ortho:{}] block tool END: spawn box "
                            "dims=({:.1f}, {:.1f}, {:.1f}) "
                            "center=({:.1f}, {:.1f}, {:.1f})",
                            cam.label(),
                            dims.x, dims.y, dims.z,
                            center.x, center.y, center.z);
                    } else {
                        Log::editor()->info(
                            "[ortho:{}] block tool END: dims too small "
                            "({:.1f} x {:.1f} < snap {:.1f}), no spawn",
                            cam.label(),
                            widthOnRight, widthOnUp, minSize);
                    }
                    m_orthoBlockSession.active = false;
                    m_orthoBlockSession.previewValid = false;
                }
            } else {
                // Panel sin tamano valido (workspace cambio mid-drag?).
                m_orthoBlockSession.active = false;
                m_orthoBlockSession.previewValid = false;
            }
        }

        // 2.4f) F2H31 Bloque B: marquee select en orto. Activa cuando el
        //       drag arranca en empty space + m_mapTool == Select. El
        //       rectangulo amarillo se dibuja desde EditorRenderPass
        //       leyendo `m_orthoMarquee.active` + el panel's dragState.
        //       Aca solo procesamos el cierre (justEnded): hit-test cada
        //       entidad con AABB world contra el rectangulo en ndc del
        //       orto; replace / Shift=add / Ctrl=toggle del SelectionSet.
        if (m_mode == EditorMode::Editor && m_scene &&
            m_orthoMarquee.active &&
            m_mapTool == MapTool::Select) {
            OrthoViewportPanel* marqOrthoPanels[3] = {
                &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
            };
            OrthoViewportPanel* op =
                marqOrthoPanels[m_orthoMarquee.orthoIdx];
            const auto ds = op->consumeDragEnded();
            if (ds.justEnded) {
                const u32 oW = op->desiredWidth();
                const u32 oH = op->desiredHeight();
                if (oW > 0 && oH > 0) {
                    const f32 oAspect = static_cast<f32>(oW) /
                                         static_cast<f32>(oH);
                    const auto& cam = op->camera();
                    const glm::mat4 mView = cam.viewMatrix();
                    const glm::mat4 mProj = cam.projMatrix(oAspect);
                    const glm::mat4 mVP = mProj * mView;
                    const f32 ndcMinX = std::min(ds.ndcStartX, ds.ndcCurX);
                    const f32 ndcMaxX = std::max(ds.ndcStartX, ds.ndcCurX);
                    const f32 ndcMinY = std::min(ds.ndcStartY, ds.ndcCurY);
                    const f32 ndcMaxY = std::max(ds.ndcStartY, ds.ndcCurY);
                    const Uint8* keys = SDL_GetKeyboardState(nullptr);
                    const bool keyShift = keys[SDL_SCANCODE_LSHIFT] ||
                                            keys[SDL_SCANCODE_RSHIFT];
                    const bool keyCtrl  = keys[SDL_SCANCODE_LCTRL]  ||
                                            keys[SDL_SCANCODE_RCTRL];

                    SelectionSet& set = m_ui.selectionSet();
                    if (!keyShift && !keyCtrl) {
                        clear(set);
                    }

                    int hitCount = 0;
                    auto hitTestEntity = [&](Entity e, const AABB& aabb) {
                        const glm::vec3 corners[8] = {
                            {aabb.min.x, aabb.min.y, aabb.min.z},
                            {aabb.max.x, aabb.min.y, aabb.min.z},
                            {aabb.min.x, aabb.max.y, aabb.min.z},
                            {aabb.max.x, aabb.max.y, aabb.min.z},
                            {aabb.min.x, aabb.min.y, aabb.max.z},
                            {aabb.max.x, aabb.min.y, aabb.max.z},
                            {aabb.min.x, aabb.max.y, aabb.max.z},
                            {aabb.max.x, aabb.max.y, aabb.max.z},
                        };
                        for (int c = 0; c < 8; ++c) {
                            const glm::vec4 clip = mVP *
                                glm::vec4(corners[c], 1.0f);
                            if (std::abs(clip.w) < 1e-6f) continue;
                            const f32 ndcX = clip.x / clip.w;
                            const f32 ndcY = clip.y / clip.w;
                            if (ndcX >= ndcMinX && ndcX <= ndcMaxX &&
                                ndcY >= ndcMinY && ndcY <= ndcMaxY) {
                                if (keyCtrl) {
                                    toggle(set, e);
                                } else {
                                    add(set, e);
                                }
                                ++hitCount;
                                return;
                            }
                        }
                    };

                    m_scene->forEach<TransformComponent>(
                        [&](Entity e, TransformComponent& t) {
                            if (e.hasComponent<BrushComponent>()) {
                                const auto& bc =
                                    e.getComponent<BrushComponent>();
                                hitTestEntity(e, brushAabbWorld(t, bc));
                            } else if (e.hasComponent<MeshRendererComponent>()) {
                                const auto& mr =
                                    e.getComponent<MeshRendererComponent>();
                                hitTestEntity(e, meshAabbWorld(
                                    t, &mr, m_assetManager.get()));
                            }
                        });

                    Log::editor()->info(
                        "[ortho:{}] marquee END ({}{}{}): {} entidades hit",
                        cam.label(),
                        keyShift ? "+Shift" : "",
                        keyCtrl  ? "+Ctrl"  : "",
                        keyShift || keyCtrl ? "" : "replace",
                        hitCount);
                }
                m_orthoMarquee.active = false;
                m_orthoMarquee.orthoIdx = -1;
            }
        }

        // 2.4e) F2H30 Bloque B: vertex/edge edit en orto. Activa cuando
        //       subMode == Vertex/Edge AND drag-down impacta un vertex
        //       o edge del brush ACTIVE (single brush — solo opera sobre
        //       activeSelection.active, no multi-edit). Mueve los planos
        //       incidentes via `d_new = d_old - dot(n_local, delta_local)`
        //       con `delta_local = R^-1 * delta_world`. Valida
        //       `isBrushValid` post; si rompe, revierte. Al soltar,
        //       pushea EditBrushGeometryCommand.
        //       F2H30 Bloque C fix: skipea si el pincel poligonal esta
        //       activo (togglePolygonDrawMode ya pone subMode=Object,
        //       pero la guarda extra previene que un session in-flight
        //       siga corriendo si el pincel se activo a mitad de drag).
        if (m_mode == EditorMode::Editor && m_scene &&
            !m_polyDraw.active &&
            (m_subMode == EditorSubMode::Vertex ||
             m_subMode == EditorSubMode::Edge)) {
            OrthoViewportPanel* vertOrthoPanels[3] = {
                &m_ui.orthoTop(),
                &m_ui.orthoFront(),
                &m_ui.orthoSide(),
            };
            // 1) Si NO hay sesion activa, ver si alguno arranco un drag.
            if (!m_orthoVertexEdit.active) {
                Entity activeBrush = m_ui.selectionSet().active;
                if (activeBrush && activeBrush.hasComponent<BrushComponent>()
                    && activeBrush.hasComponent<TransformComponent>()) {
                    for (int i = 0; i < 3; ++i) {
                        const auto& ds = vertOrthoPanels[i]->dragState();
                        if (!ds.active) continue;
                        const u32 oW = vertOrthoPanels[i]->desiredWidth();
                        const u32 oH = vertOrthoPanels[i]->desiredHeight();
                        if (oW == 0 || oH == 0) continue;
                        const f32 oAspect = static_cast<f32>(oW) /
                                             static_cast<f32>(oH);
                        const auto& cam = vertOrthoPanels[i]->camera();
                        const glm::mat4 oView = cam.viewMatrix();
                        const glm::mat4 oProj = cam.projMatrix(oAspect);
                        auto& tf = activeBrush.getComponent<TransformComponent>();
                        const glm::mat4 worldMat = tf.worldMatrix();
                        auto& bc = activeBrush.getComponent<BrushComponent>();
                        std::vector<u32> incident;
                        if (m_subMode == EditorSubMode::Vertex) {
                            auto vp = Csg::pickVertex(bc.brush, worldMat,
                                oView, oProj, ds.ndcStartX, ds.ndcStartY);
                            if (!vp) continue;
                            incident = vp->planeIndices;
                        } else { // Edge
                            auto ep = Csg::pickEdge(bc.brush, worldMat,
                                oView, oProj, ds.ndcStartX, ds.ndcStartY);
                            if (!ep) continue;
                            incident = {ep->planeA, ep->planeB};
                        }
                        // Hit. Capturar snapshot de planos + transform
                        // + pivot local del vertex/edge.
                        m_orthoVertexEdit.active = true;
                        m_orthoVertexEdit.orthoIdx = i;
                        m_orthoVertexEdit.brush = activeBrush;
                        m_orthoVertexEdit.brushTag =
                            activeBrush.getComponent<TagComponent>().name;
                        m_orthoVertexEdit.planesBefore.clear();
                        m_orthoVertexEdit.planesBefore.reserve(
                            bc.brush.faces.size());
                        for (const auto& f : bc.brush.faces) {
                            m_orthoVertexEdit.planesBefore.push_back(f.plane);
                        }
                        m_orthoVertexEdit.incidentPlanes =
                            std::move(incident);
                        m_orthoVertexEdit.tfPosBefore = tf.position;
                        // Recompute pivot local (vertex pos para Vertex,
                        // midpoint para Edge). Lo recomputamos del pick
                        // result en lugar de cachearlo arriba para
                        // mantener el codigo simple.
                        if (m_subMode == EditorSubMode::Vertex) {
                            auto vp = Csg::pickVertex(bc.brush, worldMat,
                                oView, oProj, ds.ndcStartX, ds.ndcStartY);
                            m_orthoVertexEdit.pivotLocalStart =
                                vp ? vp->localPos : glm::vec3(0.0f);
                        } else {
                            auto ep = Csg::pickEdge(bc.brush, worldMat,
                                oView, oProj, ds.ndcStartX, ds.ndcStartY);
                            m_orthoVertexEdit.pivotLocalStart =
                                ep ? 0.5f * (ep->localA + ep->localB)
                                   : glm::vec3(0.0f);
                        }
                        Log::editor()->info(
                            "[ortho:{}] {}-edit START on '{}' ({} incident planes)",
                            cam.label(),
                            m_subMode == EditorSubMode::Vertex ? "vertex" : "edge",
                            m_orthoVertexEdit.brushTag,
                            m_orthoVertexEdit.incidentPlanes.size());
                        break;
                    }
                }
            }
            // 2) Si hay sesion activa, aplicar delta o cerrar.
            if (m_orthoVertexEdit.active) {
                OrthoViewportPanel* op =
                    vertOrthoPanels[m_orthoVertexEdit.orthoIdx];
                const auto ds = op->consumeDragEnded();
                Entity brushEnt = m_orthoVertexEdit.brush;
                const u32 oW = op->desiredWidth();
                const u32 oH = op->desiredHeight();
                if (oW > 0 && oH > 0 && brushEnt &&
                    brushEnt.hasComponent<BrushComponent>() &&
                    brushEnt.hasComponent<TransformComponent>()) {
                    const f32 oAspect = static_cast<f32>(oW) /
                                         static_cast<f32>(oH);
                    const auto& cam = op->camera();
                    auto& tf = brushEnt.getComponent<TransformComponent>();
                    const glm::mat4 worldMat = tf.worldMatrix();
                    const glm::vec3 ws = cam.worldFromNdc(
                        ds.ndcStartX, ds.ndcStartY, oAspect);
                    const glm::vec3 wc = cam.worldFromNdc(
                        ds.ndcCurX, ds.ndcCurY, oAspect);
                    const glm::vec3 deltaWorld = wc - ws;
                    // F2H30 fix#1: snap se hace en WORLD space (no local).
                    // El grid del workspace orto vive en world, no en local
                    // — si el brush tiene tf.position != 0, snap en local
                    // queda desfasado. Pivot world = tf.position + R *
                    // pivotLocalStart. Snap world pivot. Reconvertir a
                    // delta_local via inverse(R).
                    //
                    // F2H30 fix#2: solo snap los ejes que el dev MUEVE
                    // (|deltaWorld[i]| > eps). Snap todos los ejes hace
                    // que coords no-grid-aligned salten a 0/16/etc al
                    // primer drag aunque el dev no las haya movido.
                    const f32 snap = static_cast<f32>(m_hammerSnapStep);
                    const glm::vec3 pivotWorldStart = glm::vec3(
                        worldMat *
                        glm::vec4(m_orthoVertexEdit.pivotLocalStart, 1.0f));
                    glm::vec3 pivotWorldNew = pivotWorldStart + deltaWorld;
                    if (snap > 0.0f) {
                        for (int i = 0; i < 3; ++i) {
                            if (std::abs(deltaWorld[i]) > 1e-4f) {
                                pivotWorldNew[i] =
                                    std::round(pivotWorldNew[i] / snap) * snap;
                            } else {
                                pivotWorldNew[i] = pivotWorldStart[i];
                            }
                        }
                    }
                    const glm::vec3 effDeltaWorld =
                        pivotWorldNew - pivotWorldStart;
                    const glm::mat3 rotInv =
                        glm::inverse(glm::mat3(worldMat));
                    const glm::vec3 effDeltaLocal = rotInv * effDeltaWorld;
                    const glm::vec3 pivotNew =
                        m_orthoVertexEdit.pivotLocalStart + effDeltaLocal;
                    // Aplicar: para cada plano incidente,
                    //   d_new = d_old_before - dot(n_local, effDeltaLocal).
                    // Reseteamos desde `before` para idempotencia (cada
                    // frame parte del estado pre-drag).
                    auto& bc = brushEnt.getComponent<BrushComponent>();
                    for (usize i = 0; i < bc.brush.faces.size(); ++i) {
                        bc.brush.faces[i].plane = m_orthoVertexEdit.planesBefore[i];
                    }
                    for (u32 pIdx : m_orthoVertexEdit.incidentPlanes) {
                        if (pIdx >= bc.brush.faces.size()) continue;
                        Plane& pl = bc.brush.faces[pIdx].plane;
                        pl.distance -= glm::dot(pl.normal, effDeltaLocal);
                    }
                    // Validar: si el AABB resulta degenerada, revertir.
                    const AABB newAabb = Csg::computeBrushAabb(bc.brush);
                    const bool valid = newAabb.isValid() &&
                        (newAabb.max.x - newAabb.min.x > kPlaneEpsilon) &&
                        (newAabb.max.y - newAabb.min.y > kPlaneEpsilon) &&
                        (newAabb.max.z - newAabb.min.z > kPlaneEpsilon);
                    if (!valid) {
                        // Revert.
                        for (usize i = 0; i < bc.brush.faces.size(); ++i) {
                            bc.brush.faces[i].plane = m_orthoVertexEdit.planesBefore[i];
                        }
                    } else {
                        bc.brush.localAabb = newAabb;
                    }
                    bc.dirty = true;
                    if (ds.justEnded) {
                        // F2H30 fix: REBASEAR brush al nuevo centroide
                        // (mismo patron que F2H12 snapshotResultWorld).
                        // Sin rebasing, el origen del entity (gizmo) queda
                        // en el centro VIEJO mientras la geometria se
                        // movio. Calculamos new centroid en local, lo
                        // restamos a cada plane.distance, y trasladamos
                        // tf.position por R * newCentroid (preserva la
                        // posicion world del mesh).
                        const glm::vec3 newCentroidLocal =
                            bc.brush.localAabb.center();
                        for (auto& f : bc.brush.faces) {
                            f.plane.distance += glm::dot(
                                f.plane.normal, newCentroidLocal);
                        }
                        bc.brush.localAabb = Csg::computeBrushAabb(bc.brush);
                        const glm::mat3 rotMat(worldMat);
                        const glm::vec3 newTfPos =
                            m_orthoVertexEdit.tfPosBefore
                            + rotMat * newCentroidLocal;
                        // Capturar `after` planes (post-rebase).
                        std::vector<Plane> afterPlanes;
                        afterPlanes.reserve(bc.brush.faces.size());
                        for (const auto& f : bc.brush.faces) {
                            afterPlanes.push_back(f.plane);
                        }
                        // Revert al before (planes + transform) antes del
                        // push (mismo patron que MultiEditTransformCommand
                        // drag-edit) — execute() re-aplica el after.
                        for (usize i = 0; i < bc.brush.faces.size(); ++i) {
                            bc.brush.faces[i].plane = m_orthoVertexEdit.planesBefore[i];
                        }
                        tf.position = m_orthoVertexEdit.tfPosBefore;
                        bc.brush.localAabb = Csg::computeBrushAabb(bc.brush);
                        auto cmd = std::make_unique<EditBrushGeometryCommand>(
                            m_scene.get(), m_orthoVertexEdit.brushTag,
                            m_orthoVertexEdit.planesBefore,
                            std::move(afterPlanes),
                            m_orthoVertexEdit.tfPosBefore,
                            newTfPos,
                            m_subMode == EditorSubMode::Vertex
                                ? std::string("Mover vertex de brush")
                                : std::string("Mover edge de brush"));
                        if (!cmd->isNoOp()) {
                            Log::editor()->info(
                                "[ortho:{}] {}-edit END push on '{}' [snap={}u]\n"
                                "  pivotLocalStart =({:.3f}, {:.3f}, {:.3f})\n"
                                "  pivotWorldStart =({:.3f}, {:.3f}, {:.3f})\n"
                                "  deltaWorld_raw  =({:.3f}, {:.3f}, {:.3f})\n"
                                "  pivotWorldNew   =({:.3f}, {:.3f}, {:.3f}) [snapped]\n"
                                "  effDeltaWorld   =({:.3f}, {:.3f}, {:.3f})\n"
                                "  effDeltaLocal   =({:.3f}, {:.3f}, {:.3f})\n"
                                "  newCentroidLocal=({:.3f}, {:.3f}, {:.3f})\n"
                                "  tfPosBefore     =({:.3f}, {:.3f}, {:.3f})\n"
                                "  tfPosAfter      =({:.3f}, {:.3f}, {:.3f})",
                                cam.label(),
                                m_subMode == EditorSubMode::Vertex ? "vertex" : "edge",
                                m_orthoVertexEdit.brushTag, m_hammerSnapStep,
                                m_orthoVertexEdit.pivotLocalStart.x,
                                m_orthoVertexEdit.pivotLocalStart.y,
                                m_orthoVertexEdit.pivotLocalStart.z,
                                pivotWorldStart.x, pivotWorldStart.y, pivotWorldStart.z,
                                deltaWorld.x, deltaWorld.y, deltaWorld.z,
                                pivotWorldNew.x, pivotWorldNew.y, pivotWorldNew.z,
                                effDeltaWorld.x, effDeltaWorld.y, effDeltaWorld.z,
                                effDeltaLocal.x, effDeltaLocal.y, effDeltaLocal.z,
                                newCentroidLocal.x, newCentroidLocal.y, newCentroidLocal.z,
                                m_orthoVertexEdit.tfPosBefore.x,
                                m_orthoVertexEdit.tfPosBefore.y,
                                m_orthoVertexEdit.tfPosBefore.z,
                                newTfPos.x, newTfPos.y, newTfPos.z);
                            m_history.push(std::move(cmd));
                        }
                        m_orthoVertexEdit.active = false;
                        m_orthoVertexEdit.brush = Entity{};
                        m_orthoVertexEdit.planesBefore.clear();
                        m_orthoVertexEdit.incidentPlanes.clear();
                    }
                } else {
                    // Brush invalido / panel sin tamano: cerrar sesion.
                    m_orthoVertexEdit.active = false;
                    m_orthoVertexEdit.brush = Entity{};
                    m_orthoVertexEdit.planesBefore.clear();
                    m_orthoVertexEdit.incidentPlanes.clear();
                }
            }
        }

}

} // namespace Mood
