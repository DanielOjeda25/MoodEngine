// Widget `quest_log_panel` (F2H53 Bloque G). Lista TODOS los quests
// activos (Active/Complete/Failed que aun esten en `QuestSystem::snapshot()`).
// Click sobre el header de un quest -> `quest.set_tracked(...)` para
// que el Quest Tracker (Bloque F, top-left) lo destaque.
//
// Default OFF (como inventory_panel). Toggle por tecla `J` en el game
// loop (Editor + Player). Engine-grade: el widget solo lee `QuestSystem`
// y `AssetManager`; no muta game state ni dispara eventos custom.
//
// Layout v1 sin scroll: si hay >N quests, los siguientes quedan fuera.
// Suficiente para juegos con pocos quests activos simultaneos. Si emerge
// necesidad, migrar a ImGui::Begin + ScrollRegion (decision a aparte).

#include "engine/game/overlay/GameOverlay_Internal.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/game/state/GameState.h"
#include "core/i18n/I18n.h"
#include "engine/quest/QuestAsset.h"
#include "engine/quest/QuestSystem.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood::GameOverlay {

namespace {

namespace QS = Mood::Quest::QuestSystem;

/// @brief Resuelve el nombre del quest (i18n key > literal > id).
std::string questDisplayName(const Mood::Quest::Asset& asset) {
    if (!asset.name_key.empty())     return Mood::I18n::T(asset.name_key);
    if (!asset.name_literal.empty()) return asset.name_literal;
    return asset.id;
}

/// @brief Resuelve la descripcion (igual fallback que el nombre).
std::string questDescription(const Mood::Quest::Asset& asset) {
    if (!asset.description_key.empty())
        return Mood::I18n::T(asset.description_key);
    return asset.description_literal;
}

/// @brief Nombre legible del objective (i18n key > literal > id).
std::string objectiveDisplayName(const Mood::Quest::Objective& o) {
    if (!o.name_key.empty())     return Mood::I18n::T(o.name_key);
    if (!o.name_literal.empty()) return o.name_literal;
    return o.id;
}

/// @brief Color de borde + label del state badge.
struct StateVis {
    ImU32 color;
    const char* labelKey;
};

StateVis visForState(QS::State s) {
    using S = QS::State;
    switch (s) {
        case S::Active:   return { palette::k_yellow,     "hud.quest_log.state.active" };
        case S::Complete: return { IM_COL32(120, 200, 120, 255),
                                    "hud.quest_log.state.complete" };
        case S::Failed:   return { palette::k_red,        "hud.quest_log.state.failed" };
        case S::NotStarted:
        default:          return { palette::k_white_dim,  "hud.quest_log.state.not_started" };
    }
}

} // namespace

