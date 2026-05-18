// EditorApplication::processViewportInteractions — todas las interacciones
// del viewport: click-to-select (perspectiva + 3 orthos), face mode picking,
// polygon brush, clip tool, drag-edit en ortos, block tool, vertex/edge
// edit, marquee select.
//
// Extraido de `EditorApplication_Run.cpp` en AUDIT-1 (2026-05-17) para
// mantener `_Run.cpp` bajo el hard cap de 800 LOC. Antes esta seccion
// vivia inline en `run()` con ~1018 LOC. Esta llamada se ejecuta una
// vez por frame entre el handling de requests UI y el de drops.
//
// Sin parametros + sin estado nuevo: la implementacion usa solo m_*
// members y getters globales de ImGui/SDL. Mover el codigo es mecanico,
// no hay refactor de logica.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "core/math/Plane.h"
#include "core/math/Ray.h"  // AUDIT-3: pickRayFromNdc
#include "editor/commands/EditBrushGeometryCommand.h"
#include "editor/commands/EditTransformCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/MultiEditTransformCommand.h"
#include "editor/panels/scene/OrthoViewportPanel.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
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

void EditorApplication::processViewportInteractions() {
        // 2.4) Click-to-select (Hito 13 Bloque 2): raycast desde el cursor
        //      y selecciona la entidad mas cercana. Click en vacio deselecciona.
        //      Solo en Editor Mode — en Play Mode el mouse es para la camara.
        //      Si el gizmo se llevo el click este frame, descartar el select
        //      para que clickear sobre una flecha no mueva la seleccion.
        const ViewportPanel::ClickSelect click = m_ui.viewport().consumeClickSelect();
        const bool skipClickDueToGizmo = m_gizmoConsumedClick;
        m_gizmoConsumedClick = false;
        if (click.pending && !skipClickDueToGizmo &&
            m_mode == EditorMode::Editor && m_scene) {
            const float aspect = viewportAspect();
            const glm::mat4 view = m_editorCamera.viewMatrix();
            const glm::mat4 projection = m_editorCamera.projectionMatrix(aspect);

            // F2H17 + F2H33 fix UX: en Face Mode, picking robusto que
            // funciona sobre CUALQUIER brush hovered (no solo el active).
            // Issue pre-F2H33: el pickFace solo testeaba el brush active,
            // asi que clickear sobre OTRO brush fallaba el faceHit ->
            // caia al pickEntity -> cambiaba el active -> limpiaba las
            // caras. El dev tenia que clickear 2 veces para seleccionar
            // una cara de otro brush (y multi-select via Shift se
            // rompia por el cambio de active).
            //
            // Flow nuevo:
            //   1. pickEntity (siempre) -> brush bajo el rayo (el que sea).
            //   2. Si es brush -> pickFace contra ese brush.
            //   3. Si face hit:
            //      - sameBrush + Shift -> toggle.
            //      - sameBrush + sin modifier -> single.
            //      - distinto brush -> replace selection + single face
            //        (cambiar de brush no togglea, seria confuso).
            //   4. Sin brush bajo el rayo o sin face hit -> fallback al
            //      Object pick handler (cambia/limpia seleccion).
            bool faceModeHandled = false;
            ScenePickResult hit{};
            if (m_subMode == EditorSubMode::Face) {
                hit = pickEntity(*m_scene, view, projection,
                                  glm::vec2(click.ndcX, click.ndcY),
                                  m_assetManager.get());
                if (hit && hit.entity.hasComponent<BrushComponent>() &&
                    hit.entity.hasComponent<TransformComponent>()) {
                    auto& bc = hit.entity.getComponent<BrushComponent>();
                    auto& t = hit.entity.getComponent<TransformComponent>();
                    const glm::mat4 invVP =
                        glm::inverse(projection * view);
                    const auto ray = pickRayFromNdc(
                        invVP, click.ndcX, click.ndcY);
                    const auto faceHit = ray
                        ? Csg::pickFace(bc.brush, ray->origin,
                                        ray->direction, t.worldMatrix())
                        : std::optional<u32>{};
                    if (faceHit.has_value()) {
                        const Uint8* keys = SDL_GetKeyboardState(nullptr);
                        const bool keyShiftFace =
                            keys[SDL_SCANCODE_LSHIFT] ||
                            keys[SDL_SCANCODE_RSHIFT];
                        SelectionSet& selSet = m_ui.selectionSet();
                        const bool sameBrush =
                            static_cast<bool>(selSet.active) &&
                            selSet.active.handle() == hit.entity.handle();
                        const i32 idx = static_cast<i32>(*faceHit);

                        if (!sameBrush) {
                            // Cambio de brush: replace + single face.
                            // El replaceWithSingle limpia las caras del
                            // brush viejo (invariante del SelectionSet).
                            replaceWithSingle(selSet, hit.entity);
                            setSingleFace(selSet, idx);
                        } else if (keyShiftFace) {
                            toggleFace(selSet, idx);
                        } else {
                            setSingleFace(selSet, idx);
                        }
                        Log::editor()->info(
                            "Face Mode: face {} of brush '{}' "
                            "({} caras seleccionadas, active={})",
                            idx,
                            selSet.active.hasComponent<TagComponent>()
                                ? selSet.active.getComponent<TagComponent>().name
                                : std::string{"(sin tag)"},
                            selSet.selectedFaceIndices.size(),
                            selSet.activeFaceIndex());
                        faceModeHandled = true;
                    }
                    // Sin face hit pero brush sí: el rayo pego el AABB
                    // pero falló contra los planos (caso borde). Caemos
                    // al Object handler con el `hit` ya computado para
                    // que cambie/preserve la seleccion del brush.
                }
                // Sin brush bajo el rayo: idem fallback con hit vacio
                // -> Object handler aplica clear si no hay modifier.
            }

            if (m_subMode != EditorSubMode::Face && !faceModeHandled) {
                hit = pickEntity(*m_scene, view, projection,
                                  glm::vec2(click.ndcX, click.ndcY),
                                  m_assetManager.get());
            }
            // F2H13: aplicar Shift / Ctrl semantics igual que en Hierarchy.
            //   Plain click   -> replaceWithSingle.
            //   Shift+click   -> ADD (Maya-style, F2H23 polish iter 3).
            //   Ctrl+click    -> TOGGLE (Maya-style).
            // Click en vacio (no hit) sin modifier -> clear.
            //                  con modifier         -> no-op (preserva).
            // F2H17: si Face Mode capturo el click (faceModeHandled),
            // saltear toda la logica de seleccion de entidad.
            if (!faceModeHandled) {
                const Uint8* keys = SDL_GetKeyboardState(nullptr);
                const bool keyShift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
                const bool keyCtrl  = keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
                SelectionSet& set = m_ui.selectionSet();
                if (hit) {
                    const std::string entityName =
                        hit.entity.hasComponent<TagComponent>()
                            ? hit.entity.getComponent<TagComponent>().name
                            : std::string{"(sin tag)"};
                    if (keyShift) {
                        add(set, hit.entity);
                        Log::editor()->info(
                            "[viewport] Shift+click ADD '{}' (selected={})",
                            entityName, set.selected.size());
                    } else if (keyCtrl) {
                        toggle(set, hit.entity);
                        Log::editor()->info(
                            "[viewport] Ctrl+click TOGGLE '{}' (selected={})",
                            entityName, set.selected.size());
                    } else {
                        replaceWithSingle(set, hit.entity);
                        Log::editor()->info("[viewport] click replace '{}'",
                                              entityName);
                    }
                } else if (!keyShift && !keyCtrl) {
                    clear(set);
                }
            }
        }

        // 2.4a-poly) F2H30 Bloque C: pincel poligonal — consume clicks
        //            de las ortos ANTES del click-select. Cada click
        //            agrega un vertex snappeado al grid. Lock a 1 orto
        //            (la primera) para mantener coplanaridad. Despues
        //            del primer click, clicks en otras ortos se
        //            ignoran silenciosamente.
        if (m_mode == EditorMode::Editor && m_polyDraw.active) {
            OrthoViewportPanel* polyOrthoPanels[3] = {
                &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
            };
            for (int i = 0; i < 3; ++i) {
                // F2H30 Bloque C: en pincel mode, aceptar tanto click
                // puro como drag-end (el panel marca como drag si el
                // cursor se movio > 4 px entre down y up — sin esto, un
                // dev temblando levemente perderia el vertex). Tomamos
                // el primero que tenga datos.
                const auto click = polyOrthoPanels[i]->consumeClickSelect();
                const auto dragEnd = polyOrthoPanels[i]->consumeDragEnded();
                bool havePending = false;
                f32 ndcX = 0.0f, ndcY = 0.0f;
                if (click.pending) {
                    havePending = true;
                    ndcX = click.ndcX;
                    ndcY = click.ndcY;
                } else if (dragEnd.justEnded) {
                    havePending = true;
                    ndcX = dragEnd.ndcCurX;
                    ndcY = dragEnd.ndcCurY;
                }
                if (!havePending) continue;
                if (m_polyDraw.orthoIdx >= 0 && m_polyDraw.orthoIdx != i) {
                    // Ignorar — locked a otra orto.
                    continue;
                }
                const u32 oW = polyOrthoPanels[i]->desiredWidth();
                const u32 oH = polyOrthoPanels[i]->desiredHeight();
                if (oW == 0 || oH == 0) continue;
                const f32 oAspect = static_cast<f32>(oW) /
                                     static_cast<f32>(oH);
                const auto& cam = polyOrthoPanels[i]->camera();
                glm::vec3 worldPt = cam.worldFromNdc(
                    ndcX, ndcY, oAspect);
                // F2H31 Bloque C: snap-to-vertex toma prioridad si el
                // toggle esta on; sino fallback al grid (comportamiento
                // F2H30 Bloque C). Mismo helper que el block tool corners.
                worldPt = snapToVertexOrGrid(worldPt, cam, oAspect);
                if (m_polyDraw.orthoIdx < 0) {
                    // Primer click: lockear orto + setear axisIndex
                    // segun el eje perpendicular del view.
                    m_polyDraw.orthoIdx = i;
                    const glm::vec3 fwd = cam.forwardAxis();
                    if (std::abs(fwd.x) > 0.5f)      m_polyDraw.axisIndex = 0;
                    else if (std::abs(fwd.y) > 0.5f) m_polyDraw.axisIndex = 1;
                    else                              m_polyDraw.axisIndex = 2;
                }
                // F2H30 fix: dedupe contra el ultimo punto. Sin esto,
                // dos clicks que snapean a la misma celda generan un
                // poligono degenerado y `closePolygonDraw` lo rechaza
                // — el dev percibia "la figura desaparece" sin pista.
                if (!m_polyDraw.pointsWorld.empty()) {
                    const glm::vec3 last = m_polyDraw.pointsWorld.back();
                    if (glm::distance(last, worldPt) < 1e-3f) {
                        Log::editor()->info(
                            "[pincel] click duplicado (mismo grid cell) "
                            "— ignorado");
                        continue;
                    }
                }
                // F2H31 Bloque C fix: auto-close al clickear cerca del
                // primer vertex (con >= 3 puntos ya). Mental model del
                // dev: "click de vuelta en el inicio = cerrar" (estilo
                // editor de polígonos 2D clasico). Sin esto, clickear
                // sobre vertex 1 generaba vertex N == vertex 1 ->
                // polígono degenerado -> rechazo silencioso.
                if (m_polyDraw.pointsWorld.size() >= 3) {
                    const glm::vec3 first = m_polyDraw.pointsWorld.front();
                    if (glm::distance(first, worldPt) < 1e-3f) {
                        Log::editor()->info(
                            "[pincel] click sobre vertex 1 — cerrando "
                            "polígono ({} puntos)",
                            m_polyDraw.pointsWorld.size());
                        closePolygonDraw();
                        break; // sesion cerrada — salir del for ortos
                    }
                }
                m_polyDraw.pointsWorld.push_back(worldPt);
                Log::editor()->info(
                    "[pincel] vertex {} agregado: ({:.1f}, {:.1f}, {:.1f})",
                    m_polyDraw.pointsWorld.size(),
                    worldPt.x, worldPt.y, worldPt.z);
            }
            // Hint en status bar mientras el modo este activo.
            m_ui.setStatusMessage(
                "Pincel: click para vertex, Enter cierra, Esc cancela ["
                + std::to_string(m_polyDraw.pointsWorld.size()) + " pts]");
        }

        // 2.4a-clip) F2H32 Bloque B: clip tool — 2 clicks en orto
        //            definen una linea; el plano resultante es
        //            perpendicular al view plane (extendido sobre el
        //            forwardAxis del orto). Tecla T cycle keepMode;
        //            Enter confirma; Esc cancela.
        if (m_mode == EditorMode::Editor && m_mapTool == MapTool::Clip) {
            OrthoViewportPanel* clipOrthoPanels[3] = {
                &m_ui.orthoTop(), &m_ui.orthoFront(), &m_ui.orthoSide(),
            };
            for (int i = 0; i < 3; ++i) {
                const auto click = clipOrthoPanels[i]->consumeClickSelect();
                if (!click.pending) continue;
                if (m_clipTool.active && m_clipTool.orthoIdx >= 0
                    && m_clipTool.orthoIdx != i) {
                    // Lockeado a otra orto — ignorar para mantener
                    // coplanaridad del plano del clip con un view plane.
                    continue;
                }
                const u32 oW = clipOrthoPanels[i]->desiredWidth();
                const u32 oH = clipOrthoPanels[i]->desiredHeight();
                if (oW == 0 || oH == 0) continue;
                const f32 oAspect = static_cast<f32>(oW) /
                                     static_cast<f32>(oH);
                const auto& cam = clipOrthoPanels[i]->camera();
                glm::vec3 worldPt = cam.worldFromNdc(
                    click.ndcX, click.ndcY, oAspect);
                worldPt = snapToVertexOrGrid(worldPt, cam, oAspect);
                if (!m_clipTool.active) {
                    // Primera vez: arranca sesion.
                    m_clipTool.active = true;
                    m_clipTool.orthoIdx = i;
                    m_clipTool.hasP1 = false;
                    m_clipTool.hasP2 = false;
                    m_clipTool.keepMode = ClipKeepMode::Front;
                }
                if (!m_clipTool.hasP1) {
                    m_clipTool.p1World = worldPt;
                    m_clipTool.hasP1 = true;
                    Log::editor()->info(
                        "[clip] p1 = ({:.1f}, {:.1f}, {:.1f})",
                        worldPt.x, worldPt.y, worldPt.z);
                } else if (!m_clipTool.hasP2) {
                    if (glm::distance(m_clipTool.p1World, worldPt)
                        < kPlaneEpsilon) {
                        Log::editor()->warn(
                            "[clip] p2 == p1 (plano degenerado), "
                            "ignorando click");
                        continue;
                    }
                    m_clipTool.p2World = worldPt;
                    m_clipTool.hasP2 = true;
                    Log::editor()->info(
                        "[clip] p2 = ({:.1f}, {:.1f}, {:.1f}) "
                        "— Enter confirma, T cycle keep mode",
                        worldPt.x, worldPt.y, worldPt.z);
                } else {
                    // Ya tenia p1 + p2; este 3er click reemplaza p2
                    // (permite ajustar el plano sin cancelar).
                    m_clipTool.p2World = worldPt;
                    Log::editor()->info(
                        "[clip] p2 actualizado = ({:.1f}, {:.1f}, {:.1f})",
                        worldPt.x, worldPt.y, worldPt.z);
                }
            }
            // Hint en status bar.
            const char* keepLbl = "Front";
            if (m_clipTool.keepMode == ClipKeepMode::Back) keepLbl = "Back";
            else if (m_clipTool.keepMode == ClipKeepMode::Both) keepLbl = "Both";
            std::string hint = "Clip: ";
            if (!m_clipTool.hasP1) hint += "click 1ro punto";
            else if (!m_clipTool.hasP2) hint += "click 2do punto";
            else hint += "Enter confirma";
            hint += " — keep="; hint += keepLbl;
            hint += " (T cycle, Esc cancela)";
            m_ui.setStatusMessage(hint);
        }

        // 2.4b) F2H28 Bloque F: click-select desde los 3 viewports
        //       ortograficos del workspace "Editor de mapas". Solo se
        //       consume si el panel reporta `pending=true` — el panel
        //       ignora drags (umbral 4 px) asi que el filtro ya esta
        //       hecho. Reusa pickEntityFromRay con un rayo paralelo:
        //         origin = camera.worldFromNdc(...) - forwardAxis * 1024
        //         dir    = camera.forwardAxis()
        //       El offset de 1024 unidades atras del plano del view
        //       garantiza que el origen quede afuera de cualquier
        //       brush razonable; el rayAABB intersecta hacia adentro
        //       del frustum orto. La seleccion va al SelectionSet
        //       global -> los 4 viewports muestran el outline al frame
        //       siguiente sin codigo extra.
        if (m_mode == EditorMode::Editor && m_scene) {
            OrthoViewportPanel* orthoPanels[3] = {
                &m_ui.orthoTop(),
                &m_ui.orthoFront(),
                &m_ui.orthoSide(),
            };
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            const bool keyShift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            const bool keyCtrl  = keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
            for (OrthoViewportPanel* op : orthoPanels) {
                const auto orthoClick = op->consumeClickSelect();
                if (!orthoClick.pending) continue;
                const u32 oW = op->desiredWidth();
                const u32 oH = op->desiredHeight();
                if (oW == 0 || oH == 0) continue;
                const f32 oAspect = static_cast<f32>(oW) / static_cast<f32>(oH);
                const auto& cam = op->camera();
                const glm::vec3 origin = cam.worldFromNdc(orthoClick.ndcX,
                                                          orthoClick.ndcY,
                                                          oAspect)
                                       - cam.forwardAxis() * 1024.0f;
                const glm::vec3 dir = cam.forwardAxis();
                const ScenePickResult hit = pickEntityFromRay(
                    *m_scene, origin, dir, m_assetManager.get());

                SelectionSet& set = m_ui.selectionSet();
                if (hit) {
                    const std::string entityName =
                        hit.entity.hasComponent<TagComponent>()
                            ? hit.entity.getComponent<TagComponent>().name
                            : std::string{"(sin tag)"};
                    if (keyShift) {
                        add(set, hit.entity);
                        Log::editor()->info(
                            "[ortho:{}] Shift+click ADD '{}' (selected={})",
                            cam.label(), entityName, set.selected.size());
                    } else if (keyCtrl) {
                        toggle(set, hit.entity);
                        Log::editor()->info(
                            "[ortho:{}] Ctrl+click TOGGLE '{}' (selected={})",
                            cam.label(), entityName, set.selected.size());
                    } else {
                        replaceWithSingle(set, hit.entity);
                        Log::editor()->info("[ortho:{}] click replace '{}'",
                                              cam.label(), entityName);
                    }
                } else if (!keyShift && !keyCtrl) {
                    clear(set);
                }
            }
        }

        // 2.4c) F2H29 Bloque B: drag-edit de brushes en ortos.
        //       Una sesion de drag = LMB-down sobre brush + drag (>4 px) +
        //       LMB-up. Captura posiciones iniciales al arrancar, aplica
        //       delta snappeado en vivo cada frame, y al cerrar pushea
        //       MultiEditTransformCommand al HistoryStack para Ctrl+Z
        //       agrupado (mismo patron que finalizeGizmoDrag perspectivo).
        //       La sesion se mueve sobre el plano de la vista (ej. Top
        //       cambia X/Z, Y intacto). Snap se aplica al DELTA, no a
        //       posicion absoluta — preserva el offset original respecto
        //       al grid si el brush arranco desalineado.
        //       F2H30 Bloque B: solo aplica si subMode == Object — en
        //       Vertex/Edge mode el drag lo consume el bloque 2.4e.
        //       F2H30 Bloque C: tambien skipea si pincel poligonal
        //       esta activo (sino un click-with-drag accidental
        //       dispara el drag-edit del brush hovered).
        if (m_mode == EditorMode::Editor && m_scene &&
            m_subMode == EditorSubMode::Object &&
            !m_polyDraw.active) {
            OrthoViewportPanel* orthoPanels[3] = {
                &m_ui.orthoTop(),
                &m_ui.orthoFront(),
                &m_ui.orthoSide(),
            };
            // 1) Si NO hay sesion activa, ver si alguno arranco un drag.
            if (!m_orthoDragSession.active) {
                for (int i = 0; i < 3; ++i) {
                    const auto& ds = orthoPanels[i]->dragState();
                    if (!ds.active) continue;
                    const u32 oW = orthoPanels[i]->desiredWidth();
                    const u32 oH = orthoPanels[i]->desiredHeight();
                    if (oW == 0 || oH == 0) continue;
                    const f32 oAspect = static_cast<f32>(oW) /
                                         static_cast<f32>(oH);
                    const auto& cam = orthoPanels[i]->camera();
                    // Pick brush en ndcStart con rayo paralelo (mismo
                    // patron que el click-select del Bloque F).
                    const glm::vec3 origin = cam.worldFromNdc(
                        ds.ndcStartX, ds.ndcStartY, oAspect)
                        - cam.forwardAxis() * 1024.0f;
                    const glm::vec3 dir = cam.forwardAxis();
                    const ScenePickResult hit = pickEntityFromRay(
                        *m_scene, origin, dir, m_assetManager.get());
                    if (!hit) {
                        // F2H31 Bloque B: empty space — el comportamiento
                        // depende del MapTool activo:
                        //   Select      -> marquee session (handling 2.4f).
                        //   CreateBlock -> block tool session (handling 2.4d).
                        //   Pincel      -> consumido en 2.4a-poly (no llega aca).
                        if (m_mapTool == MapTool::CreateBlock) {
                            if (!m_orthoBlockSession.active) {
                                m_orthoBlockSession.active = true;
                                m_orthoBlockSession.orthoIdx = i;
                                Log::editor()->info(
                                    "[ortho:{}] block tool START", cam.label());
                            }
                        } else if (m_mapTool == MapTool::Select) {
                            if (!m_orthoMarquee.active) {
                                m_orthoMarquee.active = true;
                                m_orthoMarquee.orthoIdx = i;
                                Log::editor()->info(
                                    "[ortho:{}] marquee START", cam.label());
                            }
                        }
                        continue;
                    }
                    // Asegurar que el brush picked este en seleccion.
                    // Si no estaba, seleccion se reemplaza con ese brush.
                    SelectionSet& set = m_ui.selectionSet();
                    bool inSelection = false;
                    for (const Entity& e : set.selected) {
                        if (e && e.handle() == hit.entity.handle()) {
                            inSelection = true;
                            break;
                        }
                    }
                    if (!inSelection) replaceWithSingle(set, hit.entity);
                    // Capturar posiciones iniciales de TODAS las del set
                    // que tengan TransformComponent.
                    m_orthoDragSession.active = true;
                    m_orthoDragSession.orthoIdx = i;
                    m_orthoDragSession.startPositions.clear();
                    for (const Entity& e : set.selected) {
                        if (!e || !e.hasComponent<TransformComponent>()) continue;
                        const auto& tf = e.getComponent<TransformComponent>();
                        m_orthoDragSession.startPositions.emplace_back(
                            e, tf.position);
                    }
                    Log::editor()->info(
                        "[ortho:{}] drag-edit START ({} entidades)",
                        cam.label(),
                        m_orthoDragSession.startPositions.size());
                    break; // 1 drag a la vez
                }
            }
            // 2) Si hay sesion activa, aplicar delta o cerrar.
            if (m_orthoDragSession.active) {
                OrthoViewportPanel* op =
                    orthoPanels[m_orthoDragSession.orthoIdx];
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
                    glm::vec3 deltaWorld = wc - ws;
                    // Snap al delta (Hammer convention).
                    const f32 snap = static_cast<f32>(m_hammerSnapStep);
                    if (snap > 0.0f) {
                        deltaWorld.x = std::round(deltaWorld.x / snap) * snap;
                        deltaWorld.y = std::round(deltaWorld.y / snap) * snap;
                        deltaWorld.z = std::round(deltaWorld.z / snap) * snap;
                    }
                    // Aplicar delta a position en vivo.
                    for (auto& [e, startPos] :
                         m_orthoDragSession.startPositions) {
                        if (!e || !e.hasComponent<TransformComponent>()) continue;
                        auto& tf = e.getComponent<TransformComponent>();
                        tf.position = startPos + deltaWorld;
                    }
                    // Si el drag termino este frame, push del command.
                    if (ds.justEnded) {
                        std::vector<MultiEditTransformCommand::Entry> entries;
                        entries.reserve(
                            m_orthoDragSession.startPositions.size());
                        for (const auto& [e, startPos] :
                             m_orthoDragSession.startPositions) {
                            if (!e || !e.hasComponent<TransformComponent>()) continue;
                            const auto& tf = e.getComponent<TransformComponent>();
                            entries.push_back({e, startPos, tf.position});
                        }
                        // Revert al before antes del push para que
                        // execute() del command no haga doble apply
                        // (mismo patron que finalizeGizmoDrag).
                        for (auto& [e, startPos] :
                             m_orthoDragSession.startPositions) {
                            if (!e || !e.hasComponent<TransformComponent>()) continue;
                            auto& tf = e.getComponent<TransformComponent>();
                            tf.position = startPos;
                        }
                        auto cmd = std::make_unique<MultiEditTransformCommand>(
                            EditTransformCommand::Field::Position,
                            std::move(entries));
                        if (!cmd->isNoOp()) {
                            Log::editor()->info(
                                "[ortho:{}] drag-edit END push: {} ent, "
                                "delta=({:.2f}, {:.2f}, {:.2f})",
                                cam.label(),
                                m_orthoDragSession.startPositions.size(),
                                deltaWorld.x, deltaWorld.y, deltaWorld.z);
                            m_history.push(std::move(cmd));
                        }
                        m_orthoDragSession.active = false;
                        m_orthoDragSession.startPositions.clear();
                    }
                }
            }
        }

        // 2.4d/2.4f/2.4e) Modos de herramienta ortografica (block tool +
        //                 marquee select + vertex/edge edit). Extraidos a
        //                 `EditorApplication_RunInteractions_ToolModes.cpp`
        //                 en AUDIT-2 (sub-split del archivo de 1065 LOC).
        processOrthoToolModes();
}

} // namespace Mood
