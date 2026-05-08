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
#include "editor/commands/HistoryStack.h"
#include "editor/panels/debug/PerformanceHudPanel.h"
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
#include <imgui.h>
#include <portable-file-dialogs.h>

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
