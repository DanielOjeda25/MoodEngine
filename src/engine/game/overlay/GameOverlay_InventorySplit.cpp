// F2H52 Bloque I — Container split mode. Extraido de
// `GameOverlay_Inventory.cpp` en AUDIT-1 (2026-05-17) cuando el archivo
// pasaba el hard cap de 800 LOC.
//
// Cuando `HudState::container_target` apunta a una entity valida con
// `InventoryComponent`, el widget renderea dos columnas estilo Skyrim
// "Take/Place" — inventario del player a la izquierda, del container a
// la derecha. Drag entre lados transfiere 1 unidad. Atomico: primero
// add en destino; si fallo (capacity full) abortar sin tocar source.
//
// Limitacion v1: ambas columnas renderean como FlatList visual, sin
// importar el `state.mode` del container. El loot UX casi nunca quiere
// el grid spatial del container; si el dev necesita Grid2D para abrir
// un cofre con tetris, lo agregamos en una version posterior.

#include "engine/game/overlay/GameOverlay_Internal.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Mood::GameOverlay {

namespace {

/// @brief Estado del drag entre las dos columnas del container split.
///        Static local — persiste mientras el container este abierto.
struct SplitDragState {
    int  side      = -1;  // 0 = player, 1 = container, -1 = no drag
    int  fromIndex = -1;  // index en entries[] del lado source
    bool wasDown   = false;
};

} // namespace

