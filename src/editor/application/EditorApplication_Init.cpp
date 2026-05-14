// F2H24: boot + shutdown del MoodEditor.
// glDebugCallback — receptor de mensajes del debug context GL (solo
//   builds Debug).
// EditorApplication() — init de SDL/GL/ImGui + sistemas runtime +
//   inyeccion de paneles + load del editor state previo.
// ~EditorApplication() — teardown en orden inverso al ctor.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/UserSettings.h"  // F2H43
#include "engine/audio/device/AudioDevice.h"
#include "engine/dialog/DialogScriptHost.h"  // F2H48.1
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/inventory/InventoryHooks.h"  // F2H53 shutdown order
#include "engine/quest/QuestSystem.h"         // F2H53 shutdown order
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

    // F2H43: cargar idioma persistido (default Spanish si no hay archivo)
    // y arrancar I18n con ese idioma como activo. Fallback siempre Ingles.
    UserSettings::init();
    I18n::init(UserSettings::language());

    // F2H48.1: inicializar el DialogScriptHost — registra los hooks
    // evaluator/executor en DialogSystem para que los condition_lua /
    // on_select_lua de las choices se evaluen contra una sol::state
    // dedicada (compartida con todos los scripts del juego). Sin
    // este wireup, los hooks se ignoran silenciosamente (paridad con
    // tests headless).
    Dialog::DialogScriptHost::init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodEngine Editor - v0.4.0-dev (Hito 4)";
    // F2H35: arrancar a la resolucion real del escritorio (no 1280x720
    // + maximize). Razon: SDL_WINDOW_MAXIMIZED solo encola el resize y
    // el primer SDL_GetWindowSize (que ImGui usa en NewFrame) devolvia
    // las dimensiones que pasamos al CreateWindow (1280x720), no las
    // reales maximizadas. El primer Dockspace::rebuildLayout calculaba
    // los splits a 1280x720 y los persistia como offsets absolutos en
    // el ini — quedaban torcidos cuando la ventana ya era 1920x1080.
    // Crear la ventana directamente al tamano del desktop garantiza que
    // SDL_GetWindowSize devuelva el tamano correcto desde el frame 0.
    SDL_DisplayMode dm{};
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        spec.width  = static_cast<u32>(dm.w);
        spec.height = static_cast<u32>(dm.h);
    } else {
        spec.width  = 1280;
        spec.height = 720;
    }
    spec.maximized = true;  // titulo bar muestra "restore" + respeta taskbar
    m_window = std::make_unique<Window>(spec);
    // F2H35 defensivo: bombear eventos pendientes (en particular el
    // SDL_WINDOWEVENT_SHOWN/RESIZED de la creacion) para que el state
    // SDL este completamente sincronizado antes del primer NewFrame.
    SDL_PumpEvents();

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
    // F2H46: el debug-check de IDs conflictivos (ImGui 1.92+) genera
    // false-positives con imgui-node-editor (la lib maneja su propio
    // ID stack interno y dispara la alarma sin que sea bug real).
    // Lo desactivamos globalmente — nuestra suite de tests cubre lo
    // que este check detectaria en codigo nuestro (8580 assertions).
    io.ConfigDebugHighlightIdConflicts = false;
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
    // F2H35: bump v2 -> v3. El arranque maximizado del Bloque B cambia
    // las dimensiones iniciales del viewport (era 1280x720, ahora cubre
    // toda la pantalla). Layouts persistidos en `imgui_layout_v2.ini`
    // tenian splits con tamanos absolutos calculados a 1280x720 y al
    // arrancar maximizado quedaban descuadrados (el panel "Top (XZ)"
    // del workspace "Editor de mapas" comia el 60% del ancho dejando
    // al "Viewport" amontonado). Bump fuerza fresh layout que el
    // DockBuilder calcula con las dimensiones reales de la ventana.
    // F2H46: v5 — workspace Narrativa con Sandbox 70% / Intro 30%.
    // F2H47: bump v5 -> v6. Workspace Narrativa rediseñado con
    // Dialog Editor central + Inspector right + Browser/Intro bottom.
    // Layouts v5 mostrarian estos panels nuevos flotantes.
    // F2H51: bump v6 -> v7. Workspace "Gameplay" nuevo con Item Browser
    // + Item Property Editor + Inspector. Layouts v6 los mostrarian
    // flotantes en lugar de docked.
    // F2H52 D fix: bump v7 -> v8. Workspace Gameplay reorganizado a 4
    // columnas (Item Browser / Viewport / Property Editor / Inspector).
    // Inspector ahora al costado del Property Editor en lugar de abajo
    // — ambos importantes y siempre visibles.
    io.IniFilename = "imgui_layout_v8.ini";

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    // F2H38: cambiar el default font de ProggyClean (bitmap 13px,
    // pixely, solo Basic Latin) a Lato Latin Regular (sans-serif TTF
    // 15px, smooth, Latin completo + General Punctuation). Lato vive
    // en `assets/ui/fonts/LatoLatin-Regular.ttf` desde antes pero
    // nunca se cargaba. Todos los paneles del editor heredan la
    // mejora de legibilidad — Console (text-heavy) es donde mas se
    // nota. Side effect: el em-dash U+2014 ahora rendea (Lato cubre
    // General Punctuation), pero el fix `—`->`-` de F2H37 queda en
    // su lugar (es approach mas robusto, no depende del font).
    //
    // F2H36 (FA merge) seguia activo. ImGui 1.92 obliga a que la
    // font primaria y la mergeada compartan convencion de "reference
    // size" — pre-F2H38 ambas eran implicit (AddFontDefault + 0.0f);
    // ahora que Lato carga con size explicito (15.0f), la FA tambien
    // pasa a explicito (13.0f, ~85% del texto = icons proporcionales
    // al cuerpo de Lato sin dominarlo).
    //
    // Si los TTF fallan en cargar (ej. assets/ no en el deploy),
    // ImGui tira assert al intentar Build() del atlas. Aceptable:
    // sin assets el editor no arranca en general.
    {
        ImFontAtlas* atlas = io.Fonts;

        // Lato como font primaria. Range custom: Basic Latin +
        // Latin-1 Supplement (espanol) + General Punctuation
        // (em-dash, en-dash, ellipsis, comillas curvas).
        static const ImWchar k_latoRange[] = {
            0x0020, 0x00FF, // Basic Latin + Latin-1 Sup
            0x2010, 0x2027, // General Punctuation subset
            0
        };
        ImFontConfig latoCfg;
        latoCfg.PixelSnapH = false; // smooth scaling para sans-serif
        atlas->AddFontFromFileTTF(
            "assets/ui/fonts/LatoLatin-Regular.ttf",
            15.0f,
            &latoCfg,
            k_latoRange);

        // FA mergeada al Lato. Mismo size convention (explicit) para
        // satisfacer el assert de ImGui 1.92. Tamano del icon a 13px
        // = ~85% del texto de Lato — proporcional al cuerpo sin
        // dominar el label.
        ImFontConfig iconCfg;
        iconCfg.MergeMode = true;
        iconCfg.PixelSnapH = true;
        static const ImWchar k_iconRange[] = { 0xE005, 0xF8FF, 0 };
        atlas->AddFontFromFileTTF(
            "assets/ui/fonts/fa-solid-900.ttf",
            13.0f,
            &iconCfg,
            k_iconRange);
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
    // F2H42: sync el checkbox VSync del Performance panel con el estado real
    // del Window (puede haber sido rechazado por el driver al crearse).
    m_ui.performanceHud().setVsyncState(m_window->vsyncEnabled());

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

    // F2H53: limpiar hooks globales ANTES de destruir el ScriptSystem.
    // Los hooks (Quest evaluator/executor/on_start/etc + Inventory pickup/
    // drop/use/render) capturan `sol::function` o `sol::state*` del Lua
    // VM del ScriptSystem. Si la sol::state muere primero, al terminar el
    // programa los `std::function` globales (g_onStart, g_evaluator, etc.)
    // se destruyen y dentro ~sol::function llama `lua_unref` sobre un VM
    // ya muerto -> SIGSEGV en shutdown. Mismo bug que arreglamos en
    // ~InvLuaFixture (F2H52 J). DialogSystem ya se autolimpia en
    // DialogScriptHost::reset() que el destructor del host invoca.
    Mood::Quest::QuestSystem::clearHooks();
    Mood::Inventory::Hooks::clearAll();

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