void drawQuestLogPanel(const HudContext& ctx) {
    if (ctx.assets == nullptr) return;

    const auto& snapshot = QS::snapshot();
    if (snapshot.empty()) {
        // Render empty-state mini panel para feedback al dev: si abrio el
        // log y no hay quests, mostrar mensaje en vez de no-render (que se
        // ve como "el toggle no funciono").
    }

    // Layout: panel centrado.
    constexpr float panelW   = 480.0f;
    constexpr float padX     = 16.0f;
    constexpr float padY     = 12.0f;
    constexpr float titleH   = 30.0f;
    constexpr float questGap = 12.0f;
    const float lineH = ImGui::GetTextLineHeight();

    // Pre-resolver entries con asset + textos para calcular alto total.
    struct Row {
        QuestAssetId id;
        QS::State    state;
        std::string  path;
        std::string  title;
        std::string  desc;
        struct Obj { std::string text; bool done; };
        std::vector<Obj> objectives;
        bool isTracked;
    };
    std::vector<Row> rows;
    rows.reserve(snapshot.size());
    const QuestAssetId trackedId = QS::tracked();

    for (const auto& aq : snapshot) {
        const Mood::Quest::Asset* asset = ctx.assets->getQuest(aq.id);
        if (asset == nullptr) continue;
        Row r;
        r.id        = aq.id;
        r.state     = aq.state;
        r.path      = ctx.assets->questPathOf(aq.id);
        r.title     = questDisplayName(*asset);
        r.desc      = questDescription(*asset);
        r.isTracked = (aq.id == trackedId);
        for (size_t i = 0; i < asset->objectives.size(); ++i) {
            const auto& o = asset->objectives[i];
            const bool done = (i < aq.objectives.size())
                && aq.objectives[i].completed;
            std::string text = (done ? "[x] " : "[ ] ")
                + objectiveDisplayName(o);
            r.objectives.push_back({std::move(text), done});
        }
        rows.push_back(std::move(r));
    }

    // Calcular alto: header global + por cada quest (titulo + state badge +
    // desc opcional + objectives + gap).
    float contentH = padY;
    for (const auto& r : rows) {
        contentH += lineH + 4.0f;            // titulo
        if (!r.desc.empty()) contentH += lineH + 2.0f;
        for (size_t i = 0; i < r.objectives.size(); ++i) {
            (void)i;
            contentH += lineH + 2.0f;
        }
        contentH += questGap;
    }
    if (rows.empty()) {
        contentH += lineH * 2.0f;
    }
    contentH += padY;

    const float panelH = titleH + contentH;
    const float panelX = ctx.x0 + (ctx.w - panelW) * 0.5f;
    const float panelY = ctx.y0 + (ctx.h - panelH) * 0.5f;

    // Background + border.
    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    // Titulo global.
    const std::string title = Mood::I18n::T("hud.quest_log.title");
    drawTextCentered(ctx.dl, panelX + panelW * 0.5f, panelY + 7.0f,
                      title.c_str(), palette::k_orange);

    // Empty state.
    if (rows.empty()) {
        const std::string empty = Mood::I18n::T("hud.quest_log.empty");
        drawTextCentered(ctx.dl, panelX + panelW * 0.5f,
                          panelY + titleH + padY,
                          empty.c_str(), palette::k_white_dim);
        return;
    }

    // Renderear cada quest.
    float y = panelY + titleH + padY;
    for (auto& r : rows) {
        // Header del quest: titulo + state badge a la derecha.
        const ImVec2 headerA(panelX + padX, y);
        const ImVec2 headerB(panelX + panelW - padX, y + lineH + 2.0f);
        const bool hovered = ImGui::IsMouseHoveringRect(headerA, headerB);
        if (hovered) {
            ctx.dl->AddRectFilled(headerA, headerB,
                                    palette::k_orange_dim, 2.0f);
        }
        // Click izquierdo en el header -> set_tracked.
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            QS::setTracked(r.id);
        }

        // Tracked marker.
        std::string titleText = (r.isTracked ? "* " : "  ") + r.title;
        ctx.dl->AddText(ImVec2(panelX + padX + 4.0f, y),
                         palette::k_yellow, titleText.c_str());

        // State badge a la derecha.
        const StateVis vis = visForState(r.state);
        const std::string badge = Mood::I18n::T(vis.labelKey);
        const ImVec2 badgeSz = ImGui::CalcTextSize(badge.c_str());
        ctx.dl->AddText(
            ImVec2(panelX + panelW - padX - 4.0f - badgeSz.x, y),
            vis.color, badge.c_str());

        y += lineH + 4.0f;

        // Descripcion (si la hay).
        if (!r.desc.empty()) {
            ctx.dl->AddText(ImVec2(panelX + padX + 12.0f, y),
                             palette::k_white_dim, r.desc.c_str());
            y += lineH + 2.0f;
        }

        // Objectives.
        constexpr ImU32 k_done_green = IM_COL32(120, 200, 120, 255);
        for (const auto& o : r.objectives) {
            const ImU32 col = o.done ? k_done_green : palette::k_white;
            ctx.dl->AddText(ImVec2(panelX + padX + 24.0f, y),
                             col, o.text.c_str());
            y += lineH + 2.0f;
        }

        y += questGap;
    }
}

} // namespace Mood::GameOverlay
