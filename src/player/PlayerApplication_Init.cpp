// F2H24: boot + shutdown del MoodPlayer.
// PlayerApplication() — init de SDL/GL/ImGui + sistemas runtime + carga
//   inicial (game.json o fallback al test map).
// ~PlayerApplication() — teardown en orden inverso al ctor.
// tryLoadGameManifest() — busca game.json adyacente al .exe y carga el
//   proyecto referenciado.

#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "core/UserSettings.h"  // F2H43
#include "engine/assets/manager/AssetManager.h"
#include "engine/dialog/DialogScriptHost.h"  // F2H48.1
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/audio/device/AudioDevice.h"
#include "engine/game/manifest/GameManifest.h"
#include "engine/game/state/GameState.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLMesh.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "platform/Window.h"
#include "systems/animation/AnimationSystem.h"
#include "systems/audio/AudioSystem.h"
#include "systems/ai/NavSystem.h"
#include "systems/particles/ParticleSystem.h"
#include "systems/scripting/ScriptSystem.h"

#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace Mood {

namespace {
constexpr const char* k_GlslVersion = "#version 450 core";
} // namespace

PlayerApplication::PlayerApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodPlayer iniciando...");

    // F2H43: cargar idioma persistido (compartido con MoodEditor via
    // %APPDATA%\MoodEngine\settings.json) y arrancar I18n.
    UserSettings::init();
    I18n::init(UserSettings::language());
    Dialog::DialogScriptHost::init();  // F2H48.1

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodPlayer";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    if (!gladLoaderLoadGL()) {
        throw std::runtime_error("gladLoaderLoadGL fallo: no se pudo cargar OpenGL");
    }
    Log::render()->info("OpenGL iniciado: {}",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    // F2H45 Bloque B: Lato como default font (paridad con el editor desde
    // F2H38). Pre-fix usaba ProggyClean (bitmap 13px Basic Latin) — el HUD
    // (HEALTH / MUNICION / etc) era pixely y `MUNICION` no podia llevar
    // tilde en `MUNICIÓN` por charset limitado. Rango: Basic Latin +
    // Latin-1 Supplement (acentos espanol) + General Punctuation
    // (em-dash, ellipsis). FA NO mergeado: el HUD del Player usa DrawList
    // procedural (F2H39+), no necesita iconos.
    {
        ImFontAtlas* atlas = io.Fonts;
        static const ImWchar k_latoRange[] = {
            0x0020, 0x00FF, // Basic Latin + Latin-1 Sup
            0x2010, 0x2027, // General Punctuation subset
            0
        };
        ImFontConfig latoCfg;
        latoCfg.PixelSnapH = false;
        atlas->AddFontFromFileTTF(
            "assets/ui/fonts/LatoLatin-Regular.ttf",
            15.0f,
            &latoCfg,
            k_latoRange);
    }

    m_sceneRenderer = std::make_unique<SceneRenderer>();

    m_assetManager = std::make_unique<AssetManager>(
        "assets",
        [](const std::string& fsPath) -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(fsPath);
        },
        AssetManager::AudioClipFactory{},
        [](const std::vector<f32>& verts,
           const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
            return std::make_unique<OpenGLMesh>(verts, attrs);
        },
        // Hito 26: TextureMemoryFactory para texturas embedded en .glb.
        [](const std::vector<u8>& bytes, const std::string& nameForLog)
            -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(bytes, nameForLog);
        });

    m_scene = std::make_unique<Scene>();

    // Sistemas runtime: mismos que el editor usa en Play Mode.
    m_scriptSystem    = std::make_unique<ScriptSystem>();
    m_audioDevice     = std::make_unique<AudioDevice>();
    m_audioSystem     = std::make_unique<AudioSystem>(*m_audioDevice, *m_assetManager);
    m_animationSystem = std::make_unique<AnimationSystem>();
    m_navSystem       = std::make_unique<NavSystem>();
    m_particleSystem  = std::make_unique<ParticleSystem>();
    m_physicsWorld    = std::make_unique<PhysicsWorld>();

    // Player arranca en Play Mode directo: cursor capturado por SDL,
    // sin pausa.
    GameState::reset();
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GetRelativeMouseState(nullptr, nullptr);

    // Intentamos cargar `game.json` adyacente al .exe. Si no existe o
    // hay cualquier problema, caemos al mapa de prueba — util mientras
    // el Bloque 5 (PackageBuilder) no produzca paquetes reales.
    if (!tryLoadGameManifest()) {
        Log::engine()->info(
            "MoodPlayer: sin game.json valido, cargando mapa de prueba placeholder");
        buildTestMap();
        rebuildSceneFromMap();
    }

    // F2H52 G: inyectar scene + assets al DialogScriptHost para que las
    // choices con `condition_lua = "inventory.has(...)"` funcionen
    // contra el inventario REAL del player. El player corre todo Play
    // Mode desde el inicio (no hay exit), asi que esto persiste.
    Dialog::DialogScriptHost::setSceneAndAssets(
        m_scene.get(), m_assetManager.get());

    if (m_sceneRenderer && m_scene) {
        // Aplicar Environment del proyecto YA, en lugar de esperar al
        // primer renderScene. Asi la primera frame muestra fog/exposure/
        // tonemap correctos sin un flash a defaults.
        m_sceneRenderer->applyEnvironmentFromScene(*m_scene);
    }

    MOOD_LOG_INFO("MoodPlayer listo (gameplay activo: WASD + mouse, Esc para pausar)");
}

