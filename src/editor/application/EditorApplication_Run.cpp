// F2H24: loop principal del MoodEditor.
// run() — ejecuta hasta m_running=false:
//   1) hot-reload de texturas / shaders.
//   2) tick de FPS / metrics + Performance HUD.
//   3) processEvents + beginFrame.
//   4) m_ui.draw — construye widget tree.
//   5) Atender requests del UI: Play/Stop, Toolbar, Project actions,
//      open map, boolean op, recents, demos del menu Ayuda, drops.
//   6) Click-to-select (raycast desde cursor con Shift/Ctrl modifiers).
//   7) updateCameras + Physics + Scripts + Triggers + Animation +
//      Nav + Particles + Audio.
//   8) renderSceneToViewport + endFrame.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "core/math/AABB.h"  // F2H29 Bloque C: AABB del block tool preview.
#include "core/math/Plane.h"  // F2H30 Bloque B: kPlaneEpsilon en validacion.
#include "editor/commands/EditBrushGeometryCommand.h"  // F2H30 Bloque B: vertex/edge edit undo.
#include "editor/commands/EditTransformCommand.h"  // F2H29 Bloque B: Field enum.
#include "editor/commands/HistoryStack.h"
#include "editor/commands/MultiEditTransformCommand.h"  // F2H29 Bloque B: drag-edit undo agrupado.
#include "editor/panels/debug/PerformanceHudPanel.h"
#include "editor/panels/scene/OrthoViewportPanel.h"  // F2H28 Bloque F: click-select desde ortos
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"  // F2H29 Bloque C: drawAabb preview.
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/rhi/IRenderer.h"  // FrameStats definicion completa
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/world/csg/Brush.h"
#include "systems/animation/AnimationSystem.h"
#include "systems/audio/AudioSystem.h"
#include "systems/ai/NavSystem.h"
#include "systems/particles/ParticleSystem.h"
#include "systems/scripting/ScriptSystem.h"

#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>  // F2H29 Bloque C: glm::translate / scale.
#include <imgui.h>
#include <portable-file-dialogs.h>

#include <cmath>  // F2H29 Bloque B: std::round para snap al delta.
#include <string>

