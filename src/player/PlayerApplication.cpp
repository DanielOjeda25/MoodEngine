#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioDevice.h"
#include "engine/game/GameManifest.h"
#include "engine/game/GameOverlay.h"
#include "engine/game/GameState.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLMesh.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
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
#include "systems/ParticleSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/ScriptSystem.h"
#include "systems/TriggerSystem.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL.
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>
#include <portable-file-dialogs.h>

#include <algorithm>  // std::min/max para crouch lerp clamp
#include <cmath>      // std::sin para headbob
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/common.hpp>  // glm::mix
#include <glm/gtc/quaternion.hpp>  // Hito 41 fix-up: quat → euler para Transform

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
        // Material instance unico (paridad con EditorScene::rebuildSceneFromMap).
        const MaterialAssetId floorMat =
            m_assetManager->createMaterialFromTexture(floorTex);
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

            // Cada tile recibe su propio material instance (paridad con
            // EditorScene::rebuildSceneFromMap).
            const MaterialAssetId tileMat =
                m_assetManager->createMaterialFromTexture(
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
            // en EditorApplication). En el main menu lo ignoramos (no
            // hay nada que pausar).
            if (!m_inMainMenu) {
                GameState::paused() = !GameState::paused();
            }
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_F5 &&
                   !m_inMainMenu) {
            // Hito 38 B: F5 = quicksave a `<exeDir>/quicksave.moodsave`.
            // Solo durante el juego (en el menu no hay state que guardar).
            quickSave();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_F6 &&
                   !m_inMainMenu) {
            // Hito 41 fix-up: F6 = save as con file dialog (slot custom).
            saveAs();
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

        // Hito 38 B: si estamos en el main menu, dibujamos el menu en
        // lugar del HUD. El viewport sigue rendereando el mapa cargado
        // detras como background. Si !m_inMainMenu: HUD + pause overlay
        // normal — "Salir del juego" del pause overlay vuelve al menu
        // (no cierra la app — Quit se hace desde el main menu).
        if (m_inMainMenu) {
            drawMainMenu();
        } else {
            GameOverlay::draw(ImGui::GetWindowDrawList(),
                               imgPos.x, imgPos.y, imgSize.x, imgSize.y,
                               "Salir al menu",
                               [this]() {
                                   Log::engine()->info(
                                       "[MainMenu] Salir al menu -> back to MainMenu");
                                   m_inMainMenu = true;
                                   GameState::paused() = false;
                               });
        }

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
    if (!m_physicsWorld) return;

    // Hito 30: capsule del jugador. Standing total height = 1.8m;
    // crouching = 1.0m. eyeOffset (ojos respecto al CENTRO de capsule)
    // depende del shape activo.
    constexpr f32 k_charHalfHeightStand    = 0.5f;
    constexpr f32 k_charHalfHeightCrouch   = 0.1f;
    constexpr f32 k_charRadius             = 0.4f;
    constexpr f32 k_jumpVel                = 5.5f; // ~1.5m de altura
    constexpr f32 k_jumpCooldown           = 0.2f; // anti-hold
    // Hito 40 G: per-proyecto via .moodproj — capturados al cargar.
    const f32 k_coyoteWindow     = m_coyoteWindowSec;
    const f32 k_jumpBufferWindow = m_jumpBufferWindowSec;
    constexpr f32 k_walkSpeed              = 4.0f;
    constexpr f32 k_crouchSpeed            = 2.0f;

    // Lazy create del character en la pos actual de la camara.
    if (m_playerCharId == 0) {
        const f32 eyeStand = k_charHalfHeightStand + k_charRadius - 0.2f;
        const glm::vec3 camPos = m_playCamera.position();
        m_playerCharId = m_physicsWorld->createCharacter(
            camPos - glm::vec3(0.0f, eyeStand, 0.0f),
            k_charHalfHeightStand, k_charRadius);
    }

    // Input WASD → velocidad horizontal m/s; LCtrl = crouch; Space = jump.
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    glm::vec3 inputDir(0.0f);
    if (keys[SDL_SCANCODE_W]) inputDir.z += 1.0f;
    if (keys[SDL_SCANCODE_S]) inputDir.z -= 1.0f;
    if (keys[SDL_SCANCODE_D]) inputDir.x += 1.0f;
    if (keys[SDL_SCANCODE_A]) inputDir.x -= 1.0f;

    // Crouch toggle por hold. SetShape NO mueve el centro del char —
    // hay que ajustar manualmente para mantener la base al ras del
    // piso. El delta del centro = standHalf - crouchHalf = 0.4.
    const bool wantCrouch = keys[SDL_SCANCODE_LCTRL] != 0;
    constexpr f32 k_centerDelta = k_charHalfHeightStand - k_charHalfHeightCrouch;
    if (wantCrouch && !m_crouching) {
        // Crouch: shape primero, luego bajar el centro (la capsule
        // chiquita queda con su base al ras de donde estaba antes).
        if (m_physicsWorld->setCharacterShape(m_playerCharId,
                                                k_charHalfHeightCrouch,
                                                k_charRadius)) {
            const glm::vec3 p = m_physicsWorld->characterPosition(m_playerCharId);
            m_physicsWorld->setCharacterPosition(m_playerCharId,
                p - glm::vec3(0.0f, k_centerDelta, 0.0f));
            m_crouching = true;
        }
    } else if (!wantCrouch && m_crouching) {
        // Stand up: subir el centro PRIMERO (preparar espacio para el
        // shape grande). Despues intentar setShape; si techo bloquea,
        // revertir el centro y seguir crouched.
        const glm::vec3 p = m_physicsWorld->characterPosition(m_playerCharId);
        m_physicsWorld->setCharacterPosition(m_playerCharId,
            p + glm::vec3(0.0f, k_centerDelta, 0.0f));
        if (m_physicsWorld->setCharacterShape(m_playerCharId,
                                                k_charHalfHeightStand,
                                                k_charRadius)) {
            m_crouching = false;
        } else {
            // Techo encima — revertir y seguir crouched.
            m_physicsWorld->setCharacterPosition(m_playerCharId, p);
        }
    }

    const f32 speed = m_crouching ? k_crouchSpeed : k_walkSpeed;
    glm::vec3 horizVel(0.0f);
    if (glm::length(inputDir) > 1e-4f) {
        const glm::vec3 fwd = m_playCamera.forward();
        const glm::vec3 fwdFlat = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
        const glm::vec3 right = glm::normalize(glm::cross(fwdFlat, glm::vec3(0, 1, 0)));
        glm::vec3 dir = right * inputDir.x + fwdFlat * inputDir.z;
        if (glm::length(dir) > 1e-4f) {
            dir = glm::normalize(dir);
            horizVel = dir * speed;
        }
    }

    // Jump: si SPACE + onGround + cooldown OK, usamos desired.y como
    // impulse. El step interno suma desired.y a la velocidad acumulada.
    // Hito 34 C: coyote + jump buffer (mismo patron que EditorPlayMode).
    const bool spacePressed = keys[SDL_SCANCODE_SPACE] != 0;
    const bool spaceJustPressed = spacePressed && !m_spacePrevFrame;
    m_spacePrevFrame = spacePressed;
    if (spaceJustPressed) m_jumpBufferTimer = k_jumpBufferWindow;
    m_jumpBufferTimer = std::max(0.0f, m_jumpBufferTimer - dt);

    if (m_physicsWorld->isCharacterOnGround(m_playerCharId)) {
        m_coyoteTimer = k_coyoteWindow;
    } else {
        m_coyoteTimer = std::max(0.0f, m_coyoteTimer - dt);
    }

    m_jumpCooldown = (m_jumpCooldown > dt) ? (m_jumpCooldown - dt) : 0.0f;
    f32 jumpImpulse = 0.0f;
    if (m_jumpBufferTimer > 0.0f
        && m_coyoteTimer > 0.0f
        && m_jumpCooldown <= 0.0f
        && !m_crouching) {
        jumpImpulse = k_jumpVel;
        m_jumpCooldown = k_jumpCooldown;
        m_jumpBufferTimer = 0.0f;
        m_coyoteTimer = 0.0f;
    }

    m_physicsWorld->setCharacterMovement(m_playerCharId,
        glm::vec3(horizVel.x, jumpImpulse, horizVel.z));

    // Hito 31 D: crouch lerp visual + headbob (paridad con
    // EditorApplication::updatePlayMode). El sync de la camara con
    // la pos del char vive en updateRigidBodies; aca solo avanzamos
    // los acumuladores que esa sync consume.
    const f32 crouchTarget = m_crouching ? 1.0f : 0.0f;
    const f32 lerpRate = 5.0f * dt;
    if (m_crouchVisualT < crouchTarget) {
        m_crouchVisualT = std::min(crouchTarget, m_crouchVisualT + lerpRate);
    } else if (m_crouchVisualT > crouchTarget) {
        m_crouchVisualT = std::max(crouchTarget, m_crouchVisualT - lerpRate);
    }
    const f32 horizSpeedSq = horizVel.x * horizVel.x + horizVel.z * horizVel.z;
    const bool walking = horizSpeedSq > 0.01f
                      && m_physicsWorld->isCharacterOnGround(m_playerCharId);
    if (walking) m_headbobTime += dt;
    // Hito 34 D: velocity-scaled bob amp.
    const f32 horizSpeed = std::sqrt(horizSpeedSq);
    m_horizSpeed01 = walking
        ? std::min(1.0f, horizSpeed / k_walkSpeed)
        : 0.0f;

    // Mouse aim (independiente del char).
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
            // Hito 41 fix-up #2: convertir t.rotationEuler (degrees) a
            // quat XYZW para que el body materializado preserve la
            // rotation del Transform (critico post-Load Game).
            const glm::quat q = glm::quat(glm::radians(t.rotationEuler));
            rb.bodyId = m_physicsWorld->createBody(t.position, shape,
                                                    rb.halfExtents, type, rb.mass,
                                                    rb.friction,
                                                    glm::vec4(q.x, q.y, q.z, q.w));
            // Si applyLoadedSave dejo velocidades pending (porque al
            // momento del load el body no estaba materializado), las
            // aplicamos ahora y limpiamos el flag.
            if (rb.hasPendingVel && rb.bodyId != 0) {
                m_physicsWorld->setBodyLinearVelocity(rb.bodyId, rb.pendingLinearVel);
                m_physicsWorld->setBodyAngularVelocity(rb.bodyId, rb.pendingAngularVel);
                rb.hasPendingVel = false;
                rb.pendingLinearVel = glm::vec3(0.0f);
                rb.pendingAngularVel = glm::vec3(0.0f);
            }
        });

    // Stepear simulacion + sync body -> Transform para dinamicos.
    if (!GameState::paused()) {
        m_physicsWorld->step(dt);
        m_scene->forEach<TransformComponent, RigidBodyComponent>(
            [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
                if (rb.type == RigidBodyComponent::Type::Static) return;
                if (rb.bodyId == 0) return;
                // Hito 41 fix-up: sync de POSITION + ROTATION desde el
                // body. Antes solo position — el Transform quedaba con
                // rotation stale, causando que F1 debug draw, saves, y
                // exports mostraran la rotation original del .moodmap
                // en lugar de la del body simulando.
                glm::vec4 quat;
                t.position = m_physicsWorld->bodyPositionRot(rb.bodyId, quat);
                const glm::quat q(quat.w, quat.x, quat.y, quat.z);
                t.rotationEuler = glm::degrees(glm::eulerAngles(q));
            });

        // Hito 30: sync cámara con la pos del character post-step.
        // Hito 31 D: eye Y interpola con m_crouchVisualT + headbob.
        if (m_playerCharId != 0) {
            constexpr f32 k_eyeStanding = 0.5f + 0.4f - 0.2f;  // 0.7
            constexpr f32 k_eyeCrouched = 0.1f + 0.4f - 0.2f;  // 0.3
            const f32 eye = glm::mix(k_eyeStanding, k_eyeCrouched, m_crouchVisualT);
            constexpr f32 k_bobFreq = 5.0f * 6.2831853f;
            constexpr f32 k_bobAmp  = 0.04f;
            // Hito 34 D: la amplitud escala con la velocidad horizontal
            // del frame [0..1]. Caminando full-speed = bob completo;
            // crouched (~0.5) = bob sutil; quieto = sin bob.
            const f32 bobY = std::sin(m_headbobTime * k_bobFreq) * k_bobAmp
                              * m_horizSpeed01;
            const glm::vec3 charPos = m_physicsWorld->characterPosition(m_playerCharId);
            m_playCamera.setPosition(charPos + glm::vec3(0.0f, eye + bobY, 0.0f));
        }
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

        // Hito 38 B: durante el main menu skipeamos todos los updates
        // del juego para que el viewport quede congelado como
        // background detras del menu (sin char movimiento ni
        // particulas avanzando). El render del scene SI se hace mas
        // abajo — vemos el ultimo state como wallpaper.
        // Truco: forzamos GameState::paused() = true en menu para que
        // los sistemas pause-aware (nav, particles) se detengan sin
        // tocar su lógica. Los que no son pause-aware (scripts,
        // animation) los guardamos explicitamente.
        const bool gameUpdating = !m_inMainMenu;
        // Hito 41 fix-up: sync de cursor SDL con el flag de menu.
        // - En menu: cursor visible (SDL_FALSE).
        // - Al salir del menu (New Game / Load Game): forzar captura
        //   (SDL_TRUE). El updateCamera tiene su propio sync vs `paused`
        //   pero el flag prev es stale despues de muchos frames sin que
        //   updateCamera corra → confiamos en este toggle por flank.
        static bool prevMainMenu = true;
        if (m_inMainMenu) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        } else if (prevMainMenu) {
            // Flank true→false: salimos del menu hacia el juego.
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_GetRelativeMouseState(nullptr, nullptr);  // descartar delta acumulado
            // Reset del flag pause-cursor sync para el proximo updateCamera.
            m_pausedLastFrame = false;
        }
        prevMainMenu = m_inMainMenu;
        if (gameUpdating) {
            updateCamera(dt);
            updateRigidBodies(dt);
        } else {
            GameState::paused() = true;  // freeze pause-aware systems
        }

        // Scripts + animation + audio: igual orden que el editor.
        // Cuando paused, scripts y animation siguen corriendo (para
        // que un script pueda mostrar UI o ejecutar logica de menu),
        // pero el audio sigue su flow normal — TBD si se quiere mute
        // global en pausa.
        if (gameUpdating && m_scene && m_scriptSystem) {
            m_scriptSystem->update(*m_scene, dt, m_physicsWorld.get());
        }
        // Hito 33: triggers — solo cuando el char del player ya existe.
        if (gameUpdating && m_scene && m_scriptSystem && m_physicsWorld
            && m_playerCharId != 0) {
            m_triggerSystem.update(*m_scene, *m_physicsWorld,
                                    *m_scriptSystem, m_playerCharId);
        }
        if (gameUpdating && m_scene && m_animationSystem && m_assetManager) {
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

        // Particulas (Hito 29). Pause-aware: si el menu esta abierto, no
        // avanzan (consistente con fisica + nav).
        if (m_scene && m_particleSystem && !GameState::paused()) {
            m_particleSystem->update(*m_scene, dt);
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

// === Hito 38 B: Main Menu ===

namespace {
// Path al exe del MoodPlayer (donde escribimos quicksave + leemos al
// inicio). SDL_GetBasePath devuelve string allocado — wrapper RAII.
std::filesystem::path exeBaseDir() {
    char* p = SDL_GetBasePath();
    if (p == nullptr) return std::filesystem::path{};
    std::filesystem::path out(p);
    SDL_free(p);
    return out;
}
} // namespace

void PlayerApplication::drawMainMenu() {
    // Hito 41 fix-up: estilo del editor — overlay oscuro completo,
    // panel mas grande, padding generoso, titulo grande.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(vp->Pos,
                       ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                       IM_COL32(0, 0, 0, 230));

    // Panel modal centrado mas amplio.
    constexpr float k_panelW = 420.0f;
    constexpr float k_panelH = 360.0f;
    ImGui::SetNextWindowPos(
        ImVec2(vp->Pos.x + vp->Size.x * 0.5f - k_panelW * 0.5f,
               vp->Pos.y + vp->Size.y * 0.5f - k_panelH * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(k_panelW, k_panelH));

    // Estilo: padding generoso + bordes redondeados + colores oscuros
    // del tipo editor.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.30f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.42f, 0.62f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.62f, 0.85f, 1.0f));

    ImGui::Begin("##MainMenu", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoSavedSettings);

    // Titulo grande centrado (escala 1.8x con la font default).
    ImGui::SetWindowFontScale(2.0f);
    const ImVec2 titleSize = ImGui::CalcTextSize("MoodEngine");
    ImGui::SetCursorPosX((k_panelW - titleSize.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 1.0f, 1.0f));
    ImGui::TextUnformatted("MoodEngine");
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    constexpr ImVec2 k_btn(-FLT_MIN, 48.0f);
    if (ImGui::Button("New Game", k_btn)) {
        Log::engine()->info("[MainMenu] New Game -> reload mapa default + reset state");
        // Hito 41 fix-up: New Game debe RECARGAR el mapa default
        // desde el .moodproj — sin esto el state runtime acumulado
        // (cajas movidas, hud editado, scripts con globals) se queda
        // entre partidas. Reset profesional: cleanup de bodies Jolt
        // + char + scripts, despues tryLoadGameManifest reconstruye.

        // 1) Cleanup de Jolt: destruir char + rigid bodies dynamic
        //    activos. Static se recrean cuando rebuildSceneFromMap
        //    spawnea las entidades-tile.
        if (m_physicsWorld) {
            if (m_playerCharId != 0) {
                m_physicsWorld->destroyCharacter(m_playerCharId);
                m_playerCharId = 0;
            }
            if (m_scene) {
                m_scene->forEach<RigidBodyComponent>(
                    [&](Entity, RigidBodyComponent& rb) {
                        if (rb.bodyId != 0) {
                            m_physicsWorld->destroyBody(rb.bodyId);
                            rb.bodyId = 0;
                        }
                    });
            }
        }

        // 2) Cleanup scripts: tira los sol::states (preserva sin
        //    huerfanos cuando registry().clear() invalida entt::entity).
        if (m_scriptSystem) m_scriptSystem->clear();

        // 3) Recargar el mapa default. tryLoadGameManifest() interno:
        //    - load del .moodproj + default_map .moodmap.
        //    - rebuildSceneFromMap (registry.clear + tiles + floor).
        //    - applyEntitiesToScene (recrea entidades persistidas).
        if (!tryLoadGameManifest()) {
            // Fallback al sample map (proyecto sin game.json).
            buildTestMap();
        }

        // 4) Reset GameState (hud + paused).
        GameState::reset();

        // 5) Reset cámara a la pos inicial. La pos default del
        //    constructor es la del FpsCamera member-init list.
        m_playCamera = FpsCamera(glm::vec3(-4.5f, 1.6f, 7.5f), -90.0f, 0.0f);

        m_inMainMenu = false;
    }
    if (ImGui::Button("Load Game", k_btn)) {
        // pfd::open_file devuelve un vector<string>; vacio = cancel.
        const auto results = pfd::open_file(
            "Cargar partida",
            (exeBaseDir()).generic_string(),
            { "MoodSave (*.moodsave)", "*.moodsave",
              "All Files", "*" }).result();
        if (!results.empty()) {
            const std::filesystem::path savePath(results[0]);
            const auto data = SaveLoad::load(savePath);
            if (data.has_value()) {
                applyLoadedSave(*data);
                m_inMainMenu = false;
            } else {
                Log::engine()->warn(
                    "MainMenu: no se pudo cargar '{}' (ver warning anterior)",
                    savePath.generic_string());
            }
        }
    }
    if (ImGui::Button("Quit", k_btn)) {
        Log::engine()->info("[MainMenu] Quit -> exiting MoodPlayer");
        m_running = false;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    // Atajos de teclado en gris suave, centrados.
    const char* hint = "F5 quicksave  |  F6 save as...  |  Esc menu pausa";
    const ImVec2 hintSize = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPosX((k_panelW - hintSize.x) * 0.5f);
    ImGui::TextDisabled("%s", hint);

    ImGui::End();
    ImGui::PopStyleColor(4);  // WindowBg + Button{,Hovered,Active}
    ImGui::PopStyleVar(5);    // padding/rounding/frameRound/frameBorder/itemSpacing
}

void PlayerApplication::applyLoadedSave(const SaveLoad::SaveData& data) {
    // V1: no cambiamos de mapa. Si el data.mapPath no coincide con el
    // map activo, logueamos warning pero seguimos aplicando el state
    // sobre el map actual — es responsabilidad del caller del menu
    // pre-cargar el map correcto si tiene saves de varios niveles.
    if (!data.mapPath.empty() && !m_currentMapPath.empty() &&
        data.mapPath != m_currentMapPath.generic_string()) {
        Log::engine()->warn(
            "Load: el .moodsave referencia '{}' pero el map actual es '{}' — "
            "se aplica el state sobre el map actual",
            data.mapPath, m_currentMapPath.generic_string());
    }

    GameState::reset();
    GameState::hud() = data.hud;

    if (m_physicsWorld && m_playerCharId != 0) {
        m_physicsWorld->setCharacterPosition(m_playerCharId, data.playerPosition);
    }
    m_playCamera.setOrientation(data.playerYaw, data.playerPitch);
    m_playCamera.setPosition(data.playerPosition + glm::vec3(0.0f, 0.7f, 0.0f));

    // Hito 41 A + fix-up: aplicar snapshots de bodies. Buscamos por tag
    // (estable entre sesiones, bodyId es volatil).
    //
    // FIX RAIZ: aplicamos SIEMPRE al `TransformComponent` (que existe
    // sin importar si el body fue materializado). Si el body ya existe
    // (Play Mode activo), tambien al body para que linear/angular vel
    // se preserven. Si NO existe (load desde main menu antes del primer
    // Play), updateRigidBodies materializara el body en el siguiente
    // frame leyendo del Transform — y como el Transform tiene la pose
    // del save, el body queda en la pose correcta.
    int bodiesApplied = 0;
    if (m_scene && !data.bodies.empty()) {
        std::unordered_map<std::string, const SaveLoad::BodySnapshot*> byTag;
        byTag.reserve(data.bodies.size());
        for (const auto& b : data.bodies) {
            // Hito 41 fix: warn ante tags duplicados (V1 limitation: el
            // primero gana; el dev deberia spawnear con tags unicos
            // como hace `Agregar caja fisica demo` desde Hito 41).
            if (byTag.count(b.entityTag) > 0) {
                Log::engine()->warn(
                    "[Load] tag duplicado en .moodsave: '{}' — solo se aplicara el primer snapshot",
                    b.entityTag);
                continue;
            }
            byTag[b.entityTag] = &b;
        }

        // Hito 41 fix-up #2: tracking de cuales tags del save matchean
        // entidades del scene. Lo usamos para loguear los huerfanos al
        // final (snapshots cuya entidad NO existe en el .moodmap actual).
        std::unordered_map<std::string, bool> tagMatched;
        for (const auto& kv : byTag) tagMatched[kv.first] = false;

        m_scene->forEach<TagComponent, TransformComponent, RigidBodyComponent>(
            [&](Entity, TagComponent& tag, TransformComponent& tf, RigidBodyComponent& rb) {
                if (rb.type != RigidBodyComponent::Type::Dynamic) return;
                auto it = byTag.find(tag.name);
                if (it == byTag.end()) return; // entity sin snapshot — OK silencioso
                const auto& snap = *it->second;
                tagMatched[tag.name] = true;

                // 1) SIEMPRE: aplicar al TransformComponent. Cubre el
                //    caso "load desde main menu antes del primer Play"
                //    (body NO materializado todavia → updateRigidBodies
                //    leera Transform + pending vels en el siguiente frame).
                tf.position = snap.position;
                const glm::quat q(snap.rotationQuat.w, snap.rotationQuat.x,
                                   snap.rotationQuat.y, snap.rotationQuat.z);
                tf.rotationEuler = glm::degrees(glm::eulerAngles(q));

                // 2) Si el body YA esta materializado (Play activo),
                //    aplicar al body en vivo. Si no, stash en
                //    pendingVel — updateRigidBodies las aplica al
                //    materializar.
                if (rb.bodyId != 0 && m_physicsWorld) {
                    Log::engine()->info(
                        "    [LOAD] body tag='{}' bodyId={} pos=({:.2f},{:.2f},{:.2f}) [Transform+Body+Vel]",
                        tag.name, rb.bodyId,
                        snap.position.x, snap.position.y, snap.position.z);
                    m_physicsWorld->setBodyPositionRot(
                        rb.bodyId, snap.position, snap.rotationQuat);
                    m_physicsWorld->setBodyLinearVelocity(
                        rb.bodyId, snap.linearVelocity);
                    m_physicsWorld->setBodyAngularVelocity(
                        rb.bodyId, snap.angularVelocity);
                } else {
                    Log::engine()->info(
                        "    [LOAD] body tag='{}' bodyId=0 pos=({:.2f},{:.2f},{:.2f}) [Transform + pendingVel — body se materializara con pose+vel]",
                        tag.name,
                        snap.position.x, snap.position.y, snap.position.z);
                    rb.hasPendingVel = true;
                    rb.pendingLinearVel = snap.linearVelocity;
                    rb.pendingAngularVel = snap.angularVelocity;
                }
                ++bodiesApplied;
            });

        // Snapshots huerfanos: el save tiene tags que NO existen en el
        // scene actual. Tipicamente significa que las entidades viven
        // en runtime (spawneadas via "Agregar caja fisica demo" sin
        // Ctrl+S al .moodmap antes de empaquetar). El usuario lo nota
        // como "las cajas no aparecen al hacer Load Game" — el log
        // explica la causa raiz.
        for (const auto& [tag, matched] : tagMatched) {
            if (!matched) {
                Log::engine()->warn(
                    "[Load] snapshot huerfano: tag='{}' del .moodsave NO matchea ninguna entity Dynamic en el scene. "
                    "Probablemente la entidad fue spawneada en runtime y nunca persistida al .moodmap "
                    "(Ctrl+S en el editor antes de empaquetar).",
                    tag);
            }
        }
    }
    Log::engine()->info(
        "[Load] Applied {}/{} body snapshots (tag matched), hud hp={}, player @ ({:.2f},{:.2f},{:.2f})",
        bodiesApplied, data.bodies.size(),
        data.hud.hp,
        data.playerPosition.x, data.playerPosition.y, data.playerPosition.z);

    // Hito 41 B: restaurar globals Lua. Si un script aun no se cargo,
    // ScriptSystem::restoreGlobals los stash en pendingGlobals.
    int globalsApplied = 0;
    if (m_scene && m_scriptSystem && !data.scriptGlobals.empty()) {
        std::unordered_map<std::string, const SaveLoad::ScriptGlobalsSnapshot*> byPath;
        byPath.reserve(data.scriptGlobals.size());
        for (const auto& sg : data.scriptGlobals) byPath[sg.scriptPath] = &sg;

        m_scene->forEach<ScriptComponent>(
            [&](Entity e, ScriptComponent& sc) {
                auto it = byPath.find(sc.path);
                if (it == byPath.end()) return;
                m_scriptSystem->restoreGlobals(e.handle(), it->second->globals);
                ++globalsApplied;
            });
    }
    Log::engine()->info(
        "[Load] Restored {} script globals snapshots (path matched)",
        globalsApplied);
}

SaveLoad::SaveData PlayerApplication::captureCurrentState() {
    SaveLoad::SaveData d;
    d.mapPath        = m_currentMapPath.generic_string();
    d.hud            = GameState::hud();
    if (m_physicsWorld && m_playerCharId != 0) {
        d.playerPosition = m_physicsWorld->characterPosition(m_playerCharId);
    } else {
        d.playerPosition = m_playCamera.position();
    }
    d.playerYaw   = m_playCamera.yawDeg();
    d.playerPitch = m_playCamera.pitchDeg();

    Log::engine()->info("[Save] capturando state...");
    Log::engine()->info("  - hud: hp={}, ammo={}", d.hud.hp, d.hud.ammo);
    Log::engine()->info("  - player pos=({:.2f},{:.2f},{:.2f}) yaw={:.1f} pitch={:.1f}",
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z,
        d.playerYaw, d.playerPitch);

    // Hito 41 A: capturar snapshots de Dynamic bodies activos.
    if (m_scene && m_physicsWorld) {
        Log::engine()->info("  - capturing Dynamic body snapshots...");
        m_scene->forEach<TagComponent, RigidBodyComponent>(
            [&](Entity, TagComponent& tag, RigidBodyComponent& rb) {
                if (rb.type != RigidBodyComponent::Type::Dynamic) return;
                if (rb.bodyId == 0) return;
                if (tag.name.empty()) return;  // sin tag estable, skip
                SaveLoad::BodySnapshot b;
                b.entityTag = tag.name;
                glm::vec4 quat;
                b.position = m_physicsWorld->bodyPositionRot(rb.bodyId, quat);
                b.rotationQuat = quat;
                b.linearVelocity  = m_physicsWorld->bodyLinearVelocity(rb.bodyId);
                b.angularVelocity = m_physicsWorld->bodyAngularVelocity(rb.bodyId);
                // Hito 41 fix-up: clamp velocidades casi-cero a exactamente
                // cero. Sin esto un body que estaba dormido (sleeping)
                // pero con epsilon de velocity guardada, al cargar se
                // re-activa y empieza a moverse/rotar lentamente.
                // Threshold ~ 0.01 m/s — bajo el umbral de Jolt sleeping.
                constexpr f32 k_velEpsilon = 0.01f;
                if (glm::length(b.linearVelocity) < k_velEpsilon) {
                    b.linearVelocity = glm::vec3(0.0f);
                }
                if (glm::length(b.angularVelocity) < k_velEpsilon) {
                    b.angularVelocity = glm::vec3(0.0f);
                }
                Log::engine()->info(
                    "    [SAVE] body tag='{}' bodyId={} pos=({:.2f},{:.2f},{:.2f}) quat=({:.3f},{:.3f},{:.3f},{:.3f}) linVel=({:.2f},{:.2f},{:.2f})",
                    tag.name, rb.bodyId,
                    b.position.x, b.position.y, b.position.z,
                    quat.x, quat.y, quat.z, quat.w,
                    b.linearVelocity.x, b.linearVelocity.y, b.linearVelocity.z);
                d.bodies.push_back(std::move(b));
            });
        Log::engine()->info("  - {} body snapshots captured", d.bodies.size());
    }

    // Hito 41 B: capturar globals Lua per script.
    if (m_scene && m_scriptSystem) {
        Log::engine()->info("  - capturing Lua script globals...");
        m_scene->forEach<ScriptComponent>(
            [&](Entity e, ScriptComponent& sc) {
                if (sc.path.empty() || !sc.loaded) return;
                Log::engine()->debug("    script path='{}'", sc.path);
                auto globals = m_scriptSystem->captureGlobals(e.handle());
                if (globals.empty()) return;
                SaveLoad::ScriptGlobalsSnapshot sg;
                sg.scriptPath = sc.path;
                sg.globals    = std::move(globals);
                d.scriptGlobals.push_back(std::move(sg));
            });
        Log::engine()->info("  - {} scripts with globals captured", d.scriptGlobals.size());
    }

    return d;
}

void PlayerApplication::quickSave() {
    const auto d = captureCurrentState();
    const auto savePath = exeBaseDir() / "quicksave.moodsave";
    if (SaveLoad::save(d, savePath)) {
        Log::engine()->info("Quicksave OK -> '{}'", savePath.generic_string());
    } else {
        Log::engine()->warn("Quicksave fallo -> '{}'", savePath.generic_string());
    }
}

void PlayerApplication::saveAs() {
    const auto d = captureCurrentState();
    // pfd::save_file abre dialog "Guardar como" con default_path en el
    // exe dir y filtro `.moodsave`. Si el user cancela, retorna "".
    const auto chosen = pfd::save_file(
        "Guardar partida como...",
        (exeBaseDir() / "partida.moodsave").generic_string(),
        { "MoodSave (*.moodsave)", "*.moodsave" }).result();
    if (chosen.empty()) {
        Log::engine()->info("[SaveAs] cancelado por el usuario");
        return;
    }
    std::filesystem::path savePath(chosen);
    // Si el usuario no agrego extension, ponemos .moodsave por nosotros.
    if (savePath.extension() != ".moodsave") {
        savePath += ".moodsave";
    }
    if (SaveLoad::save(d, savePath)) {
        Log::engine()->info("[SaveAs] OK -> '{}'", savePath.generic_string());
    } else {
        Log::engine()->warn("[SaveAs] fallo -> '{}'", savePath.generic_string());
    }
}

} // namespace Mood
