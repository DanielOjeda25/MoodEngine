#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "core/math/AABB.h"
#include "engine/render/preview/MaterialPreviewRenderer.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLMesh.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/scene/components/BrushComponent.h"  // F2H17: pickFace
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ScenePick.h"
#include "engine/world/csg/Brush.h"  // F2H17: Csg::pickFace
#include "engine/scene/queries/ViewportPick.h"
#include "engine/scene/serialization/PrefabSerializer.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/audio/device/AudioDevice.h"
#include "engine/game/state/GameState.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "systems/animation/AnimationSystem.h"
#include "systems/audio/AudioSystem.h"
#include "systems/light/LightSystem.h"
#include "systems/ai/NavSystem.h"
#include "systems/particles/ParticleSystem.h"
#include "systems/render/PostProcessPass.h"
#include "systems/render/ShadowPass.h"
#include "systems/render/SkyboxRenderer.h"
#include "systems/physics/PhysicsSystem.h"
#include "systems/scripting/ScriptSystem.h"
#include "systems/physics/TriggerSystem.h"

#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL
// (evita redefiniciones con <SDL_opengl.h> o los loaders internos).
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Mood {

namespace {

constexpr const char* k_GlslVersion = "#version 450 core";

// Callback que recibe el driver para cada warning/error GL. Filtra por
// severidad y rutea al logger `render` con el nivel equivalente. Solo se
// activa si el contexto fue creado con el debug bit (builds Debug).
void APIENTRY glDebugCallback(GLenum /*source*/, GLenum type, GLuint id,
                               GLenum severity, GLsizei /*length*/,
                               const GLchar* message, const void* /*userParam*/) {
    // Filtrar notifications verbosos del driver (buffer info, etc.).
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    // Silenciar warnings conocidos que sabemos benignos y que spamearian la
    // Console cada frame. Si el driver cambia y empiezan a aparecer otros
    // mensajes relevantes, el callback los sigue capturando.
    if (std::strstr(message, "API_ID_LINE_WIDTH") != nullptr) {
        // glLineWidth(>1.0) esta deprecated en Core Profile pero todos los
        // drivers actuales lo honran. Lo usamos en el debug draw.
        return;
    }

    const char* typeStr = "other";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "error";       break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "deprecated";  break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "undefined";   break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "perf";        break;
        default: break;
    }

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        Mood::Log::render()->error("[GL {} #{}] {}", typeStr, id, message);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Mood::Log::render()->warn("[GL {} #{}] {}", typeStr, id, message);
    } else {
        Mood::Log::render()->debug("[GL {} #{}] {}", typeStr, id, message);
    }
}

} // namespace