bool PlayerApplication::tryLoadGameManifest() {
    // Buscamos `game.json` junto al .exe (NO al working dir — que puede
    // variar entre VS, cmd, o doble-click). SDL_GetBasePath devuelve el
    // dir absoluto del .exe con trailing slash; el caller debe liberar
    // la memoria con SDL_free.
    char* base = SDL_GetBasePath();
    const std::filesystem::path exeDir = base
        ? std::filesystem::path(base)
        : std::filesystem::current_path();
    if (base) SDL_free(base);
    const auto manifestPath = exeDir / "game.json";
    auto manifest = GameManifest::loadFromFile(manifestPath);
    if (!manifest) return false;

    Log::engine()->info("GameManifest: '{}' (proyecto='{}')",
                         manifest->name.empty() ? "<sin nombre>" : manifest->name,
                         manifest->projectRelative.generic_string());

    // Resolver paths: project relativo al manifest dir, mapa relativo
    // al directorio del .moodproj.
    const auto manifestDir = manifestPath.parent_path();
    const auto moodprojPath = manifestDir / manifest->projectRelative;

    auto loaded = ProjectSerializer::load(moodprojPath);
    if (!loaded.has_value() || loaded->maps.empty()) {
        Log::engine()->warn(
            "GameManifest: proyecto invalido o sin mapas en '{}'",
            moodprojPath.generic_string());
        return false;
    }

    const auto mapRel = manifest->defaultMapRelative.empty()
        ? loaded->defaultMap
        : manifest->defaultMapRelative;
    const auto mapPath = loaded->root / mapRel;

    auto savedMap = SceneSerializer::load(mapPath, *m_assetManager);
    if (!savedMap.has_value()) {
        Log::engine()->warn(
            "GameManifest: no se pudo cargar mapa default '{}'",
            mapPath.generic_string());
        return false;
    }

    m_map = std::move(savedMap->map);
    rebuildSceneFromMap();
    // F2H26: pasar useCompiledMesh=true para que el Player use la mesh
    // estatica unificada del .moodmap (si existe) en lugar de procesar
    // los brushes CSG individuales en runtime. Habilita "brushes solo
    // en el editor" del plan original F2H14.
    SceneLoader::applyEntitiesToScene(*savedMap, *m_scene, *m_assetManager,
                                       /*useCompiledMesh=*/true);
    m_currentMapPath = mapRel;  // Hito 38 B: para que SaveLoad lo persista.
    // Hito 40 G: char controller windows del proyecto.
    m_coyoteWindowSec     = loaded->coyoteWindowSec;
    m_jumpBufferWindowSec = loaded->jumpBufferWindowSec;

    Log::engine()->info(
        "MoodPlayer: proyecto '{}' cargado ({} entidades persistidas)",
        loaded->name, savedMap->entities.size());
    return true;
}

PlayerApplication::~PlayerApplication() {
    // Orden inverso al ctor. Sistemas primero (algunos dependen de
    // m_scene / m_assetManager / m_audioDevice), luego el scene, luego
    // los recursos GL via SceneRenderer, luego el contexto GL.
    m_scriptSystem.reset();
    m_audioSystem.reset();
    m_audioDevice.reset();
    m_animationSystem.reset();
    m_navSystem.reset();
    m_particleSystem.reset();
    m_physicsWorld.reset();
    m_scene.reset();
    m_assetManager.reset();
    m_sceneRenderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    gladLoaderUnloadGL();

    m_window.reset();
    SDL_Quit();

    MOOD_LOG_INFO("MoodPlayer cerrado limpiamente");
    Log::shutdown();
}

} // namespace Mood
