// F2H24: loop principal del MoodPlayer.
// processEvents — SDL events: quit, window close, Esc (pausa), F5/F6
//   (quicksave/saveAs).
// beginFrame / endFrame — ImGui frame + render del viewport con HUD o
//   main menu encima.
// updateCamera — char controller (WASD/crouch/jump) + mouse aim.
// updateRigidBodies — materializar bodies + step + sync body→Transform
//   + sync camera con char.
// run() — loop while(m_running).

#include "player/PlayerApplication.h"

#include <cmath>  // F2H40: std::abs en pase de re-sync de halfExtents

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/game/overlay/GameOverlay.h"
#include "engine/dialog/DialogInteractSystem.h"  // F2H48
#include "engine/dialog/DialogSystem.h"          // F2H48
#include "engine/game/state/GameState.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
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

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Mood {

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
            const std::string exitLbl = I18n::T("hud.menu.exit_to_menu");
            GameOverlay::draw(ImGui::GetWindowDrawList(),
                               imgPos.x, imgPos.y, imgSize.x, imgSize.y,
                               ImGui::GetIO().DeltaTime,
                               m_playCamera.forward(),
                               exitLbl.c_str(),
                               [this]() {
                                   Log::engine()->info(
                                       "[MainMenu] Exit to menu -> back to MainMenu");
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
    // F2H48: lock de input mientras hay dialog activo (paridad con
    // EditorPlayMode). Mouse-look queda libre.
    const bool dialogLocked = Mood::GameState::dialogActive();
    glm::vec3 inputDir(0.0f);
    if (!dialogLocked) {
        if (keys[SDL_SCANCODE_W]) inputDir.z += 1.0f;
        if (keys[SDL_SCANCODE_S]) inputDir.z -= 1.0f;
        if (keys[SDL_SCANCODE_D]) inputDir.x += 1.0f;
        if (keys[SDL_SCANCODE_A]) inputDir.x -= 1.0f;
    }

    // Crouch toggle por hold. SetShape NO mueve el centro del char —
    // hay que ajustar manualmente para mantener la base al ras del
    // piso. El delta del centro = standHalf - crouchHalf = 0.4.
    const bool wantCrouch = !dialogLocked && keys[SDL_SCANCODE_LCTRL] != 0;
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
    const bool spacePressed = !dialogLocked && keys[SDL_SCANCODE_SPACE] != 0;
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
            // F2H40: trackear el halfExtents inicial sincronizado al body.
            if (rb.bodyId != 0) {
                rb.lastSyncedHalfExtents = rb.halfExtents;
            }
        });

    // F2H40: pase de re-sync para bodies ya materializados (paridad
    // con `EditorApplication::updateRigidBodies`). En el Player es
    // menos comun que el dev cambie scale en runtime — pero los
    // saves cargados pueden tener Floor con scale enlargado y body
    // pre-existente, y los scripts Lua pueden mutar Transform.
    m_scene->forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
            if (rb.bodyId == 0) return;
            constexpr f32 k_eps = 1e-4f;
            if (rb.shape == RigidBodyComponent::Shape::Box) {
                const glm::vec3 desired = t.scale * 0.5f;
                if (std::abs(desired.x - rb.halfExtents.x) > k_eps ||
                    std::abs(desired.y - rb.halfExtents.y) > k_eps ||
                    std::abs(desired.z - rb.halfExtents.z) > k_eps) {
                    rb.halfExtents = desired;
                }
            }
            if (std::abs(rb.halfExtents.x - rb.lastSyncedHalfExtents.x) > k_eps ||
                std::abs(rb.halfExtents.y - rb.lastSyncedHalfExtents.y) > k_eps ||
                std::abs(rb.halfExtents.z - rb.lastSyncedHalfExtents.z) > k_eps) {
                CollisionShape shape = CollisionShape::Box;
                switch (rb.shape) {
                    case RigidBodyComponent::Shape::Box:     shape = CollisionShape::Box;     break;
                    case RigidBodyComponent::Shape::Sphere:  shape = CollisionShape::Sphere;  break;
                    case RigidBodyComponent::Shape::Capsule: shape = CollisionShape::Capsule; break;
                }
                m_physicsWorld->setBodyHalfExtents(rb.bodyId, shape, rb.halfExtents);
                rb.lastSyncedHalfExtents = rb.halfExtents;
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
            m_scriptSystem->update(*m_scene, dt, m_physicsWorld.get(),
                                    m_assetManager.get());  // F2H48: dialog bindings
        }
        // Hito 33: triggers — solo cuando el char del player ya existe.
        if (gameUpdating && m_scene && m_scriptSystem && m_physicsWorld
            && m_playerCharId != 0) {
            m_triggerSystem.update(*m_scene, *m_physicsWorld,
                                    *m_scriptSystem, m_playerCharId);
        }

        // F2H48: DialogInteractSystem (paridad con EditorPlayMode).
        // Detecta tecla E + player-inside-trigger + DialogComponent.
        if (gameUpdating && m_scene && m_assetManager) {
            const Uint8* frameKeys = SDL_GetKeyboardState(nullptr);
            const bool ePressed = frameKeys[SDL_SCANCODE_E] != 0;
            const bool eJustPressed = ePressed && !m_ePrevFrame;
            m_ePrevFrame = ePressed;
            const bool dialogStarted = Mood::Dialog::DialogInteractSystem::tick(
                *m_scene, *m_assetManager, eJustPressed);

            if (!dialogStarted && Mood::Dialog::DialogSystem::isActive()) {
                int digitJustPressed = -1;
                for (int i = 0; i < 9; ++i) {
                    const bool pressed = frameKeys[SDL_SCANCODE_1 + i] != 0;
                    if (pressed && !m_digitPrevFrame[i]) {
                        digitJustPressed = i + 1;
                    }
                    m_digitPrevFrame[i] = pressed;
                }
                Mood::Dialog::DialogInteractSystem::tickActiveDialog(
                    eJustPressed, digitJustPressed);
            } else {
                for (int i = 0; i < 9; ++i) m_digitPrevFrame[i] = false;
            }
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

        MOOD_PROFILE_FRAME();
    }

    return 0;
}

} // namespace Mood
