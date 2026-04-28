#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "core/math/AABB.h"
#include "engine/game/GameState.h"
#include "engine/scene/ViewportPick.h"
#include "engine/world/GridMap.h"
#include "platform/Window.h"
#include "systems/PhysicsSystem.h"

#include <SDL.h>
#include <imgui.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cstdio>

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
    // Convencion: TODAS las coords del overlay son screen-space del
    // panel Viewport (origen top-left en x0,y0). El drawlist clip rect
    // ya esta seteado al area del Image por el ViewportPanel.

    // --- Crosshair ---
    {
        const float cx = x0 + w * 0.5f;
        const float cy = y0 + h * 0.5f;
        const float r  = 6.0f;
        const ImU32 col = IM_COL32(255, 255, 255, 220);
        const ImU32 outline = IM_COL32(0, 0, 0, 220);
        // Cruz con outline (negra atras, blanca encima — legible
        // contra cualquier escena).
        dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx + r, cy), outline, 3.0f);
        dl->AddLine(ImVec2(cx, cy - r), ImVec2(cx, cy + r), outline, 3.0f);
        dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx + r, cy), col,     1.5f);
        dl->AddLine(ImVec2(cx, cy - r), ImVec2(cx, cy + r), col,     1.5f);
    }

    // --- HUD blocks (HP + AMMO) ---
    auto drawHudBlock = [&](float bx, float by, const char* label, int value) {
        constexpr float bw = 140.0f;
        constexpr float bh = 70.0f;
        const ImU32 bg     = IM_COL32(0, 0, 0, 180);
        const ImU32 border = IM_COL32(255, 255, 255, 100);
        const ImU32 lblCol = IM_COL32(220, 220, 220, 220);
        const ImU32 valCol = IM_COL32(255, 255, 255, 255);

        const ImVec2 a(bx, by);
        const ImVec2 b(bx + bw, by + bh);
        dl->AddRectFilled(a, b, bg, 4.0f);
        dl->AddRect(a, b, border, 4.0f, 0, 2.0f);

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", value);

        const ImVec2 lblSz = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(bx + (bw - lblSz.x) * 0.5f, by + 8.0f),
                     lblCol, label);

        const ImVec2 valSz = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(bx + (bw - valSz.x) * 0.5f, by + bh - valSz.y - 12.0f),
                     valCol, buf);
    };

    const HudState& hud = GameState::hud();
    constexpr float pad = 24.0f;
    drawHudBlock(x0 + pad,             y0 + h - 70.0f - pad, "HP",   hud.hp);
    drawHudBlock(x0 + w - 140.0f - pad, y0 + h - 70.0f - pad, "AMMO", hud.ammo);

    // --- Menu de pausa ---
    if (GameState::paused()) {
        // Overlay oscuro cubre todo el panel Viewport.
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h),
                           IM_COL32(0, 0, 0, 180));

        // Panel cuadrado centrado. Tamanio adaptativo: 50% del ancho
        // del viewport con clamp [320, 520]. Asi escala con el panel
        // sin distorsionarse.
        const float panelW = std::clamp(w * 0.5f, 320.0f, 520.0f);
        const float panelH = 360.0f;
        const float px = x0 + (w - panelW) * 0.5f;
        const float py = y0 + (h - panelH) * 0.5f;
        const ImU32 panelBg     = IM_COL32(20, 22, 28, 230);
        const ImU32 panelBorder = IM_COL32(255, 255, 255, 100);
        dl->AddRectFilled(ImVec2(px, py), ImVec2(px + panelW, py + panelH),
                           panelBg, 6.0f);
        dl->AddRect(ImVec2(px, py), ImVec2(px + panelW, py + panelH),
                     panelBorder, 6.0f, 0, 2.0f);

        // Titulo "PAUSA" centrado.
        const char* title = "PAUSA";
        const ImVec2 titleSz = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(px + (panelW - titleSz.x) * 0.5f, py + 28.0f),
                     IM_COL32(255, 255, 255, 255), title);

        struct PauseButton {
            const char* label;
            int action; // 0 = Resume, 1 = Options, 2 = Quit
        };
        const PauseButton buttons[3] = {
            {"Continuar",      0},
            {"Opciones",       1},
            {"Salir al editor", 2},
        };
        constexpr float btnH    = 50.0f;
        constexpr float btnGap  = 12.0f;
        const float btnW = panelW - 48.0f;
        const float bx = px + 24.0f;
        const float by0 = py + 90.0f;

        for (int i = 0; i < 3; ++i) {
            const float by = by0 + i * (btnH + btnGap);
            const ImVec2 a(bx, by), b(bx + btnW, by + btnH);
            // NOTA: NO usamos `viewport().imageHovered()` aqui porque
            // ViewportPanel resetea ese flag al inicio del frame y solo
            // lo setea DESPUES de invocar este callback. IsMouseHoveringRect
            // ya es un test de pixel puro (clip por current clip rect).
            const bool hovered = ImGui::IsMouseHoveringRect(a, b);
            const ImU32 bg = hovered
                ? IM_COL32(80, 90, 110, 240)
                : IM_COL32(60, 65, 80, 220);
            dl->AddRectFilled(a, b, bg, 4.0f);
            dl->AddRect(a, b, IM_COL32(255, 255, 255, 100), 4.0f, 0, 1.0f);
            const ImVec2 sz = ImGui::CalcTextSize(buttons[i].label);
            dl->AddText(ImVec2(bx + (btnW - sz.x) * 0.5f,
                                by + (btnH - sz.y) * 0.5f),
                         IM_COL32(255, 255, 255, 255), buttons[i].label);

            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                Log::editor()->info("PauseMenu click: {}", buttons[i].label);
                switch (buttons[i].action) {
                    case 0: // Resume
                        // Sync de cursor lo hace updateCameras al detectar
                        // la transicion paused=true -> false.
                        GameState::paused() = false;
                        break;
                    case 1: // Options
                        Log::editor()->info("Pause: 'Opciones' aun no implementado.");
                        break;
                    case 2: // Quit
                        exitPlayMode();
                        break;
                    default: break;
                }
            }
        }
    }
}

} // namespace Mood
