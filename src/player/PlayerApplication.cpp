#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioDevice.h"
#include "engine/game/GameManifest.h"
#include "engine/game/GameOverlay.h"
#include "engine/game/GameState.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/render/IMesh.h"
#include "engine/render/ITexture.h"
#include "engine/render/SceneRenderer.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLMesh.h"
#include "engine/render/opengl/OpenGLTexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/serialization/SceneLoader.h"
#include "engine/serialization/SceneSerializer.h"
#include "platform/Window.h"
#include "systems/AnimationSystem.h"
#include "systems/AudioSystem.h"
#include "systems/NavSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/ScriptSystem.h"

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

    // Sistemas runtime: mismos que el editor usa en Play Mode.
    m_scriptSystem    = std::make_unique<ScriptSystem>();
    m_audioDevice     = std::make_unique<AudioDevice>();
    m_audioSystem     = std::make_unique<AudioSystem>(*m_audioDevice, *m_assetManager);
    m_animationSystem = std::make_unique<AnimationSystem>();
    m_navSystem       = std::make_unique<NavSystem>();
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
    SceneLoader::applyEntitiesToScene(*savedMap, *m_scene, *m_assetManager);

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

glm::vec3 PlayerApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

void PlayerApplication::buildTestMap() {
    // Sala abierta 16x16 (mismo placeholder que el editor: piso + 1
    // columna brick). Sin muros perimetrales.
    m_map = GridMap(16u, 16u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");
    (void)grid;

    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("MoodPlayer: arena 16x16 ({} tiles solidos)",
                        m_map.solidCount());
}

void PlayerApplication::rebuildSceneFromMap() {
    m_scene->registry().clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

    // Piso plano (mismo patron que EditorApplication::rebuildSceneFromMap).
    {
        const f32 mapW = static_cast<f32>(m_map.width())  * tileSize;
        const f32 mapH = static_cast<f32>(m_map.height()) * tileSize;
        const TextureAssetId floorTex =
            m_assetManager->loadTexture("textures/grid.png");
        Entity floor = m_scene->createEntity("Floor");
        auto& tf = floor.getComponent<TransformComponent>();
        tf.position = glm::vec3(
            origin.x + mapW * 0.5f,
            origin.y - 0.05f,
            origin.z + mapH * 0.5f);
        tf.scale = glm::vec3(mapW, 0.1f, mapH);
        const MaterialAssetId floorMat =
            m_assetManager->loadMaterialFromTexture(floorTex);
        floor.addComponent<MeshRendererComponent>(
            m_assetManager->missingMeshId(), floorMat);
        floor.addComponent<RigidBodyComponent>(
            RigidBodyComponent::Type::Static,
            RigidBodyComponent::Shape::Box,
            glm::vec3(mapW * 0.5f, 0.05f, mapH * 0.5f),
            0.0f);
    }

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

            // RigidBody static para los tiles (mismo flow que el editor):
            // permite que entidades dinamicas (cajas spawnedas via script)
            // colisionen contra las paredes.
            e.addComponent<RigidBodyComponent>(
                RigidBodyComponent::Type::Static,
                RigidBodyComponent::Shape::Box,
                glm::vec3(tileSize * 0.5f),
                0.0f);
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
            // Esc: togglea pausa. El sync de cursor con SDL lo hace
            // updateCamera al detectar la transicion del flag (igual que
            // en EditorApplication).
            GameState::paused() = !GameState::paused();
        }
    }
}

void PlayerApplication::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void PlayerApplication::endFrame() {
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
        const ImVec2 imgPos  = ImGui::GetCursorScreenPos();
        const ImVec2 imgSize = ImGui::GetContentRegionAvail();
        ImGui::Image(m_sceneRenderer->viewportFb().colorAttachmentHandle(),
                      imgSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        // HUD + menu de pausa por encima del Image, en el mismo
        // drawlist de la ventana. "Salir del juego" cierra la app.
        GameOverlay::draw(ImGui::GetWindowDrawList(),
                           imgPos.x, imgPos.y, imgSize.x, imgSize.y,
                           "Salir del juego",
                           [this]() { m_running = false; });

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
    // Sync cursor con paused, igual que en EditorApplication.
    const bool nowPaused = GameState::paused();
    if (nowPaused != m_pausedLastFrame) {
        if (nowPaused) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        } else {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_GetRelativeMouseState(nullptr, nullptr);
        }
        m_pausedLastFrame = nowPaused;
    }
    if (nowPaused) return;

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

void PlayerApplication::updateRigidBodies(f32 dt) {
    if (!m_scene || !m_physicsWorld) return;

    // Materializar bodies nuevos.
    m_scene->forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
            if (rb.bodyId != 0) return;
            CollisionShape shape = CollisionShape::Box;
            switch (rb.shape) {
                case RigidBodyComponent::Shape::Box:     shape = CollisionShape::Box; break;
                case RigidBodyComponent::Shape::Sphere:  shape = CollisionShape::Sphere; break;
                case RigidBodyComponent::Shape::Capsule: shape = CollisionShape::Capsule; break;
            }
            BodyType type = BodyType::Static;
            switch (rb.type) {
                case RigidBodyComponent::Type::Static:    type = BodyType::Static; break;
                case RigidBodyComponent::Type::Kinematic: type = BodyType::Kinematic; break;
                case RigidBodyComponent::Type::Dynamic:   type = BodyType::Dynamic; break;
            }
            rb.bodyId = m_physicsWorld->createBody(t.position, shape,
                                                    rb.halfExtents, type, rb.mass);
        });

    // Stepear simulacion + sync body -> Transform para dinamicos.
    if (!GameState::paused()) {
        m_physicsWorld->step(dt);
        m_scene->forEach<TransformComponent, RigidBodyComponent>(
            [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
                if (rb.type == RigidBodyComponent::Type::Static) return;
                if (rb.bodyId == 0) return;
                t.position = m_physicsWorld->bodyPosition(rb.bodyId);
            });
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
        updateRigidBodies(dt);

        // Scripts + animation + audio: igual orden que el editor.
        // Cuando paused, scripts y animation siguen corriendo (para
        // que un script pueda mostrar UI o ejecutar logica de menu),
        // pero el audio sigue su flow normal — TBD si se quiere mute
        // global en pausa.
        if (m_scene && m_scriptSystem) {
            m_scriptSystem->update(*m_scene, dt);
        }
        if (m_scene && m_animationSystem && m_assetManager) {
            m_animationSystem->update(*m_scene, *m_assetManager, dt);
        }

        // Hito 23: NavAgents persiguen al jugador. El target se updatea
        // antes del NavSystem.update — sin pause check porque el player
        // no tiene Editor Mode, todo el tiempo es "Play".
        if (m_scene && m_navSystem && !GameState::paused()) {
            const glm::vec3 playerPos = m_playCamera.position();
            m_scene->forEach<NavAgentComponent>(
                [&](Entity, NavAgentComponent& nav) {
                    nav.target = playerPos;
                });
            m_navSystem->update(*m_scene, m_map, mapWorldOrigin(), dt);
        }
        if (m_scene && m_audioSystem) {
            m_audioSystem->update(*m_scene, dt);
            // Listener = camara FPS del player. World-up fijo (0,1,0).
            m_audioSystem->setListener(
                m_playCamera.position(),
                m_playCamera.forward(),
                glm::vec3(0.0f, 1.0f, 0.0f));
        }

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
