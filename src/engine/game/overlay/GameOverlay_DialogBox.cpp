// F2H48 dialog box widget. Extraido de GameOverlay.cpp en AUDIT-1
// (2026-05-17) cuando el archivo pasaba el hard cap de 800 LOC.
//
// Caja inferior estilo Half-Life 2 con el texto del NPC + las choices
// disponibles. Solo dibuja si DialogSystem::isActive(). Detecta el i18n
// resolution del label_key/text_key: label_literal/text_literal tiene
// prioridad sobre la key (paridad con la convencion de DialogAsset::Line).

#include "engine/game/overlay/GameOverlay_Internal.h"

#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogSystem.h"
#include "core/i18n/I18n.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Mood::GameOverlay {

void drawDialogBox(const HudContext& ctx) {
    using namespace Mood::Dialog;
    if (!DialogSystem::isActive()) return;

    // Resolver Line + choices visibles.
    const Line line = DialogSystem::currentLine();
    const auto choices = DialogSystem::availableChoices();

    // Texto del NPC: literal o resolver via i18n.
    std::string npcText = !line.text_literal.empty()
        ? line.text_literal
        : (!line.text_key.empty()
              ? Mood::I18n::T(line.text_key)
              : std::string{});

    // Layout: caja inferior ancha (clamped a 60% del width o 800px).
    const float boxW = std::min(800.0f, ctx.w * 0.7f);
    const float boxX = ctx.x0 + (ctx.w - boxW) * 0.5f;

    // Wrap del texto NPC para calcular alto. Usamos ImGui::CalcTextSize
    // con wrapWidth = boxW - 2*padX. Mismo trick que tooltips.
    constexpr float padX = 24.0f;
    constexpr float padY = 18.0f;
    const float wrapW = boxW - 2.0f * padX;
    const ImVec2 textSize = ImGui::CalcTextSize(
        npcText.c_str(), nullptr, false, wrapW);

    // Cada choice ocupa una linea. Si no hay choices, espacio para hint
    // "[E] Continuar".
    constexpr float choiceLineH = 22.0f;
    const float choicesAreaH = !choices.empty()
        ? (static_cast<float>(choices.size()) * choiceLineH + 8.0f)
        : (choiceLineH + 4.0f);

    const float boxH = padY + textSize.y + 12.0f + choicesAreaH + padY;
    const float boxY = ctx.y0 + ctx.h - boxH - 40.0f;  // 40px margen abajo

    // Background HL2-style: panel oscuro + borde naranja.
    const ImVec2 a(boxX, boxY);
    const ImVec2 b(boxX + boxW, boxY + boxH);
    ctx.dl->AddRectFilled(a, b, palette::k_bg_panel, 6.0f);
    ctx.dl->AddRect(a, b, palette::k_orange, 6.0f, 0, 2.0f);

    // Texto del NPC (multilinea con wrap manual via ImDrawList::AddText
    // con wrap_width).
    const ImVec2 textPos(boxX + padX, boxY + padY);
    ctx.dl->AddText(ImGui::GetFont(),
                     ImGui::GetFontSize(),
                     textPos,
                     palette::k_white,
                     npcText.c_str(),
                     nullptr,
                     wrapW);

    // Separador delicado entre NPC text y choices.
    const float sepY = textPos.y + textSize.y + 8.0f;
    ctx.dl->AddLine(ImVec2(boxX + padX, sepY),
                     ImVec2(boxX + boxW - padX, sepY),
                     palette::k_border, 1.0f);

    // Choices: numeradas "1) opcion", "2) opcion" ... estilo HL2.
    if (choices.empty()) {
        // Continue-only: hint "[E] Continuar" centrado.
        const std::string hint = Mood::I18n::T("hud.dialog.continue_hint");
        const ImVec2 hSz = ImGui::CalcTextSize(hint.c_str());
        const float hx = boxX + (boxW - hSz.x) * 0.5f;
        const float hy = sepY + 12.0f;
        ctx.dl->AddText(ImVec2(hx, hy), palette::k_yellow_dim, hint.c_str());
    } else {
        for (size_t i = 0; i < choices.size(); ++i) {
            const std::string& rawLabel = choices[i].label;
            // Si la label parece i18n key (contiene "."), resolverla;
            // sino usar literal. DialogSystem::availableChoices ya
            // metio literal con prioridad — lo que llega aca es uno o
            // el otro.
            const bool looksLikeKey = !rawLabel.empty()
                                       && rawLabel.find('.') != std::string::npos
                                       && rawLabel.find(' ') == std::string::npos;
            const std::string resolved = looksLikeKey
                ? Mood::I18n::T(rawLabel)
                : rawLabel;
            // Numeracion 1-9 (las claves del teclado del player).
            char prefix[8];
            std::snprintf(prefix, sizeof(prefix), "%zu) ", i + 1);
            const std::string fullLine = std::string(prefix) + resolved;
            const float lx = boxX + padX;
            const float ly = sepY + 8.0f + static_cast<float>(i) * choiceLineH;
            ctx.dl->AddText(ImVec2(lx, ly), palette::k_yellow, fullLine.c_str());
        }
    }
}

} // namespace Mood::GameOverlay
