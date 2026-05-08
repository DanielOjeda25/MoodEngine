// F2H24: boot + shutdown del MoodEditor.
// glDebugCallback — receptor de mensajes del debug context GL (solo
//   builds Debug).
// EditorApplication() — init de SDL/GL/ImGui + sistemas runtime +
//   inyeccion de paneles + load del editor state previo.
// ~EditorApplication() — teardown en orden inverso al ctor.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/audio/device/AudioDevice.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/preview/MaterialPreviewRenderer.h"
#include "engine/render/rhi/IFramebuffer.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLMesh.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/scene/core/Scene.h"
#include "systems/animation/AnimationSystem.h"
#include "systems/audio/AudioSystem.h"
#include "systems/ai/NavSystem.h"
#include "systems/particles/ParticleSystem.h"
#include "systems/scripting/ScriptSystem.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL
// (evita redefiniciones con <SDL_opengl.h> o los loaders internos).
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Mood {

namespace {

constexpr const char* k_GlslVersion = "#version 450 core";

// Callback que recibe el driver para cada warning/error GL. Filtra por
// severidad y rutea al logger `render` con el nivel equivalente. Solo se
// activa si el contexto fue creado con el debug bit (builds Debug).
// Usamos GLAD_API_PTR (definido siempre por glad/gl.h) en lugar de
// APIENTRY directo para no depender de que <windows.h> haya sido
// incluido transitivamente por algun otro header.
void GLAD_API_PTR glDebugCallback(GLenum /*source*/, GLenum type, GLuint id,
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
    // F2H25: layout version stamping. ImGui guarda window positions y
    // dock targets en este ini. Cuando agregamos paneles nuevos al
    // dockspace (ej. F2H22 agrego "Tools", F2H23 reorganizo columnas),
    // los ini viejos no tienen settings para esos panels → caen como
    // flotantes en el primer arranque post-update. Bumpear el sufijo
    // ("_v2", "_v3", ...) hace que ImGui ignore el ini viejo y caiga
    // al layout default que `Dockspace::buildLayoutForWorkspace`
    // construye. Las customizaciones que el user haga DESPUES se
    // persisten en el archivo nuevo y sobreviven entre sesiones. Solo
    // se pierden cuando bumpeamos esta version (decision deliberada
    // del dev: "por defecto la UI es fija, luego el user acomoda").
    io.IniFilename = "imgui_layout_v2.ini";

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

} // namespace Mood