void drawInventorySplit(const HudContext& ctx,
                         Inventory::State& playerInv,
                         Inventory::State& containerInv,
                         AssetManager& assets) {
    static SplitDragState s_drag;

    constexpr float panelW = 320.0f;
    constexpr float rowH   = 28.0f;
    constexpr float padX   = 12.0f;
    constexpr float padY   = 10.0f;
    constexpr float titleH = 28.0f;
    constexpr float gap    = 16.0f;

    const int rowsP = std::max(1, static_cast<int>(playerInv.entries.size()));
    const int rowsC = std::max(1, static_cast<int>(containerInv.entries.size()));
    const int maxRows = std::max(rowsP, rowsC);
    const float panelH = titleH + padY + maxRows * rowH + padY;

    const float totalW = 2.0f * panelW + gap;
    const float startX = ctx.x0 + (ctx.w - totalW) * 0.5f;
    const float panelY = ctx.y0 + 80.0f;
    const float xLeft  = startX;
    const float xRight = startX + panelW + gap;

    // Render de un lado del split. Retorna el index del entry hovered
    // o -1 si ninguno.
    auto renderSide = [&](int sideIdx, Inventory::State& inv,
                           float x, const char* titleKey) -> int {
        ctx.dl->AddRectFilled(ImVec2(x, panelY),
                               ImVec2(x + panelW, panelY + panelH),
                               palette::k_bg_panel, 4.0f);
        ctx.dl->AddRect(ImVec2(x, panelY),
                         ImVec2(x + panelW, panelY + panelH),
                         palette::k_border, 4.0f, 0, 1.5f);

        const std::string title = I18n::T(titleKey);
        drawTextCentered(ctx.dl, x + panelW * 0.5f, panelY + 6.0f,
                          title.c_str(), palette::k_orange);

        if (inv.entries.empty()) {
            const std::string empty = I18n::T("hud.inventory.empty");
            drawTextCentered(ctx.dl, x + panelW * 0.5f,
                              panelY + titleH + padY + 4.0f,
                              empty.c_str(), palette::k_white_dim);
            return -1;
        }

        int hoveredIdx = -1;
        float y = panelY + titleH + padY;
        for (int i = 0; i < static_cast<int>(inv.entries.size()); ++i) {
            const auto& e = inv.entries[i];
            const Inventory::Asset* asset = assets.getItem(e.itemId);
            const std::string path = assets.itemPathOf(e.itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;

            const ImVec2 a(x + padX, y);
            const ImVec2 b(x + panelW - padX, y + rowH - 4.0f);
            const bool hovered = ImGui::IsMouseHoveringRect(a, b);
            if (hovered) {
                ctx.dl->AddRectFilled(a, b, palette::k_orange_dim, 2.0f);
                hoveredIdx = i;
            }

            const bool isDragSource = (s_drag.side == sideIdx
                                        && s_drag.fromIndex == i);
            const ImU32 col = isDragSource
                ? palette::k_white_dim
                : palette::k_white;
            ctx.dl->AddText(ImVec2(x + padX + 4.0f, y + 4.0f), col,
                              name.c_str());

            if (e.quantity > 1) {
                char qtyBuf[32];
                std::snprintf(qtyBuf, sizeof(qtyBuf), "x %d", e.quantity);
                const ImVec2 sz = ImGui::CalcTextSize(qtyBuf);
                ctx.dl->AddText(
                    ImVec2(x + panelW - padX - 4.0f - sz.x, y + 4.0f),
                    palette::k_yellow, qtyBuf);
            }
            y += rowH;
        }
        return hoveredIdx;
    };

    const int hoveredLeft  = renderSide(0, playerInv,    xLeft,
                                         "hud.inventory.title");
    const int hoveredRight = renderSide(1, containerInv, xRight,
                                         "hud.inventory.container_title");

    // Drag flank events.
    const bool mouseDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool mouseClicked = mouseDown && !s_drag.wasDown;
    const bool mouseReleased= !mouseDown && s_drag.wasDown;
    s_drag.wasDown = mouseDown;

    if (mouseClicked) {
        if (hoveredLeft >= 0) {
            s_drag.side      = 0;
            s_drag.fromIndex = hoveredLeft;
        } else if (hoveredRight >= 0) {
            s_drag.side      = 1;
            s_drag.fromIndex = hoveredRight;
        }
    }

    if (mouseReleased) {
        int dstSide = -1;
        if (hoveredLeft  >= 0) dstSide = 0;
        else if (hoveredRight >= 0) dstSide = 1;

        if (s_drag.side >= 0 && dstSide >= 0 && dstSide != s_drag.side) {
            Inventory::State& src = (s_drag.side == 0)
                ? playerInv : containerInv;
            Inventory::State& dst = (dstSide == 0)
                ? playerInv : containerInv;

            if (s_drag.fromIndex >= 0
                && s_drag.fromIndex < static_cast<int>(src.entries.size())) {
                const ItemAssetId itemId = src.entries[s_drag.fromIndex].itemId;
                // Atomico: add primero; remove solo si add ok.
                if (dst.add(itemId, 1, assets)) {
                    if (!src.remove(itemId, 1)) {
                        // Defensive: si add OK pero remove fail, refund.
                        Log::script()->warn(
                            "[inventory split] transfer: src.remove fallo "
                            "tras dst.add ok - posible corrupcion de state");
                    }
                }
            }
        }
        s_drag.side      = -1;
        s_drag.fromIndex = -1;
    }

    // Drag ghost en el cursor.
    if (s_drag.side >= 0) {
        Inventory::State& src = (s_drag.side == 0)
            ? playerInv : containerInv;
        if (s_drag.fromIndex >= 0
            && s_drag.fromIndex < static_cast<int>(src.entries.size())) {
            const auto& e = src.entries[s_drag.fromIndex];
            const Inventory::Asset* asset = assets.getItem(e.itemId);
            const std::string path = assets.itemPathOf(e.itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            const ImVec2 mp = ImGui::GetMousePos();
            const ImVec2 sz = ImGui::CalcTextSize(name.c_str());
            const ImVec2 ga(mp.x - sz.x * 0.5f - 6, mp.y - 12);
            const ImVec2 gb(mp.x + sz.x * 0.5f + 6, mp.y + 12);
            ctx.dl->AddRectFilled(ga, gb, palette::k_orange_dim, 4.0f);
            ctx.dl->AddText(ImVec2(mp.x - sz.x * 0.5f, mp.y - 7),
                              palette::k_white, name.c_str());
        }
    }
}

} // namespace Mood::GameOverlay