EditorApplication::EditorApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodEngine iniciando...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodEngine Editor - v0.4.0-dev (Hito 4)";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    // --- GLAD ---
    if (!gladLoaderLoadGL()) {
        throw std::runtime_error("gladLoaderLoadGL fallo: no se pudo cargar OpenGL");
    }
    Log::render()->info("OpenGL iniciado: {}",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::render()->info("GPU: {} {}",
        reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    // Debug context callback: solo si el driver expone la extension y el
    // contexto fue creado con el flag (builds Debug). En Release el callback
    // no esta enganchado y no cuesta nada.
    {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // stacktrace util al paso
            glDebugMessageCallback(glDebugCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                                  0, nullptr, GL_TRUE);
            Log::render()->info("GL debug output enganchado");
        }
    }

    // --- Dear ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    // Hito 21 Bloque 2: SceneRenderer es dueno del pipeline completo de
    // render (FBs, shaders PBR, skybox, shadow, post-process, IBL, light
    // grid + SSBOs, debug renderer). El editor solo le pasa scene +
    // assets + camara + tamano del panel cada frame.
    m_sceneRenderer = std::make_unique<SceneRenderer>();

    // Factoria real: crea OpenGLTexture desde el path de filesystem. Los tests
    // pasan otra factoria que devuelve mocks sin tocar GL. El cubo fallback
    // ya no vive aca: el AssetManager lo genera via MeshFactory en el slot 0.
    m_assetManager = std::make_unique<AssetManager>(
        "assets",
        [](const std::string& fsPath) -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(fsPath);
        },
        /*audioFactory*/ AssetManager::AudioClipFactory{},
        /*meshFactory*/
        [](const std::vector<f32>& verts,
           const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
            return std::make_unique<OpenGLMesh>(verts, attrs);
        },
        /*textureMemoryFactory (Hito 26)*/
        [](const std::vector<u8>& bytes, const std::string& nameForLog)
            -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(bytes, nameForLog);
        });
    m_ui.assetBrowser().setAssetManager(m_assetManager.get());
    m_ui.setHistoryStack(&m_history); // Hito 27: para items Editar > Deshacer/Rehacer

    m_ui.viewport().setFramebuffer(&m_sceneRenderer->viewportFb());

    m_scene = std::make_unique<Scene>();
    m_scriptSystem = std::make_unique<ScriptSystem>();
    m_animationSystem = std::make_unique<AnimationSystem>(); // Hito 19
    m_navSystem       = std::make_unique<NavSystem>();       // Hito 23
    m_particleSystem  = std::make_unique<ParticleSystem>();  // Hito 29
    // Hito 12: PhysicsWorld (Jolt). Init es pesado la primera vez del proceso
    // (Factory + RegisterTypes) pero instancias subsecuentes son baratas.
    m_physicsWorld = std::make_unique<PhysicsWorld>();

    // Audio (Hito 9). El device tolera fallo de init (modo mute) — el motor
    // sigue funcionando aunque no haya hardware de audio.
    m_audioDevice = std::make_unique<AudioDevice>();
    m_audioSystem = std::make_unique<AudioSystem>(*m_audioDevice, *m_assetManager);

    // Inyectar dependencias en los paneles de ECS.
    m_ui.setScene(m_scene.get());  // F2H12: necesario para drawBooleanOpMenu
    m_ui.hierarchy().setScene(m_scene.get());
    m_ui.hierarchy().setEditorUi(&m_ui);
    m_ui.inspector().setEditorUi(&m_ui);
    m_ui.inspector().setAssetManager(m_assetManager.get());
    m_ui.materialEditor().setAssetManager(m_assetManager.get());  // Hito 42

    // F2H21: preview esferico off-screen del Material Editor. Se crea
    // post SceneRenderer para inyectarle el IBL compartido. Si la carga
    // del IBL del SceneRenderer fallo (try/catch silencioso en su ctor),
    // los handles seran nullptr y el preview cae a ambient escalar.
    m_materialPreview = std::make_unique<MaterialPreviewRenderer>(256u, 256u);
    m_materialPreview->setIblTextures(
        m_sceneRenderer->iblIrradiance(),
        m_sceneRenderer->iblPrefilter(),
        m_sceneRenderer->iblBrdfLut());
    m_ui.materialEditor().setPreviewRenderer(m_materialPreview.get());

    buildInitialTestMap();
    rebuildSceneFromMap();
    updateWindowTitle();

    // Hito 13 Bloque 2.5 + 3: overlay 2D sobre el viewport.
    // - En Editor Mode: iconos (Light/Audio), halo de seleccion, gizmo
    //   translate/rotate/scale, hotkeys W/E/R/Period y Delete (ver
    //   `EditorOverlay.cpp`).
    // - En Play Mode: HUD del juego + menu de pausa (ver `EditorPlayMode.cpp`).
    m_ui.viewport().setOverlayDraw(
        [this](ImDrawList* dl, float x0, float y0, float w, float h) {
            if (!m_scene) return;
            if (m_mode == EditorMode::Play) {
                drawGameOverlay(dl, x0, y0, w, h);
            } else {
                drawEditorOverlay(dl, x0, y0, w, h);
            }
        });

    MOOD_LOG_INFO("Editor listo");

    // Restaurar estado de la sesion anterior (ultimo proyecto, flags).
    // Silencioso si no hay archivo o si el ultimo proyecto desaparecio.
    loadEditorState();
}

void EditorApplication::updateWindowTitle() {
    std::string title = "MoodEngine Editor - v0.6.0-dev (Hito 6)";
    if (m_project.has_value()) {
        title = "MoodEngine Editor - " + m_project->name;
        if (m_projectDirty) title += " *";
    }
    SDL_SetWindowTitle(m_window->sdlHandle(), title.c_str());
    m_ui.setHasProject(m_project.has_value());
}

void EditorApplication::markDirty() {
    if (!m_project.has_value()) return;
    if (!m_projectDirty) {
        m_projectDirty = true;
        updateWindowTitle();
    }
}

// Project handlers (confirmDiscardChanges, handleNewProject, handleOpenProject,
// tryOpenProjectPath, addToRecentProjects, handleSave, handleSaveAs,
// handleCloseProject, loadEditorState, saveEditorState) movidos a
// `EditorProjectActions.cpp` (Hito 16 refactor).


EditorApplication::~EditorApplication() {
    // Persistir preferencias antes de tirar cualquier recurso. Falla silencio-
    // samente si no puede escribir — no queremos que un shutdown explote por
    // un archivo de estado corrupto.
    saveEditorState();

    // Los recursos GL dependen del contexto; liberar ANTES de destruir ImGui
    // y el contexto (el destructor del Window destruye el contexto al final).
    m_scriptSystem.reset(); // sol::state destructors; no dependen de GL
    // AudioSystem antes que AudioDevice: el system tiene &device pero no
    // toca el device en su destructor; stopAll lo llamamos antes via clear.
    m_audioSystem.reset();
    m_audioDevice.reset();
    m_animationSystem.reset();
    m_navSystem.reset();
    m_particleSystem.reset();
    m_physicsWorld.reset();
    m_assetManager.reset(); // dueño de texturas y meshes (incluido el cubo fallback)
    // F2H21: preview tiene refs (no-owning) a las IBL textures del
    // SceneRenderer. Liberar ANTES del SceneRenderer para evitar
    // dangling pointers en el dtor del preview.
    m_materialPreview.reset();
    // SceneRenderer destruye en orden inverso al ctor todos sus recursos
    // GL (FBs, shaders, IBL textures, debug renderer, etc.).
    m_sceneRenderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    gladLoaderUnloadGL();

    m_window.reset();
    SDL_Quit();

    MOOD_LOG_INFO("MoodEngine cerrado limpiamente");
    Log::shutdown();
}

