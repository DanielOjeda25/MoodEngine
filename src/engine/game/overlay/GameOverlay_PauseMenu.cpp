// Pause menu Doom-style. Extraido de GameOverlay.cpp en AUDIT-1
// (2026-05-17) para mantener el archivo bajo el hard cap de 800 LOC.
//
// Dim background + titulo "PAUSED" grande arriba + 3 botones con border
// geometrico (chevrons en las esquinas en hover). No es un widget del
// registry k_widgets — tiene firma distinta (recibe exitLabel + callback)
// y se llama directo desde draw() porque su comportamiento depende de
// quien lo hostea (Editor Play Mode vs MoodPlayer).

#include "engine/game/overlay/GameOverlay_Internal.h"

#include "core/Log.h"
#include "engine/game/state/GameState.h"
#include "engine/i18n/I18n.h"

#include <imgui.h>

#include <functional>
#include <string>

namespace Mood::GameOverlay {

void drawPauseMenu(const HudContext& ctx,
                    const char* exitLabel,
                    const std::function<void()>& onExitRequested) {
    if (!GameState::paused()) return;

    // Dim del background (~85% alpha).
    ctx.dl->AddRectFilled(ImVec2(ctx.x0, ctx.y0),
                           ImVec2(ctx.x0 + ctx.w, ctx.y0 + ctx.h),
                           palette::k_bg_pause);

    // Titulo "PAUSED" en grande arriba (font scale 4x). F2H43: i18n.
    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float titleY = ctx.y0 + ctx.h * 0.18f;
    const std::string pausedLbl = I18n::T("hud.menu.paused");
    const ImVec2 titleSz = calcTextSizeScaled(pausedLbl.c_str(), 4.0f);
    drawTextScaled(ctx.dl, ImVec2(cx - titleSz.x * 0.5f, titleY),
                   pausedLbl.c_str(), palette::k_orange, 4.0f);

    // Subtitulo chico. NO traducimos "MoodEngine" — es el nombre del producto.
    drawTextCentered(ctx.dl, cx, titleY + titleSz.y + 8.0f,
                      "MoodEngine", palette::k_white_dim);

    // 3 botones con border geometrico. F2H43: labels traducidos.
    const std::string continueLbl = I18n::T("hud.menu.continue");
    const std::string optionsLbl  = I18n::T("hud.menu.options");
    struct Btn { const char* label; int action; };
    const Btn btns[3] = {
        {continueLbl.c_str(), 0},
        {optionsLbl.c_str(),  1},
        {exitLabel,            2},
    };
    constexpr float btnW = 360.0f;
    constexpr float btnH = 56.0f;
    constexpr float btnGap = 14.0f;
    const float bx = cx - btnW * 0.5f;
    const float by0 = titleY + titleSz.y + 80.0f;

    for (int i = 0; i < 3; ++i) {
        const float by = by0 + i * (btnH + btnGap);
        const ImVec2 a(bx, by), b(bx + btnW, by + btnH);
        const bool hovered = ImGui::IsMouseHoveringRect(a, b);

        // Background de la botonera.
        const ImU32 bg = hovered
            ? palette::k_orange_dim
            : palette::k_bg_panel;
        ctx.dl->AddRectFilled(a, b, bg, 2.0f);

        // Borders geometricos: 4 lineas + 4 chevrons en las esquinas.
        const ImU32 borderCol = hovered ? palette::k_orange : palette::k_border;
        const float thick = hovered ? 2.5f : 1.5f;
        ctx.dl->AddRect(a, b, borderCol, 2.0f, 0, thick);

        // Chevrons en esquinas (lineas diagonales para "geometric"
        // feel Doom). Solo en hover.
        if (hovered) {
            constexpr float ch = 10.0f;
            const ImU32 cc = palette::k_orange;
            // Esquinas: top-left, top-right, bottom-left, bottom-right.
            ctx.dl->AddLine(ImVec2(a.x - 4.0f, a.y + ch), ImVec2(a.x - 4.0f, a.y - 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(a.x - 4.0f, a.y - 4.0f), ImVec2(a.x + ch, a.y - 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(b.x + 4.0f, a.y + ch), ImVec2(b.x + 4.0f, a.y - 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(b.x + 4.0f, a.y - 4.0f), ImVec2(b.x - ch, a.y - 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(a.x - 4.0f, b.y - ch), ImVec2(a.x - 4.0f, b.y + 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(a.x - 4.0f, b.y + 4.0f), ImVec2(a.x + ch, b.y + 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(b.x + 4.0f, b.y - ch), ImVec2(b.x + 4.0f, b.y + 4.0f), cc, 2.0f);
            ctx.dl->AddLine(ImVec2(b.x + 4.0f, b.y + 4.0f), ImVec2(b.x - ch, b.y + 4.0f), cc, 2.0f);
        }

        // Label centrado, escala 1.4x.
        const ImVec2 lblSz = calcTextSizeScaled(btns[i].label, 1.4f);
        drawTextScaled(ctx.dl,
                        ImVec2(bx + (btnW - lblSz.x) * 0.5f,
                               by + (btnH - lblSz.y) * 0.5f),
                        btns[i].label,
                        hovered ? palette::k_white : palette::k_white_dim,
                        1.4f);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            Log::engine()->info("PauseMenu click: {}", btns[i].label);
            switch (btns[i].action) {
                case 0: GameState::paused() = false; break;
                case 1: Log::engine()->info("Pause: 'Options' aun no implementado."); break;
                case 2: if (onExitRequested) onExitRequested(); break;
                default: break;
            }
        }
    }
}

} // namespace Mood::GameOverlay
