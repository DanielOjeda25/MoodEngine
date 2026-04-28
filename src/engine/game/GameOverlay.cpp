#include "engine/game/GameOverlay.h"

#include "core/Log.h"
#include "engine/game/GameState.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace Mood::GameOverlay {

void draw(::ImDrawList* dl,
          float x0, float y0, float w, float h,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested) {
    // Convencion: TODAS las coords del overlay son screen-space del
    // panel/ventana del juego (origen top-left en x0,y0). El drawlist
    // clip rect ya esta seteado al area del Image por el caller.

    // --- Crosshair ---
    {
        const float cx = x0 + w * 0.5f;
        const float cy = y0 + h * 0.5f;
        const float r  = 6.0f;
        const ImU32 col = IM_COL32(255, 255, 255, 220);
        const ImU32 outline = IM_COL32(0, 0, 0, 220);
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
        // Overlay oscuro cubre todo el rect.
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h),
                           IM_COL32(0, 0, 0, 180));

        // Panel cuadrado centrado: 50% del ancho con clamp [320, 520].
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

        const char* title = "PAUSA";
        const ImVec2 titleSz = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(px + (panelW - titleSz.x) * 0.5f, py + 28.0f),
                     IM_COL32(255, 255, 255, 255), title);

        struct PauseButton {
            const char* label;
            int action; // 0 = Resume, 1 = Options, 2 = Exit
        };
        const PauseButton buttons[3] = {
            {"Continuar",     0},
            {"Opciones",      1},
            {exitButtonLabel, 2},
        };
        constexpr float btnH    = 50.0f;
        constexpr float btnGap  = 12.0f;
        const float btnW = panelW - 48.0f;
        const float bx = px + 24.0f;
        const float by0 = py + 90.0f;

        for (int i = 0; i < 3; ++i) {
            const float by = by0 + i * (btnH + btnGap);
            const ImVec2 a(bx, by), b(bx + btnW, by + btnH);
            // IsMouseHoveringRect = test de pixel puro respetando el
            // clip rect actual del drawlist.
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
                Log::engine()->info("PauseMenu click: {}", buttons[i].label);
                switch (buttons[i].action) {
                    case 0: // Resume
                        // Sync de cursor lo hace el caller al detectar
                        // la transicion paused=true -> false.
                        GameState::paused() = false;
                        break;
                    case 1: // Options
                        Log::engine()->info("Pause: 'Opciones' aun no implementado.");
                        break;
                    case 2: // Exit
                        if (onExitRequested) onExitRequested();
                        break;
                    default: break;
                }
            }
        }
    }
}

} // namespace Mood::GameOverlay
