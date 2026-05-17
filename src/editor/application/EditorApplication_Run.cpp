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
#include "engine/dialog/DialogInteractSystem.h"  // F2H48
#include "engine/dialog/DialogSystem.h"          // F2H48
#include "engine/game/state/GameState.h"         // F2H52 H: Tab toggle inventory_panel
#include "engine/inventory/ItemPickupSystem.h"   // F2H52 Bloque C
#include "engine/quest/QuestSystem.h"            // F2H53 Bloque F
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

            // F2H42: aplicar toggle VSync si el dev clickeo el checkbox.
            bool vsyncRequested = true;
            if (m_ui.performanceHud().consumeVsyncToggleRequest(vsyncRequested)) {
                m_window->setVSync(vsyncRequested);
                // Sync el panel con el estado real del Window (puede no
                // haberse aplicado si el driver rechazo el cambio).
                m_ui.performanceHud().setVsyncState(m_window->vsyncEnabled());
            }
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
            m_ui.selectionSet().selectedFaceIndices.clear();  // F2H33
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
            // F2H31 Bloque B: refleja el estado del pincel en m_mapTool.
            // togglePolygonDrawMode ya forza m_subMode=Object; aca solo
            // resincronizamos el tool selector del toolbar.
            m_mapTool = m_polyDraw.active ? MapTool::Pincel : MapTool::Select;
        }
        // F2H31 Bloque B: tool selector del toolbar (Select / CreateBlock).
        // Pincel se setea via la rama togglePolygonDrawMode arriba; aca
        // solo gestionamos las otras 2 transiciones — y si el dev cambia
        // de Pincel a otro tool, cancelamos el pincel.
        MapTool requestedTool;
        if (m_ui.consumeMapToolRequest(requestedTool)) {
            if (requestedTool != MapTool::Pincel && m_polyDraw.active) {
                cancelPolygonDraw();
            }
            // F2H32 Bloque B: si cambiamos de tool y habia una sesion
            // de clip activa, cancelarla (sin spawnear).
            if (requestedTool != MapTool::Clip && m_clipTool.active) {
                cancelClipTool();
            }
            // F2H32 Bloque B: hint visible si se activa Clip sin nada
            // selecto. El consumer real (confirmClipTool) chequea de
            // nuevo y aborta si la seleccion sigue vacia.
            if (requestedTool == MapTool::Clip) {
                bool hasSelection = false;
                for (const Entity& sel : m_ui.selectionSet().selected) {
                    if (sel && sel.hasComponent<BrushComponent>()) {
                        hasSelection = true;
                        break;
                    }
                }
                if (!hasSelection) {
                    Log::editor()->warn(
                        "[clip] activado sin brush selecto — clickea "
                        "'Selecc' + click sobre un brush primero");
                    m_ui.setStatusMessage(
                        "Clip: selecciona un brush primero (Selecc tool)");
                }
            }
            if (requestedTool != MapTool::Pincel) {
                m_mapTool = requestedTool;
                const char* lbl = "Select";
                switch (requestedTool) {
                    case MapTool::Select:      lbl = "Select"; break;
                    case MapTool::CreateBlock: lbl = "CreateBlock"; break;
                    case MapTool::Pincel:      lbl = "Pincel"; break;
                    case MapTool::Clip:        lbl = "Clip"; break;
                }
                Log::editor()->info("[map toolbar] tool -> {}", lbl);
            }
            // Si requestedTool == Pincel, requestTogglePolygonDraw ya
            // se disparo (el toolbar emite ambos requests cuando clickeas
            // el boton Pincel) y la rama de arriba lo manejo.
        }
        // F2H31 Bloque C: toggle snap-to-vertex desde el toolbar
        // (alias de la tecla V que vive en EditorOverlay).
        if (m_ui.consumeToggleSnapToVertexRequest()) {
            m_snapToVertexEnabled = !m_snapToVertexEnabled;
            Log::editor()->info("[snap] vertex snap = {} (via toolbar)",
                m_snapToVertexEnabled ? "on" : "off");
        }
        // F2H32 Bloque C: carve action button (sin keyboard shortcut).
        if (m_ui.consumeCarveRequest()) {
            handleCarve();
        }
        // F2H35 Bloque E: toggle labels point entities (boton "Nombres"
        // del toolbar). Estado vive en EditorUI (default ON).
        if (m_ui.consumeToggleEntityLabelsRequest()) {
            m_ui.setShowEntityLabels(!m_ui.showEntityLabels());
            Log::editor()->info("[labels] entity labels = {}",
                m_ui.showEntityLabels() ? "on" : "off");
            markDirty();  // persistir el toggle al guardar el proyecto
        }
        // Sync state: para que la top toolbar pueda highlight los
        // botones activos.
        m_ui.setPolygonDrawActive(m_polyDraw.active);
        m_ui.setMapTool(m_mapTool);
        m_ui.setSnapToVertexEnabled(m_snapToVertexEnabled);

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
            // F2H59: primitivas clasicas adicionales.
            case ProjectAction::AddPlaneBrush:           handleAddPlaneBrush();            break;
            case ProjectAction::AddQuadBrush:            handleAddQuadBrush();             break;
            case ProjectAction::AddConeBrush:            handleAddConeBrush();             break;
            case ProjectAction::AddCapsuleBrush:         handleAddCapsuleBrush();          break;
            // F2H60 polish iter2: spawn directo de luces desde el modal.
            case ProjectAction::AddDirectionalLight:     handleAddDirectionalLight();      break;
            case ProjectAction::AddPointLight:           handleAddPointLight();            break;
            // F2H20: compilacion brush -> mesh estatica + export OBJ.
            case ProjectAction::CompileMap:              handleCompileMap();               break;
            case ProjectAction::ExportObj:               handleExportObj();                break;
            case ProjectAction::None:           break;
        }

        // F2H44: Welcome modal demo. Crea proyecto vacio + spawnea Fox.
        // Sincronia: handleNewProject bloquea con un pfd::save_file
        // dialog; cuando retorna ya hay m_project (o el dev cancelo).
        // Si hubo proyecto creado, agendamos el spawn via flag — la
        // dispatch de spawners corre mas abajo en el mismo frame.
        if (m_ui.consumeOpenDemoMapRequest()) {
            handleNewProject();
            if (m_project.has_value()) {
                m_ui.requestSpawnAnimatedCharacter();
            }
        }

        // F2H50 Bloque C: Welcome modal — demo narrativo. Crea proyecto
        // fresco y agenda la generacion/apertura del narrative_demo.moodmap
        // (genera el .moodmap + el .mooddialog si faltan, despues lo abre).
        if (m_ui.consumeOpenNarrativeDemoRequest()) {
            handleNewProject();
            if (m_project.has_value()) {
                m_ui.requestGenerateNarrativeDemoMap();
            }
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
        // F2H42: full stress scene PRIMERO — setea flags individuales que
        // los process*Request de abajo consumen en este mismo frame.
        processSpawnFullStressSceneRequest();
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
        processSpawnDialogDemoRequest();  // F2H47
        processGenerateNarrativeDemoMapRequest();  // F2H50 Bloque A
        processSpawnStressTrisRequest();
        processSpawnPointLightRequest();
        processSpawnAudioSourceRequest();
        processSavePrefabRequest();
        processCreateEntityFromModelRequest();  // F2H57
        processCreateEntityPlaceholderRequest();  // F2H57 followup
        renderPickFromLoadedMeshesModal();        // F2H57 followup

        // F2H57 Bloque C: menu contextual del panel Escena puede
        // disparar el delete sin pasar por la tecla del teclado.
        if (m_ui.consumeDeleteSelectedRequest()) {
            deleteSelectedEntity();
        }

        // F2H57 Bloque D: modal "Convertir entidad". Se abre cuando
        // se consume el request del context menu del HierarchyPanel.
        renderConvertEntityModal();

        // 2.4) Interacciones del viewport: click-to-select (perspectiva +
        //      3 orthos), face mode picking, polygon brush, clip tool,
        //      drag-edit / block tool / vertex-edge edit / marquee en
        //      ortos. Extraido a `EditorApplication_RunInteractions.cpp`
        //      en AUDIT-1 para mantener este archivo bajo el hard cap
        //      de 800 LOC.
        processViewportInteractions();


        // 2.5/2.6/2.7) Drops desde AssetBrowser (textura / mesh / prefab).
        // Hito 16: implementaciones movidas a DemoSpawners.cpp para
        // mantener run() legible.
        processViewportTextureDrop();
        processViewportMeshDrop();
        processViewportPrefabDrop();
        processViewportMaterialDrop();
        processViewportScriptDrop();
        processViewportItemDrop();   // F2H52 Bloque D

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
            m_scriptSystem->update(*m_scene, dt, m_physicsWorld.get(),
                                    m_assetManager.get());  // F2H48: dialog bindings
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

        // F2H48: el DialogInteractSystem consume el `playerInside` de los
        // triggers + busca DialogComponent en la misma entidad. Si el
        // player presiono E este frame, arranca el dialog. Tambien
        // mantiene el HUD interact_prompt sincronizado con la presencia
        // del player en el trigger.
        if (m_scene && m_assetManager && m_mode == EditorMode::Play) {
            // Detectar flanco up->down de E (mismo patron que SPACE).
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            const bool ePressed = keys[SDL_SCANCODE_E] != 0;
            m_ePlayJustPressed   = ePressed && !m_ePlayPrevFrame;
            m_ePlayPrevFrame     = ePressed;

            MOOD_PROFILE_SCOPE("DialogInteractSystem::tick");
            const bool dialogStarted = Dialog::DialogInteractSystem::tick(
                *m_scene, *m_assetManager, m_ePlayJustPressed);

            // Si dialog ya esta activo (no recien arrancado), detectar
            // teclas 1-9 para choices + E para continueNext.
            if (!dialogStarted && Dialog::DialogSystem::isActive()) {
                int digitJustPressed = -1;
                for (int i = 0; i < 9; ++i) {
                    const bool pressed = keys[SDL_SCANCODE_1 + i] != 0;
                    if (pressed && !m_digitPrevFrame[i]) {
                        digitJustPressed = i + 1;
                    }
                    m_digitPrevFrame[i] = pressed;
                }
                Dialog::DialogInteractSystem::tickActiveDialog(
                    m_ePlayJustPressed, digitJustPressed);
            } else {
                // Sin dialog activo: reset prev state de digitos para que
                // el primer digit que se presione sea "just pressed".
                for (int i = 0; i < 9; ++i) m_digitPrevFrame[i] = false;
            }

            // F2H52 Bloque C: ItemPickupSystem. Consume el mismo flanco
            // de E que el dialog system (si hay dialog activo, el pickup
            // sistema lo skipea para no robarle la tecla). Engine-grade:
            // el motor SOLO mueve items entre containers + dispara el
            // hook on_pickup (registrado por Lua en Bloque F).
            {
                MOOD_PROFILE_SCOPE("ItemPickupSystem::tick");
                const bool dialogActive =
                    dialogStarted || Dialog::DialogSystem::isActive();
                Inventory::ItemPickupSystem::tick(
                    *m_scene, *m_assetManager,
                    m_ePlayJustPressed, dialogActive);
            }

            // F2H53 Bloque F: QuestSystem::tick — evalua predicates de
            // objectives via el LuaEvaluator (inyectado por
            // setupQuestBindings desde ScriptSystem). Lo corremos despues
            // del ItemPickupSystem para que un Collect predicate
            // (`inventory.count(...) >= N`) que recien quedo satisfecho
            // por el pickup de este frame se complete inmediatamente.
            // Tambien despues del DialogInteractSystem para que objectives
            // Talk/Reach que dependen de `dialog.has_var(...)` resuelvan
            // en el mismo frame que el dialog set la var.
            {
                MOOD_PROFILE_SCOPE("QuestSystem::tick");
                Mood::Quest::QuestSystem::tick(*m_assetManager);
            }

            // F2H52 Bloque H: Tab togglea el widget `inventory_panel`.
            // Flanco up->down (no spam mientras se mantiene). Skipea si
            // hay un dialog abierto (Tab le pertenece al dialog si en el
            // futuro lo usa para skip).
            {
                const bool tabPressed = keys[SDL_SCANCODE_TAB] != 0;
                const bool tabJustPressed = tabPressed && !m_tabPrevFrame;
                m_tabPrevFrame = tabPressed;
                if (tabJustPressed && !Dialog::DialogSystem::isActive()) {
                    auto& hudRef = Mood::GameState::hud();
                    const bool nowOn = !hudRef.isWidgetEnabled("inventory_panel");
                    hudRef.widget_enabled["inventory_panel"] = nowOn;
                }
            }

            // F2H53 Bloque G: J togglea el widget `quest_log_panel`.
            // Mismo patron que Tab para el inventory. Convencion RPG
            // (Skyrim, Witcher) — J de "Journal".
            {
                const bool jPressed = keys[SDL_SCANCODE_J] != 0;
                const bool jJustPressed = jPressed && !m_jPrevFrame;
                m_jPrevFrame = jPressed;
                if (jJustPressed && !Dialog::DialogSystem::isActive()) {
                    auto& hudRef = Mood::GameState::hud();
                    const bool nowOn = !hudRef.isWidgetEnabled("quest_log_panel");
                    hudRef.widget_enabled["quest_log_panel"] = nowOn;
                }
            }
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
