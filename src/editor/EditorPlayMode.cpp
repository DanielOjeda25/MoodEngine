#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/game/GameOverlay.h"
#include "engine/game/GameState.h"
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
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 dir(0.0f);
        if (keys[SDL_SCANCODE_W]) dir.z += 1.0f;
        if (keys[SDL_SCANCODE_S]) dir.z -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;
        if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
        if (keys[SDL_SCANCODE_SPACE]) dir.y += 1.0f;
        if (keys[SDL_SCANCODE_LSHIFT]) dir.y -= 1.0f;

        // Movimiento: delta deseado -> colisiones -> delta aplicado.
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