void EditorApplication::processEvents() {
    MOOD_PROFILE_FUNCTION();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) {
            m_running = false;
        } else if (ev.type == SDL_WINDOWEVENT &&
                   ev.window.event == SDL_WINDOWEVENT_CLOSE &&
                   ev.window.windowID == SDL_GetWindowID(m_window->sdlHandle())) {
            m_running = false;
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_ESCAPE &&
                   m_mode == EditorMode::Play) {
            // Hito 20: Esc togglea menu de pausa. La sincronizacion del
            // cursor con SDL la hace `updateCameras` detectando la
            // transicion del flag (asi tambien funciona si lo cambia un
            // script Lua via `hud.setPaused`).
            GameState::paused() = !GameState::paused();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_F1 &&
                   ev.key.repeat == 0) {
            m_debugDraw = !m_debugDraw;
            Log::editor()->info("Debug draw {}", m_debugDraw ? "activado" : "desactivado");
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_s &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Ctrl+S: atajo de Guardar. Emitimos la misma solicitud que el menu
            // para reusar el dispatcher unico.
            m_ui.requestProjectAction(ProjectAction::Save);
        } else if (ev.type == SDL_KEYDOWN &&
                   (ev.key.keysym.sym == SDLK_DELETE ||
                    ev.key.keysym.sym == SDLK_BACKSPACE) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 13/20: Delete/Backspace borra la entidad seleccionada.
            // Via evento SDL en lugar de ImGui::IsKeyPressed para evitar
            // problemas de foco entre paneles. El filtro WantTextInput
            // evita borrar la entidad mientras el usuario edita un campo.
            deleteSelectedEntity();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_z &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 27: Ctrl+Z deshace el ultimo comando del history.
            m_history.undo();
        } else if (ev.type == SDL_KEYDOWN &&
                   ((ev.key.keysym.sym == SDLK_y &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0) ||
                    (ev.key.keysym.sym == SDLK_z &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                     (ev.key.keysym.mod & KMOD_SHIFT) != 0)) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 27: Ctrl+Y o Ctrl+Shift+Z rehace.
            m_history.redo();
        }
    }
}

void EditorApplication::beginFrame() {
    MOOD_PROFILE_FUNCTION();
    // F2H7: aplicar pending workspace switch ANTES de NewFrame.
    // LoadIniSettingsFromMemory no debe llamarse dentro de un frame
    // ImGui activo (segun la doc de ImGui).
    m_ui.applyPendingWorkspaceSwitch();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void EditorApplication::endFrame() {
    MOOD_PROFILE_FUNCTION();
    {
        MOOD_PROFILE_SCOPE("ImGui::Render");
        ImGui::Render();
    }

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        MOOD_PROFILE_SCOPE("ImGui_ImplOpenGL3_RenderDrawData");
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    {
        MOOD_PROFILE_SCOPE("swapBuffers");
        m_window->swapBuffers();
    }
}

glm::vec3 EditorApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

f32 EditorApplication::viewportAspect() const {
    const auto& fb = m_sceneRenderer->viewportFb();
    return (fb.height() > 0)
        ? static_cast<f32>(fb.width()) / static_cast<f32>(fb.height())
        : 1.0f;
}

// `renderSceneToViewport` orquesta SceneRenderer + overlay 3D del editor.
// La definicion vive en `EditorRenderPass.cpp` (Hito 21 Bloque 2).

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
            //   Shift+click   -> toggle.
            //   Ctrl+click    -> add (que ademas setea active).
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
                    if (keyShift) {
                        toggle(set, hit.entity);
                    } else if (keyCtrl) {
                        add(set, hit.entity);
                    } else {
                        replaceWithSingle(set, hit.entity);
                    }
                    if (hit.entity.hasComponent<TagComponent>()) {
                        Log::editor()->info("Click-select: '{}'",
                            hit.entity.getComponent<TagComponent>().name);
                    }
                } else if (!keyShift && !keyCtrl) {
                    clear(set);
                }
            }
        }

        // 2.5/2.6/2.7) Drops desde AssetBrowser (textura / mesh / prefab).
        // Hito 16: implementaciones movidas a  para
        // mantener  legible.
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
