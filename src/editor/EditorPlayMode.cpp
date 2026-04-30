#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/game/GameOverlay.h"
#include "engine/game/GameState.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/ViewportPick.h"
#include "engine/world/GridMap.h"
#include "platform/Window.h"
#include "systems/PhysicsSystem.h"

#include <SDL.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Mood {

void EditorApplication::enterPlayMode() {
    m_mode = EditorMode::Play;
    m_ui.setMode(EditorMode::Play);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    // Descartar cualquier delta acumulado para evitar un salto inicial de la vista.
    SDL_GetRelativeMouseState(nullptr, nullptr);
    // Reset del tracker de pausa: arrancamos sin pausa, asi que la
    // primera transicion seria paused=true y el sync mostrara el cursor.
    m_pausedLastFrame = false;
    Log::editor()->info("Play Mode activo (WASD + mouse. Esc para pausar)");
}

void EditorApplication::exitPlayMode() {
    m_mode = EditorMode::Editor;
    m_ui.setMode(EditorMode::Editor);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    // Resetea HUD + pause al baseline para que el proximo Play Mode
    // arranque limpio.
    GameState::reset();
    // Hito 30: destruir el character al salir de Play Mode. La proxima
    // entrada lo recrea en la pos de spawn de la camara. Reseteamos
    // tambien jumpCooldown + crouching para que un futuro Play arranque
    // standing.
    if (m_physicsWorld && m_playerCharId != 0) {
        m_physicsWorld->destroyCharacter(m_playerCharId);
        m_playerCharId = 0;
    }
    m_jumpCooldown = 0.0f;
    m_crouching    = false;
    Log::editor()->info("Editor Mode activo");
}

void EditorApplication::updateCameras(f32 dt) {
    if (m_mode == EditorMode::Editor) {
        const float dx = m_ui.viewport().cameraRotateDx();
        const float dy = m_ui.viewport().cameraRotateDy();
        const float panDx = m_ui.viewport().cameraPanDx();
        const float panDy = m_ui.viewport().cameraPanDy();
        const float wheel = m_ui.viewport().cameraWheel();
        if (dx != 0.0f || dy != 0.0f) m_editorCamera.applyMouseDrag(dx, dy);
        if (panDx != 0.0f || panDy != 0.0f) m_editorCamera.applyPan(panDx, panDy);
        if (wheel != 0.0f) m_editorCamera.applyWheel(wheel);
    } else {
        // Sync cursor con paused. Detectamos la transicion (no llamamos
        // SDL cada frame) para soportar todos los caminos que cambian la
        // pausa: tecla Esc, click en "Continuar", o un script Lua via
        // `hud.setPaused`. SDL_SetRelativeMouseMode(FALSE) muestra el
        // cursor; (TRUE) lo atrapa para gameplay.
        const bool nowPaused = GameState::paused();
        if (nowPaused != m_pausedLastFrame) {
            if (nowPaused) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
            } else {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_GetRelativeMouseState(nullptr, nullptr); // descartar delta
            }
            m_pausedLastFrame = nowPaused;
        }

        // Hito 20: con el menu de pausa visible el gameplay se
        // congela — no leemos input ni actualizamos posicion del
        // jugador. El cursor esta libre para clickear botones.
        if (nowPaused) return;
        if (!m_physicsWorld) return;

        // Hito 30: shape standing/crouching + lazy create.
        constexpr f32 k_charHalfHeightStand  = 0.5f;
        constexpr f32 k_charHalfHeightCrouch = 0.1f;
        constexpr f32 k_charRadius           = 0.4f;
        constexpr f32 k_jumpVel              = 5.5f;
        constexpr f32 k_jumpCooldown         = 0.2f;
        constexpr f32 k_walkSpeed            = 4.0f;
        constexpr f32 k_crouchSpeed          = 2.0f;
        if (m_playerCharId == 0) {
            const f32 eyeStand = k_charHalfHeightStand + k_charRadius - 0.2f;
            const glm::vec3 camPos = m_playCamera.position();
            m_playerCharId = m_physicsWorld->createCharacter(
                camPos - glm::vec3(0.0f, eyeStand, 0.0f),
                k_charHalfHeightStand, k_charRadius);
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 inputDir(0.0f);
        if (keys[SDL_SCANCODE_W]) inputDir.z += 1.0f;
        if (keys[SDL_SCANCODE_S]) inputDir.z -= 1.0f;
        if (keys[SDL_SCANCODE_D]) inputDir.x += 1.0f;
        if (keys[SDL_SCANCODE_A]) inputDir.x -= 1.0f;

        // Crouch toggle. SetShape NO mueve el centro del char — ajusta
        // pos manual para mantener base al ras (mismo patron que en
        // PlayerApplication).
        const bool wantCrouch = keys[SDL_SCANCODE_LCTRL] != 0;
        constexpr f32 k_centerDelta = k_charHalfHeightStand - k_charHalfHeightCrouch;
        if (wantCrouch && !m_crouching) {
            if (m_physicsWorld->setCharacterShape(m_playerCharId,
                                                    k_charHalfHeightCrouch,
                                                    k_charRadius)) {
                const glm::vec3 p = m_physicsWorld->characterPosition(m_playerCharId);
                m_physicsWorld->setCharacterPosition(m_playerCharId,
                    p - glm::vec3(0.0f, k_centerDelta, 0.0f));
                m_crouching = true;
            }
        } else if (!wantCrouch && m_crouching) {
            const glm::vec3 p = m_physicsWorld->characterPosition(m_playerCharId);
            m_physicsWorld->setCharacterPosition(m_playerCharId,
                p + glm::vec3(0.0f, k_centerDelta, 0.0f));
            if (m_physicsWorld->setCharacterShape(m_playerCharId,
                                                    k_charHalfHeightStand,
                                                    k_charRadius)) {
                m_crouching = false;
            } else {
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

        m_jumpCooldown = (m_jumpCooldown > dt) ? (m_jumpCooldown - dt) : 0.0f;
        f32 jumpImpulse = 0.0f;
        if (keys[SDL_SCANCODE_SPACE]
            && m_jumpCooldown <= 0.0f
            && !m_crouching
            && m_physicsWorld->isCharacterOnGround(m_playerCharId)) {
            jumpImpulse = k_jumpVel;
            m_jumpCooldown = k_jumpCooldown;
        }

        m_physicsWorld->setCharacterMovement(m_playerCharId,
            glm::vec3(horizVel.x, jumpImpulse, horizVel.z));

        int mx = 0;
        int my = 0;
        SDL_GetRelativeMouseState(&mx, &my);
        if (mx != 0 || my != 0) {
            m_playCamera.applyMouseMove(static_cast<float>(mx), static_cast<float>(my));
        }
    }
}

void EditorApplication::drawGameOverlay(ImDrawList* dl,
                                          float x0, float y0,
                                          float w, float h) {
    // Hito 21 Bloque 3 Fase B: el overlay vive en `engine/game/GameOverlay`
    // para que el editor (Play Mode) y `MoodPlayer` lo compartan.
    GameOverlay::draw(dl, x0, y0, w, h, "Salir al editor",
                       [this]() { exitPlayMode(); });
}

} // namespace Mood