namespace Mood {

int EditorApplication::run() {
    m_deltaTimer.reset();

    while (m_running) {
        // Hot-reload de texturas solicitado el frame anterior (boton
        // Recargar del Asset Browser). Se aplica aca, entre frames, para
        // que ningun draw en curso use un GLuint recien destruido.
        if (m_ui.assetBrowser().consumeReloadRequest()) {
            const auto n = m_assetManager->reloadChanged();
            if (n > 0) {
                Log::editor()->info("Hot-reload: {} texturas actualizadas", n);
            }
        }

        const f64 dtD = m_deltaTimer.elapsedSeconds();
        const f32 dt = static_cast<f32>(dtD);
        m_deltaTimer.reset();

        const f32 fps = m_fpsCounter.tick(dtD);
        m_ui.setFps(fps);
        // F2H17: sync sub-mode al UI cada frame para statusbar e
        // InspectorPanel.
        m_ui.setSubMode(m_subMode);
        MOOD_PROFILE_PLOT("FPS", fps);
        MOOD_PROFILE_PLOT("FrameMs", static_cast<f32>(dtD * 1000.0));
        if (m_scene) {
            MOOD_PROFILE_PLOT("EntityCount",
                static_cast<int64_t>(m_scene->entityCount()));
        }

        // F2H2: pasar metricas al Performance HUD. Lee stats del frame
        // ANTERIOR (m_renderer->frameStats todavia no fue reset por
        // beginFrame de este frame). Esto es lo que queremos: el HUD
        // muestra la carga real del frame que el dev acaba de ver.
        {
            PerformanceHudPanel::Metrics metrics;
            metrics.fps = fps;
            metrics.frameMs = static_cast<f32>(dtD * 1000.0);
            if (m_sceneRenderer) {
                const FrameStats stats = m_sceneRenderer->frameStats();
                metrics.drawCalls = stats.drawCalls;
                metrics.triangles = stats.triangles;
            }
            if (m_scene) {
                metrics.entityCount = static_cast<u32>(m_scene->entityCount());
            }
            m_ui.performanceHud().setMetrics(metrics);
        }

        // Hot-reload de shaders (Hito 25 G): chequeo throttle 500ms de
        // mtime y recompila los .vert/.frag que cambiaron en disco.
        // Iterativo y atomico — un error de compilacion mantiene el
        // shader anterior funcionando.
        OpenGLShader::tickHotReload(dt);

        processEvents();
        beginFrame();

        // 1) Construir el widget tree de ImGui. ViewportPanel captura input
        //    de camara aqui (solo efectivo en Editor Mode).
        bool requestQuit = false;
        {
            MOOD_PROFILE_SCOPE("UI::draw");
            m_ui.draw(requestQuit);
        }

        // 2) Atender toggles de modo solicitados desde la UI (boton Play/Stop).
        if (m_ui.consumeTogglePlayRequest()) {
            if (m_mode == EditorMode::Editor) {
                enterPlayMode();
            } else {
                exitPlayMode();
            }
        }

        // F2H22: requests del Toolbar lateral.
        const int gizmoReq = m_ui.consumeGizmoModeRequest();
        if (gizmoReq >= 0 && gizmoReq <= 2) {
            m_gizmoMode = static_cast<GizmoMode>(gizmoReq);
        }
        if (m_ui.consumeToggleFaceModeRequest()) {
            m_subMode = (m_subMode == EditorSubMode::Face)
                ? EditorSubMode::Object
                : EditorSubMode::Face;
            Log::editor()->info("[toolbar] sub-mode -> {}",
                m_subMode == EditorSubMode::Face ? "Face" : "Object");
        }
        // F2H30 Bloque C: top toolbar (Map Tools) requests.
        EditorSubMode requestedSubMode;
        if (m_ui.consumeSubModeRequest(requestedSubMode)) {
            m_subMode = requestedSubMode;
            m_ui.selectionSet().activeFaceIndex = -1;
            // Si activan otro sub-modo, cancelar el pincel si estaba.
            if (m_polyDraw.active) cancelPolygonDraw();
            const char* lbl = "Object";
            switch (m_subMode) {
                case EditorSubMode::Vertex: lbl = "Vertex"; break;
                case EditorSubMode::Edge:   lbl = "Edge";   break;
                case EditorSubMode::Face:   lbl = "Face";   break;
                case EditorSubMode::Object: lbl = "Object"; break;
            }
            Log::editor()->info("[map toolbar] sub-mode -> {}", lbl);
        }
        if (m_ui.consumeTogglePolygonDrawRequest()) {
            togglePolygonDrawMode();
        }
        // Sync state: para que la top toolbar pueda highlight el
        // boton "Pincel" cuando esta activo.
        m_ui.setPolygonDrawActive(m_polyDraw.active);

        // 2.25) Acciones de proyecto (Archivo > Nuevo / Abrir / Guardar / ...).
        switch (m_ui.consumeProjectAction()) {
            case ProjectAction::NewProject:   handleNewProject();   break;
            case ProjectAction::OpenProject:  handleOpenProject();  break;
            case ProjectAction::Save:         handleSave();         break;
            case ProjectAction::SaveAs:       handleSaveAs();       break;
            case ProjectAction::CloseProject:   handleCloseProject();   break;
            case ProjectAction::PackageProject: handlePackageProject(); break;
            case ProjectAction::NewScript:      handleNewScript();      break;
            // F2H8: gestion multi-mapa.
            case ProjectAction::NewMap:                  handleNewMap();                   break;
            case ProjectAction::SaveMapAs:               handleSaveMapAs();                break;
            case ProjectAction::SetCurrentMapAsDefault:  handleSetCurrentMapAsDefault();   break;
            case ProjectAction::DeleteCurrentMap:        handleDeleteCurrentMap();         break;
            case ProjectAction::AddBoxBrush:             handleAddBoxBrush();              break;
            // F2H14: primitivas extendidas.
            case ProjectAction::AddCylinderBrush:        handleAddCylinderBrush();         break;
            case ProjectAction::AddSphereBrush:          handleAddSphereBrush();           break;
            case ProjectAction::AddPyramidBrush:         handleAddPyramidBrush();          break;
            case ProjectAction::AddWedgeBrush:           handleAddWedgeBrush();            break;
            case ProjectAction::AddPrismTriangularBrush: handleAddPrismTriangularBrush();  break;
            case ProjectAction::AddPrismHexagonalBrush:  handleAddPrismHexagonalBrush();   break;
            // F2H20: compilacion brush -> mesh estatica + export OBJ.
            case ProjectAction::CompileMap:              handleCompileMap();               break;
            case ProjectAction::ExportObj:               handleExportObj();                break;
            case ProjectAction::None:           break;
        }
        // F2H8: open map request (con payload del path).
        if (auto openMap = m_ui.consumeOpenMapRequest()) {
            handleOpenMap(*openMap);
        }

        // F2H12: boolean op request (con payload kind + entity B).
        if (auto bopKind = m_ui.consumeBooleanOpRequest()) {
            handleBooleanOp(*bopKind);
        }

        // Hito 15 polish: el modal Welcome puede editar la lista de
        // recientes (boton X por entrada + "Limpiar inexistentes"). Cuando
        // hay cambios, sincronizamos `m_recentProjects` con la lista del
        // UI y persistimos al `editor_state.json`.
        if (m_ui.consumeRecentsDirty()) {
            m_recentProjects.assign(m_ui.recentProjects().begin(),
                                     m_ui.recentProjects().end());
            saveEditorState();
        }

        // Modal Welcome: click en un reciente emite path directo en lugar
        // de pasar por el dialogo pfd.
        if (auto recentPath = m_ui.consumeOpenProjectPath(); recentPath.has_value()) {
            if (!tryOpenProjectPath(*recentPath)) {
                pfd::message("MoodEngine",
                    "No se pudo abrir el proyecto reciente '" +
                    recentPath->generic_string() + "'. Quedara resaltado en la "
                    "lista hasta que se cierre el editor.",
                    pfd::choice::ok, pfd::icon::warning);
            }
        }

        // Demo Hito 8: "Ayuda > Agregar rotador demo". Crea una entidad
        // flotante sobre el centro del mapa con ScriptComponent apuntando a
        // assets/scripts/rotator.lua. Util para validar el ScriptSystem sin
        // tocar el mapa ni el serializer.
        // Demos del menu Ayuda + handlers de drag&drop al viewport. Hito 16:
        // implementaciones movidas a `DemoSpawners.cpp` para mantener `run()`
        // legible.
        processSpawnRotatorRequest();
        processSpawnHudDemoRequest();
        processSpawnEnemyDemoRequest();
        processSpawnPhysicsBoxRequest();
        processSpawnEnvironmentRequest();
        processSpawnShadowDemoRequest();
        processSpawnPbrSpheresRequest();
        processSpawnLightStressRequest();
        processSpawnAnimatedCharacterRequest();
        processSpawnFireParticlesRequest();
        processSpawnTriggerRequest();
        processSpawnStressTrisRequest();
        processSpawnPointLightRequest();
        processSpawnAudioSourceRequest();
        processSavePrefabRequest();

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

            // F2H17: en Face Mode, intentar pickFace contra el brush
            // active ANTES del pickEntity. Si pega, solo actualiza
            // activeFaceIndex sin tocar la seleccion. Si NO pega o
            // no hay brush active, fallback al picking de Object Mode.
            bool faceModeHandled = false;
            if (m_subMode == EditorSubMode::Face) {
                SelectionSet& selSet = m_ui.selectionSet();
                if (static_cast<bool>(selSet.active) &&
                    selSet.active.hasComponent<BrushComponent>() &&
                    selSet.active.hasComponent<TransformComponent>()) {
                    auto& bc = selSet.active.getComponent<BrushComponent>();
                    auto& t = selSet.active.getComponent<TransformComponent>();
                    const glm::mat4 invVP =
                        glm::inverse(projection * view);
                    const glm::vec4 nearH = invVP *
                        glm::vec4(click.ndcX, click.ndcY, -1.0f, 1.0f);
                    const glm::vec4 farH = invVP *
                        glm::vec4(click.ndcX, click.ndcY, 1.0f, 1.0f);
                    const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
                    const glm::vec3 farW = glm::vec3(farH) / farH.w;
                    const glm::vec3 origin = nearW;
                    const glm::vec3 dir = glm::normalize(farW - nearW);
                    const auto faceHit = Csg::pickFace(
                        bc.brush, origin, dir, t.worldMatrix());
                    if (faceHit.has_value()) {
                        selSet.activeFaceIndex =
                            static_cast<i32>(*faceHit);
                        Log::editor()->info(
                            "Face Mode: selected face {} of brush '{}'",
                            *faceHit,
                            selSet.active.hasComponent<TagComponent>()
                                ? selSet.active.getComponent<TagComponent>().name
                                : std::string{"(sin tag)"});
                        faceModeHandled = true;
                    }
                    // Sin face hit: cae al picking de Object para
                    // permitir cambiar de brush activo.
                }
            }

            ScenePickResult hit{};
            if (!faceModeHandled) {
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
                // Snap absoluto al grid (mismo que vertex/edge edit).
                const f32 snap = static_cast<f32>(m_hammerSnapStep);
                if (snap > 0.0f) {
                    worldPt.x = std::round(worldPt.x / snap) * snap;
                    worldPt.y = std::round(worldPt.y / snap) * snap;
                    worldPt.z = std::round(worldPt.z / snap) * snap;
                }
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
                        // F2H29 Bloque C: empty space — block tool.
                        // Marca sesion; el handling (preview AABB +
                        // spawn al soltar) vive en el bloque 2.4d.
                        if (!m_orthoBlockSession.active) {
                            m_orthoBlockSession.active = true;
                            m_orthoBlockSession.orthoIdx = i;
                            Log::editor()->info(
                                "[ortho:{}] block tool START", cam.label());
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
        if (m_mode == EditorMode::Editor && m_scene &&
            m_subMode == EditorSubMode::Object &&
            !m_polyDraw.active &&
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
                // Snap ambas esquinas al grid (no al delta — para que
                // el brush quede alineado al grid global).
                const f32 snap = static_cast<f32>(m_hammerSnapStep);
                auto snapVec3 = [snap](glm::vec3 v) {
                    if (snap > 0.0f) {
                        v.x = std::round(v.x / snap) * snap;
                        v.y = std::round(v.y / snap) * snap;
                        v.z = std::round(v.z / snap) * snap;
                    }
                    return v;
                };
                const glm::vec3 wsSnap = snapVec3(ws);
                const glm::vec3 wcSnap = snapVec3(wc);
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

        // 2.5/2.6/2.7) Drops desde AssetBrowser (textura / mesh / prefab).
        // Hito 16: implementaciones movidas a DemoSpawners.cpp para
        // mantener run() legible.
        processViewportTextureDrop();
        processViewportMeshDrop();
        processViewportPrefabDrop();
        processViewportMaterialDrop();
        processViewportScriptDrop();

        // 3) Input -> camaras.
        {
            MOOD_PROFILE_SCOPE("updateCameras");
            updateCameras(dt);
        }

        // 3.4) Fisica (Jolt, Hito 12): materializa bodies nuevos siempre y
        //      stepea la sim solo en Play Mode. Despues del input y antes
        //      de scripts — asi un script puede leer la posicion post-step.
        {
            MOOD_PROFILE_SCOPE("Physics::updateRigidBodies");
            updateRigidBodies(dt);
        }

        // 3.5) Scripts Lua: correr onUpdate(self, dt) en cada entidad con
        //      ScriptComponent. Antes del render para que los cambios en
        //      Transform se vean este mismo frame.
        if (m_scene && m_scriptSystem) {
            MOOD_PROFILE_SCOPE("ScriptSystem::update");
            m_scriptSystem->update(*m_scene, dt, m_physicsWorld.get());
        }

        // Hito 33: triggers detectan al player char entrando/saliendo y
        // dispatchan on_trigger_enter/_exit a los scripts. Solo en Play
        // Mode (m_playerCharId != 0). Despues del scriptSystem para que
        // los handlers ya esten registrados.
        if (m_scene && m_scriptSystem && m_physicsWorld
            && m_mode == EditorMode::Play) {
            MOOD_PROFILE_SCOPE("TriggerSystem::update");
            m_triggerSystem.update(*m_scene, *m_physicsWorld,
                                    *m_scriptSystem, m_playerCharId);
        }

        // 3.55) Animacion (Hito 19): avanza time del Animator y rellena el
        //       SkeletonComponent.skinningMatrices que el render lee. Antes
        //       del audio para que un sound posicional adjunto a un hueso
        //       (futuro) ya tenga la pose actualizada — hoy es indistinto.
        if (m_scene && m_animationSystem) {
            MOOD_PROFILE_SCOPE("AnimationSystem::update");
            m_animationSystem->update(*m_scene, *m_assetManager, dt);
        }

        // 3.57) Navegacion (Hito 23): solo en Play Mode. En Editor Mode los
        //       agentes no se mueven (mismo criterio que la fisica).
        //       Antes del NavSystem actualizamos el `target` de cada agente
        //       a la posicion del FpsCamera — los enemigos persiguen al
        //       jugador. Si en el futuro un script Lua quiere setear otro
        //       target (patrol, idle, item), lo va a poder hacer via
        //       binding cuando se exponga NavAgent a sol2.
        if (m_mode == EditorMode::Play && m_scene && m_navSystem) {
            MOOD_PROFILE_SCOPE("NavSystem::update");
            const glm::vec3 playerPos = m_playCamera.position();
            m_scene->forEach<NavAgentComponent>(
                [&](Entity, NavAgentComponent& nav) {
                    nav.target = playerPos;
                });
            m_navSystem->update(*m_scene, m_map, mapWorldOrigin(), dt);
        }

        // 3.58) Particulas (Hito 29): corren en TODOS los modos (incluido
        //       Editor) para que el dev vea en vivo lo que esta editando.
        //       Sin pausa especifica — si el dev quiere congelarlas, baja
        //       el emitRate a 0 desde el Inspector.
        if (m_scene && m_particleSystem) {
            MOOD_PROFILE_SCOPE("ParticleSystem::update");
            m_particleSystem->update(*m_scene, dt);
        }

        // 3.6) Audio: arranca playOnStart, sincroniza posicion 3D, y setea
        //      el listener a la camara activa. Despues de scripts para que
        //      cualquier cambio de Transform del frame ya este aplicado.
        if (m_scene && m_audioSystem) {
            MOOD_PROFILE_SCOPE("AudioSystem::update");
            m_audioSystem->update(*m_scene, dt);

            // Listener = camara activa segun modo. World-up fijo (0,1,0).
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            if (m_mode == EditorMode::Play) {
                m_audioSystem->setListener(
                    m_playCamera.position(),
                    m_playCamera.forward(),
                    worldUp);
            } else {
                // Editor Mode: listener en la posicion de la orbit cam
                // mirando al origen (target por defecto del EditorCamera).
                const glm::vec3 camPos = m_editorCamera.position();
                const glm::vec3 fwd = glm::length(camPos) > 1e-5f
                    ? glm::normalize(-camPos)
                    : glm::vec3(0.0f, 0.0f, -1.0f);
                m_audioSystem->setListener(camPos, fwd, worldUp);
            }
        }

        // 4) Render 3D al framebuffer offscreen (el panel mostrara la textura).
        {
            MOOD_PROFILE_SCOPE("renderSceneToViewport");
            renderSceneToViewport(dt);
        }

        // 5) Draw final de ImGui.
        endFrame();

        if (requestQuit) {
            m_running = false;
        }

        MOOD_PROFILE_FRAME();
    }

    return 0;
}

} // namespace Mood
