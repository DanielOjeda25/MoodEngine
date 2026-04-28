#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/IMesh.h"
#include "engine/render/ITexture.h"
#include "engine/render/SceneRenderer.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLMesh.h"
#include "engine/render/opengl/OpenGLTexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "platform/Window.h"
#include "systems/PhysicsSystem.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL.
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

    // ImGui: igual que en el editor pero sin layout persistente. El HUD
    // del juego (Bloque 3 Fase B) usa ImDrawList, no panels.
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

    m_sceneRenderer = std::make_unique<SceneRenderer>();

    // AssetManager: misma factoria GL que el editor. En Bloque 4 esto
    // pasara a vivir en una funcion compartida con EditorApplication.
    m_assetManager = std::make_unique<AssetManager>(
        "assets",
        [](const std::string& fsPath) -> std::unique_ptr<ITexture> {
            return std::make_unique<OpenGLTexture>(fsPath);
        },
        AssetManager::AudioClipFactory{},
        [](const std::vector<f32>& verts,
           const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
            return std::make_unique<OpenGLMesh>(verts, attrs);
        });

    m_scene = std::make_unique<Scene>();

    // Capturar el mouse para FPS camera (relative mode = sin cursor visible
    // y deltas relativos en eventos de motion).
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GetRelativeMouseState(nullptr, nullptr);

    buildTestMap();
    rebuildSceneFromMap();

    MOOD_LOG_INFO("MoodPlayer listo");
}

PlayerApplication::~PlayerApplication() {
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

glm::vec3 PlayerApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

void PlayerApplication::buildTestMap() {
    // Mismo placeholder que arranca el editor: sala 8x8 perimetral con
    // grid.png + columna central con brick.png. Bloque 4 reemplaza esto
    // por la carga del `.moodmap` indicado en game.json.
    m_map = GridMap(8u, 8u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");

    for (u32 i = 0; i < m_map.width(); ++i) {
        m_map.setTile(i, 0u,                  TileType::SolidWall, grid);
        m_map.setTile(i, m_map.height() - 1u, TileType::SolidWall, grid);
    }
    for (u32 j = 0; j < m_map.height(); ++j) {
        m_map.setTile(0u,                 j, TileType::SolidWall, grid);
        m_map.setTile(m_map.width() - 1u, j, TileType::SolidWall, grid);
    }
    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("MoodPlayer: mapa de prueba 8x8 ({} tiles solidos)",
                        m_map.solidCount());
}

void PlayerApplication::rebuildSceneFromMap() {
    m_scene->registry().clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

    for (u32 y = 0; y < m_map.height(); ++y) {
        for (u32 x = 0; x < m_map.width(); ++x) {
            if (!m_map.isSolid(x, y)) continue;

            const std::string name = "Tile_" + std::to_string(x) + "_" + std::to_string(y);
            Entity e = m_scene->createEntity(name);

            auto& t = e.getComponent<TransformComponent>();
            t.position = glm::vec3(
                origin.x + (static_cast<f32>(x) + 0.5f) * tileSize,
                origin.y + 0.5f * tileSize,
                origin.z + (static_cast<f32>(y) + 0.5f) * tileSize);
            t.scale = glm::vec3(tileSize);

            const MaterialAssetId tileMat =
                m_assetManager->loadMaterialFromTexture(
                    m_map.tileTextureAt(x, y));
            e.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(), tileMat);
        }
    }
}

void PlayerApplication::processEvents() {
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
                   ev.key.keysym.sym == SDLK_ESCAPE) {
            // Bloque 3 Fase A: Esc cierra. Fase B lo conectara al menu
            // de pausa (toggle de GameState::paused).
            m_running = false;
        }
    }
}

void PlayerApplication::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void PlayerApplication::endFrame() {
    // Mostramos el viewport FB del SceneRenderer cubriendo toda la
    // ventana antes de Render() — `Image` es un widget mas y necesita
    // estar dentro del ImGui frame en curso. UV invertido vertical por
    // la convencion GL (origen bottom-left) vs ImGui (top-left).
    if (m_sceneRenderer) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##GameViewport", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);
        const ImVec2 size = ImGui::GetContentRegionAvail();
        ImGui::Image(m_sceneRenderer->viewportFb().colorAttachmentHandle(),
                      size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    ImGui::Render();

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    m_window->swapBuffers();
}

void PlayerApplication::updateCamera(f32 dt) {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    glm::vec3 dir(0.0f);
    if (keys[SDL_SCANCODE_W]) dir.z += 1.0f;
    if (keys[SDL_SCANCODE_S]) dir.z -= 1.0f;
    if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;
    if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
    if (keys[SDL_SCANCODE_SPACE])  dir.y += 1.0f;
    if (keys[SDL_SCANCODE_LSHIFT]) dir.y -= 1.0f;

    const glm::vec3 desired = m_playCamera.computeMoveDelta(dir, dt);
    if (desired.x != 0.0f || desired.y != 0.0f || desired.z != 0.0f) {
        const glm::vec3 camPos = m_playCamera.position();
        AABB playerBox{camPos - k_playerHalfExtents, camPos + k_playerHalfExtents};
        const glm::vec3 actual = moveAndSlide(m_map, mapWorldOrigin(), playerBox, desired);
        m_playCamera.translate(actual);
    }

    int mx = 0;
    int my = 0;
    SDL_GetRelativeMouseState(&mx, &my);
    if (mx != 0 || my != 0) {
        m_playCamera.applyMouseMove(static_cast<float>(mx), static_cast<float>(my));
    }
}

int PlayerApplication::run() {
    m_deltaTimer.reset();

    while (m_running) {
        const f64 dtD = m_deltaTimer.elapsedSeconds();
        const f32 dt  = static_cast<f32>(dtD);
        m_deltaTimer.reset();

        processEvents();
        beginFrame();

        updateCamera(dt);

        if (m_scene && m_assetManager && m_sceneRenderer) {
            int fbWidth = 0;
            int fbHeight = 0;
            SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
            const f32 aspect = (fbHeight > 0)
                ? static_cast<f32>(fbWidth) / static_cast<f32>(fbHeight)
                : 1.0f;
            const glm::mat4 view = m_playCamera.viewMatrix();
            const glm::mat4 projection = m_playCamera.projectionMatrix(aspect);
            const glm::vec3 cameraPos = m_playCamera.position();

            m_sceneRenderer->renderScene(*m_scene, *m_assetManager,
                                           view, projection, aspect, cameraPos,
                                           static_cast<u32>(fbWidth),
                                           static_cast<u32>(fbHeight));
            m_sceneRenderer->endFrame();
        }

        endFrame();
    }

    return 0;
}

} // namespace Mood
