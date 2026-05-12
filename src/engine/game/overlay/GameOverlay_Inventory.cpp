// Widget `inventory_panel` del HUD del juego (F2H52 Bloque H). Split
// de `GameOverlay.cpp` cuando el archivo paso los 1300 lineas. La logica
// vive aca; el registry de widgets sigue en GameOverlay.cpp y referencia
// `drawInventoryPanel` declarado en `GameOverlay_Internal.h`.
//
// 3 modes segun `InventoryComponent.state.mode`:
//   - FlatList: lista vertical de filas "nombre x qty" (Diablo / Pip-Boy).
//   - Grid2D: grid WxH de cells con drag entre slots (Resident Evil).
//   - EquipmentSlots: slots nombrados con tag_filter (Skyrim).
//
// Engine-grade: el widget SOLO lee `Inventory::State` + `Inventory::Asset`.
// Las acciones (use/drop) van por right-click menu (H5) que invoca los
// hooks Lua o llama directo a `inventory.remove` para drop.

#include "engine/game/overlay/GameOverlay_Internal.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/game/state/GameState.h"
#include "engine/i18n/I18n.h"
#include "engine/inventory/InventoryHooks.h"   // F2H52 H5
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/inventory/ItemSpawn.h"        // F2H52 H5
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Mood::GameOverlay {

namespace {

/// @brief Busca el primer entity con TagComponent.name == "Player" y
///        InventoryComponent. Devuelve Entity falsy si no esta.
Entity findPlayerForHud(Scene& scene) {
    Entity result;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (result) return;
        if (tag.name == "Player" && e.hasComponent<InventoryComponent>()) {
            result = e;
        }
    });
    return result;
}

/// @brief Nombre legible del item (mismo orden de fallback que el editor):
///        name_literal -> i18n(name_key) -> id -> path stem -> "(unknown)".
std::string displayNameOfItem(const Inventory::Asset& asset,
                               const std::string& pathFallback) {
    if (!asset.name_literal.empty()) return asset.name_literal;
    if (!asset.name_key.empty())     return Mood::I18n::T(asset.name_key);
    if (!asset.id.empty())           return asset.id;
    if (!pathFallback.empty()) {
        const auto slash = pathFallback.find_last_of('/');
        const auto dot   = pathFallback.find_last_of('.');
        const auto beg   = (slash == std::string::npos) ? 0 : slash + 1;
        const auto end   = (dot == std::string::npos || dot < beg)
                                ? pathFallback.size() : dot;
        return pathFallback.substr(beg, end - beg);
    }
    return "(unknown)";
}

// =============================================================
// F2H52 H5 — TOOLTIP + RIGHT-CLICK CONTEXT MENU
// =============================================================
// Tooltip:  al hover sobre un item se dibuja un mini-panel cerca del
//           cursor con nombre + descripcion + tags + stats. Skipea si
//           el menu contextual esta abierto.
// Menu:     al click derecho sobre un item, se abre un menu de 2
//           botones — Usar / Tirar. Permanece abierto hasta que el dev
//           clickea fuera o elige una opcion.
//           - Usar: invoca `Inventory::Hooks::invokeUse(player, path)`.
//             El motor NO auto-consume — el hook del dev decide si
//             llama `inventory.remove(...)` (caso pocion) o no (caso
//             arma equipable).
//           - Tirar: `inv.remove(itemId, 1)` + spawn pickup en el mundo
//             enfrente del player + `Inventory::Hooks::invokeDrop`. El
//             item aparece en el mundo y se puede levantar de nuevo con E.

struct ContextMenuState {
    bool         open      = false;
    ImVec2       pos       {0.0f, 0.0f};
    ItemAssetId  itemId    = 0;
    std::string  itemPath;
    std::string  itemName;
};
static ContextMenuState s_ctxMenu;

void openContextMenu(ImVec2 mousePos, const Inventory::Entry& entry,
                      AssetManager& assets) {
    s_ctxMenu.open     = true;
    s_ctxMenu.pos      = mousePos;
    s_ctxMenu.itemId   = entry.itemId;
    s_ctxMenu.itemPath = assets.itemPathOf(entry.itemId);
    const Inventory::Asset* asset = assets.getItem(entry.itemId);
    s_ctxMenu.itemName = (asset != nullptr)
        ? displayNameOfItem(*asset, s_ctxMenu.itemPath)
        : s_ctxMenu.itemPath;
}

/// @brief Dibuja un tooltip flotante con nombre + descripcion + tags +
///        stats del item, posicionado cerca del cursor. Flipea a la
///        izquierda si excederia el viewport. No-op si el context menu
///        esta abierto (para evitar doble overlay).
void drawItemTooltip(const HudContext& ctx,
                      const Inventory::Asset& asset,
                      const std::string& pathFallback) {
    if (s_ctxMenu.open) return;

    const ImVec2 mouse = ImGui::GetMousePos();
    const std::string name = displayNameOfItem(asset, pathFallback);

    std::string desc;
    if (!asset.description_literal.empty()) {
        desc = asset.description_literal;
    } else if (!asset.description_key.empty()) {
        desc = I18n::T(asset.description_key);
    }

    // Construir lineas en orden: nombre, descripcion, tags, stats.
    std::vector<std::pair<std::string, ImU32>> lines;
    lines.emplace_back(name, palette::k_orange);
    if (!desc.empty()) {
        lines.emplace_back(desc, palette::k_white);
    }
    if (!asset.tags.empty()) {
        std::string tagsLine;
        for (size_t i = 0; i < asset.tags.size(); ++i) {
            if (i > 0) tagsLine += "  ";
            tagsLine += "* ";
            tagsLine += asset.tags[i];
        }
        lines.emplace_back(std::move(tagsLine), palette::k_yellow_dim);
    }
    for (const auto& [statName, statValue] : asset.stats) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s: %.2f",
                      statName.c_str(),
                      static_cast<double>(statValue));
        lines.emplace_back(buf, palette::k_white_dim);
    }

    constexpr float padX  = 10.0f;
    constexpr float padY  = 8.0f;
    constexpr float lineH = 18.0f;
    float maxW = 0.0f;
    for (const auto& [text, _col] : lines) {
        const ImVec2 sz = ImGui::CalcTextSize(text.c_str());
        if (sz.x > maxW) maxW = sz.x;
    }
    const float w = maxW + 2.0f * padX;
    const float h = static_cast<float>(lines.size()) * lineH + 2.0f * padY;

    // Default: a la derecha del cursor; flipea a la izquierda si excede.
    float x = mouse.x + 18.0f;
    float y = mouse.y + 12.0f;
    if (x + w > ctx.x0 + ctx.w) {
        x = mouse.x - 18.0f - w;
    }
    if (y + h > ctx.y0 + ctx.h) {
        y = ctx.y0 + ctx.h - h - 4.0f;
    }

    ctx.dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                     palette::k_border, 4.0f, 0, 1.5f);

    float yy = y + padY;
    for (const auto& [text, col] : lines) {
        ctx.dl->AddText(ImVec2(x + padX, yy), col, text.c_str());
        yy += lineH;
    }
}

/// @brief Dibuja el context menu si esta abierto + handlea clicks en
///        los botones. Cierra si se clickea fuera. Llamado desde
///        `drawInventoryPanel` despues del modo activo, para que quede
///        ENCIMA del inventario.
void drawContextMenu(const HudContext& ctx, Entity player,
                      Inventory::State& inv) {
    if (!s_ctxMenu.open) return;
    if (s_ctxMenu.itemId == 0) {
        s_ctxMenu.open = false;
        return;
    }
    if (ctx.scene == nullptr || ctx.assets == nullptr) {
        s_ctxMenu.open = false;
        return;
    }

    constexpr float w      = 140.0f;
    constexpr float btnH   = 26.0f;
    constexpr float titleH = 22.0f;
    constexpr float padX   = 8.0f;
    constexpr float padY   = 6.0f;
    const float h = titleH + 2.0f * btnH + 2.0f * padY + 4.0f;

    float x = s_ctxMenu.pos.x;
    float y = s_ctxMenu.pos.y;
    if (x + w > ctx.x0 + ctx.w) x = ctx.x0 + ctx.w - w - 4.0f;
    if (y + h > ctx.y0 + ctx.h) y = ctx.y0 + ctx.h - h - 4.0f;

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool insidePanel = (mouse.x >= x && mouse.x <= x + w &&
                              mouse.y >= y && mouse.y <= y + h);
    const bool leftClicked  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    if ((leftClicked || rightClicked) && !insidePanel) {
        s_ctxMenu.open = false;
        return;
    }

    ctx.dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                     palette::k_orange, 4.0f, 0, 1.5f);

    ctx.dl->AddText(ImVec2(x + padX, y + padY), palette::k_orange,
                      s_ctxMenu.itemName.c_str());

    auto drawBtn = [&](const char* label, float yPos) -> bool {
        const ImVec2 a(x + padX, yPos);
        const ImVec2 b(x + w - padX, yPos + btnH - 2.0f);
        const bool hovered = mouse.x >= a.x && mouse.x <= b.x &&
                              mouse.y >= a.y && mouse.y <= b.y;
        const ImU32 bg = hovered ? palette::k_orange_dim : palette::k_bg_box;
        ctx.dl->AddRectFilled(a, b, bg, 2.0f);
        ctx.dl->AddRect(a, b, palette::k_border, 2.0f, 0, 1.0f);
        const ImVec2 sz = ImGui::CalcTextSize(label);
        ctx.dl->AddText(ImVec2(a.x + (b.x - a.x - sz.x) * 0.5f,
                                 a.y + (b.y - a.y - sz.y) * 0.5f),
                          palette::k_white, label);
        return hovered && leftClicked;
    };

    const std::string usarLbl  = I18n::T("hud.inventory.use");
    const std::string tirarLbl = I18n::T("hud.inventory.drop");
    const float btnY0 = y + titleH + padY;
    const bool usarClicked  = drawBtn(usarLbl.c_str(),  btnY0);
    const bool tirarClicked = drawBtn(tirarLbl.c_str(), btnY0 + btnH + 2.0f);

    if (usarClicked) {
        Inventory::Hooks::invokeUse(player, s_ctxMenu.itemPath);
        s_ctxMenu.open = false;
    } else if (tirarClicked) {
        if (inv.remove(s_ctxMenu.itemId, 1)) {
            const glm::vec3 dropPos = ctx.cameraPosition
                + ctx.cameraForward * 1.8f
                + glm::vec3(0.0f, -0.4f, 0.0f);
            Inventory::spawnPickupInWorld(ctx.scene, ctx.assets,
                                          s_ctxMenu.itemPath, dropPos, 1);
            Inventory::Hooks::invokeDrop(s_ctxMenu.itemPath, 1);
        }
        s_ctxMenu.open = false;
    }
}

/// @brief Renderea el modo FlatList: lista vertical de filas
///        "nombre x qty". Hover dispara tooltip; click derecho abre
///        context menu Usar / Tirar (F2H52 H5).
void drawInventoryFlatList(const HudContext& ctx,
                            const Inventory::State& inv,
                            AssetManager& assets) {
    constexpr float panelW = 360.0f;
    constexpr float rowH   = 28.0f;
    constexpr float padX   = 12.0f;
    constexpr float padY   = 10.0f;
    constexpr float titleH = 28.0f;

    const int nRows = static_cast<int>(inv.entries.size());
    const float panelH = titleH + padY + std::max(1, nRows) * rowH + padY;
    const float panelX = ctx.x0 + ctx.w - panelW - 20.0f;
    const float panelY = ctx.y0 + 80.0f;

    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    const std::string title = I18n::T("hud.inventory.title");
    drawTextCentered(ctx.dl, panelX + panelW * 0.5f, panelY + 6.0f,
                      title.c_str(), palette::k_orange);

    if (inv.entries.empty()) {
        const std::string empty = I18n::T("hud.inventory.empty");
        drawTextCentered(ctx.dl, panelX + panelW * 0.5f,
                          panelY + titleH + padY + 4.0f,
                          empty.c_str(), palette::k_white_dim);
        return;
    }

    const Inventory::Entry* hoveredEntry = nullptr;
    const Inventory::Asset* hoveredAsset = nullptr;
    std::string hoveredPath;

    float y = panelY + titleH + padY;
    for (const auto& e : inv.entries) {
        const Inventory::Asset* asset = assets.getItem(e.itemId);
        const std::string path = assets.itemPathOf(e.itemId);
        const std::string name = asset != nullptr
            ? displayNameOfItem(*asset, path)
            : path;

        const ImVec2 rowA(panelX + padX, y);
        const ImVec2 rowB(panelX + panelW - padX, y + rowH - 4.0f);
        const bool hovered = ImGui::IsMouseHoveringRect(rowA, rowB);
        if (hovered) {
            ctx.dl->AddRectFilled(rowA, rowB, palette::k_orange_dim, 2.0f);
            hoveredEntry = &e;
            hoveredAsset = asset;
            hoveredPath  = path;
        }

        ctx.dl->AddText(ImVec2(panelX + padX + 4.0f, y + 4.0f),
                          palette::k_white, name.c_str());

        if (e.quantity > 1) {
            char qtyBuf[32];
            std::snprintf(qtyBuf, sizeof(qtyBuf), "x %d", e.quantity);
            const ImVec2 sz = ImGui::CalcTextSize(qtyBuf);
            ctx.dl->AddText(
                ImVec2(panelX + panelW - padX - 4.0f - sz.x, y + 4.0f),
                palette::k_yellow, qtyBuf);
        }

        y += rowH;
    }

    // F2H52 H5: tooltip + right-click sobre el row hovered.
    if (hoveredEntry != nullptr && !s_ctxMenu.open) {
        if (hoveredAsset != nullptr) {
            drawItemTooltip(ctx, *hoveredAsset, hoveredPath);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            openContextMenu(ImGui::GetMousePos(), *hoveredEntry, assets);
        }
    }
}

/// @brief F2H52 H3: estado de drag entre cells del Grid2D / Equipment.
///        Persiste cross-frame (static local). slot_index = -1 = no drag.
struct GridDragState {
    int  fromSlot = -1;
    bool wasDown  = false;
};

/// @brief F2H52 H3: render del modo Grid2D (RE4 style). Grid WxH de cells
///        60x60 con icono placeholder + badge qty + drag entre cells.
void drawInventoryGrid2D(const HudContext& ctx,
                          Inventory::State& inv,
                          AssetManager& assets) {
    static GridDragState s_drag;

    const int W = std::max(1, inv.config.grid_width);
    const int H = std::max(1, inv.config.grid_height);
    constexpr float cellSize = 60.0f;
    constexpr float cellGap  = 4.0f;
    constexpr float padX     = 12.0f;
    constexpr float padY     = 12.0f;
    constexpr float titleH   = 28.0f;

    const float gridW = W * cellSize + (W - 1) * cellGap;
    const float gridH = H * cellSize + (H - 1) * cellGap;
    const float panelW = gridW + 2 * padX;
    const float panelH = titleH + padY + gridH + padY;
    const float panelX = ctx.x0 + ctx.w - panelW - 20.0f;
    const float panelY = ctx.y0 + 80.0f;

    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    const std::string title = I18n::T("hud.inventory.title");
    drawTextCentered(ctx.dl, panelX + panelW * 0.5f, panelY + 6.0f,
                      title.c_str(), palette::k_orange);

    std::vector<const Inventory::Entry*> slotMap(W * H, nullptr);
    for (const auto& e : inv.entries) {
        if (e.slot_index >= 0 && e.slot_index < W * H) {
            slotMap[e.slot_index] = &e;
        }
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool   mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool   mouseClicked = mouseDown && !s_drag.wasDown;
    const bool   mouseReleased = !mouseDown && s_drag.wasDown;
    s_drag.wasDown = mouseDown;
    const bool menuOpen = s_ctxMenu.open;  // F2H52 H5

    int hoveredSlot = -1;
    const Inventory::Entry* hoveredEntry = nullptr;
    const Inventory::Asset* hoveredAsset = nullptr;
    std::string hoveredPath;

    const float gridX0 = panelX + padX;
    const float gridY0 = panelY + titleH + padY;
    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            const int slot = row * W + col;
            const float x = gridX0 + col * (cellSize + cellGap);
            const float y = gridY0 + row * (cellSize + cellGap);
            const ImVec2 a(x, y), b(x + cellSize, y + cellSize);
            const bool cellHovered = ImGui::IsMouseHoveringRect(a, b);
            if (cellHovered) hoveredSlot = slot;

            const ImU32 cellBg = cellHovered
                ? palette::k_orange_dim
                : palette::k_bg_box;
            ctx.dl->AddRectFilled(a, b, cellBg, 2.0f);
            ctx.dl->AddRect(a, b, palette::k_border, 2.0f, 0, 1.0f);

            const Inventory::Entry* entry = slotMap[slot];
            if (entry == nullptr) continue;
            const bool isDragSource = (s_drag.fromSlot == slot);
            const ImU32 textCol = isDragSource
                ? palette::k_white_dim
                : palette::k_white;

            const Inventory::Asset* asset = assets.getItem(entry->itemId);
            const std::string path = assets.itemPathOf(entry->itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            if (cellHovered) {
                hoveredEntry = entry;
                hoveredAsset = asset;
                hoveredPath  = path;
            }
            char iconBuf[8] = {0};
            if (!name.empty()) iconBuf[0] = name[0];
            const ImVec2 iconSz = calcTextSizeScaled(iconBuf, 2.5f);
            drawTextScaled(ctx.dl,
                            ImVec2(x + cellSize * 0.5f - iconSz.x * 0.5f,
                                   y + cellSize * 0.5f - iconSz.y * 0.5f),
                            iconBuf, textCol, 2.5f);

            if (entry->quantity > 1) {
                char qtyBuf[16];
                std::snprintf(qtyBuf, sizeof(qtyBuf), "x%d", entry->quantity);
                const ImVec2 qsz = ImGui::CalcTextSize(qtyBuf);
                ctx.dl->AddText(
                    ImVec2(x + cellSize - qsz.x - 4.0f,
                           y + cellSize - qsz.y - 2.0f),
                    palette::k_yellow, qtyBuf);
            }
        }
    }

    // F2H52 H5: tooltip + right-click. Solo si menu cerrado para no
    // pisar el context menu con doble overlay.
    if (!menuOpen && hoveredEntry != nullptr) {
        if (hoveredAsset != nullptr) {
            drawItemTooltip(ctx, *hoveredAsset, hoveredPath);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            openContextMenu(ImGui::GetMousePos(), *hoveredEntry, assets);
        }
    }

    // Drag flank events — solo si el menu esta cerrado.
    if (!menuOpen) {
        if (mouseClicked && hoveredSlot >= 0 && slotMap[hoveredSlot] != nullptr) {
            s_drag.fromSlot = hoveredSlot;
        }
        if (mouseReleased) {
            if (s_drag.fromSlot >= 0 && hoveredSlot >= 0
                && hoveredSlot != s_drag.fromSlot) {
                inv.moveSlot(s_drag.fromSlot, hoveredSlot);
            }
            s_drag.fromSlot = -1;
        }
    } else {
        // Menu abierto: cancelar drag in-flight para que no se commitee
        // al cerrar el menu.
        s_drag.fromSlot = -1;
    }

    if (s_drag.fromSlot >= 0
        && s_drag.fromSlot < static_cast<int>(slotMap.size())) {
        const Inventory::Entry* dragged = slotMap[s_drag.fromSlot];
        if (dragged != nullptr) {
            const Inventory::Asset* asset = assets.getItem(dragged->itemId);
            const std::string path = assets.itemPathOf(dragged->itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            char iconBuf[8] = {0};
            if (!name.empty()) iconBuf[0] = name[0];
            const ImVec2 sz = calcTextSizeScaled(iconBuf, 2.5f);
            const ImVec2 ga(mousePos.x - 20, mousePos.y - 20);
            const ImVec2 gb(mousePos.x + 20, mousePos.y + 20);
            ctx.dl->AddRectFilled(ga, gb, palette::k_orange_dim, 4.0f);
            drawTextScaled(ctx.dl,
                ImVec2(mousePos.x - sz.x * 0.5f, mousePos.y - sz.y * 0.5f),
                iconBuf, palette::k_white, 2.5f);
        }
    }
}

/// @brief F2H52 H4: EquipmentSlots mode (Skyrim-style). Slots nombrados
///        verticales, drag valida tag_filter del slot destino.
void drawInventoryEquipment(const HudContext& ctx,
                             Inventory::State& inv,
                             AssetManager& assets) {
    static GridDragState s_drag;

    const auto& slots = inv.config.equipment_slots;
    constexpr float rowH    = 56.0f;
    constexpr float padX    = 12.0f;
    constexpr float padY    = 12.0f;
    constexpr float titleH  = 28.0f;
    constexpr float labelW  = 120.0f;
    constexpr float slotW   = 220.0f;

    const int nSlots = static_cast<int>(slots.size());
    const float panelW = padX + labelW + 8.0f + slotW + padX;
    const float panelH = titleH + padY + std::max(1, nSlots) * rowH + padY;
    const float panelX = ctx.x0 + ctx.w - panelW - 20.0f;
    const float panelY = ctx.y0 + 80.0f;

    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    const std::string title = I18n::T("hud.inventory.title");
    drawTextCentered(ctx.dl, panelX + panelW * 0.5f, panelY + 6.0f,
                      title.c_str(), palette::k_orange);

    if (slots.empty()) {
        const std::string empty = I18n::T("hud.inventory.empty");
        drawTextCentered(ctx.dl, panelX + panelW * 0.5f,
                          panelY + titleH + padY + 4.0f,
                          empty.c_str(), palette::k_white_dim);
        return;
    }

    std::vector<const Inventory::Entry*> slotMap(slots.size(), nullptr);
    for (const auto& e : inv.entries) {
        if (e.slot_index >= 0
            && e.slot_index < static_cast<int>(slots.size())) {
            slotMap[e.slot_index] = &e;
        }
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool   mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool   mouseClicked  = mouseDown && !s_drag.wasDown;
    const bool   mouseReleased = !mouseDown && s_drag.wasDown;
    s_drag.wasDown = mouseDown;
    const bool menuOpen = s_ctxMenu.open;  // F2H52 H5

    std::string dragItemFirstTag;
    if (s_drag.fromSlot >= 0
        && s_drag.fromSlot < static_cast<int>(slotMap.size())
        && slotMap[s_drag.fromSlot] != nullptr) {
        const Inventory::Asset* asset =
            assets.getItem(slotMap[s_drag.fromSlot]->itemId);
        if (asset != nullptr && !asset->tags.empty()) {
            dragItemFirstTag = asset->tags.front();
        }
    }

    int hoveredSlot = -1;
    const Inventory::Entry* hoveredEntry = nullptr;
    const Inventory::Asset* hoveredAsset = nullptr;
    std::string hoveredPath;

    float y = panelY + titleH + padY;
    for (int i = 0; i < nSlots; ++i) {
        const auto& slot = slots[i];

        ctx.dl->AddText(ImVec2(panelX + padX, y + rowH * 0.5f - 7.0f),
                          palette::k_yellow_dim, slot.name.c_str());

        const float sx = panelX + padX + labelW + 8.0f;
        const ImVec2 a(sx, y), b(sx + slotW, y + rowH - 4.0f);
        const bool slotHovered = ImGui::IsMouseHoveringRect(a, b);
        if (slotHovered) hoveredSlot = i;

        const bool dragOk = s_drag.fromSlot >= 0
            && (slot.tag_filter.empty() || slot.tag_filter == dragItemFirstTag);
        const ImU32 slotBg = slotHovered
            ? (dragOk ? palette::k_stamina : palette::k_orange_dim)
            : palette::k_bg_box;
        ctx.dl->AddRectFilled(a, b, slotBg, 2.0f);
        ctx.dl->AddRect(a, b, palette::k_border, 2.0f, 0, 1.0f);

        const Inventory::Entry* entry = slotMap[i];
        if (entry != nullptr) {
            const Inventory::Asset* asset = assets.getItem(entry->itemId);
            const std::string path = assets.itemPathOf(entry->itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            if (slotHovered) {
                hoveredEntry = entry;
                hoveredAsset = asset;
                hoveredPath  = path;
            }
            const bool isDragSource = (s_drag.fromSlot == i);
            ctx.dl->AddText(ImVec2(sx + 8.0f, y + 8.0f),
                              isDragSource
                                ? palette::k_white_dim
                                : palette::k_white,
                              name.c_str());
        } else if (!slot.tag_filter.empty()) {
            const std::string hint = "(" + slot.tag_filter + ")";
            ctx.dl->AddText(ImVec2(sx + 8.0f, y + 8.0f),
                              palette::k_white_dim, hint.c_str());
        }

        y += rowH;
    }

    // F2H52 H5: tooltip + right-click sobre el slot equipado hovered.
    if (!menuOpen && hoveredEntry != nullptr) {
        if (hoveredAsset != nullptr) {
            drawItemTooltip(ctx, *hoveredAsset, hoveredPath);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            openContextMenu(ImGui::GetMousePos(), *hoveredEntry, assets);
        }
    }

    if (!menuOpen) {
        if (mouseClicked && hoveredSlot >= 0 && slotMap[hoveredSlot] != nullptr) {
            s_drag.fromSlot = hoveredSlot;
        }
        if (mouseReleased) {
            if (s_drag.fromSlot >= 0 && hoveredSlot >= 0
                && hoveredSlot != s_drag.fromSlot) {
                inv.moveSlot(s_drag.fromSlot, hoveredSlot);
            }
            s_drag.fromSlot = -1;
        }
    } else {
        s_drag.fromSlot = -1;
    }

    if (s_drag.fromSlot >= 0 && slotMap[s_drag.fromSlot] != nullptr) {
        const auto* dragged = slotMap[s_drag.fromSlot];
        const Inventory::Asset* asset = assets.getItem(dragged->itemId);
        const std::string path = assets.itemPathOf(dragged->itemId);
        const std::string name = asset != nullptr
            ? displayNameOfItem(*asset, path)
            : path;
        const ImVec2 sz = ImGui::CalcTextSize(name.c_str());
        const ImVec2 ga(mousePos.x - sz.x * 0.5f - 6, mousePos.y - 12);
        const ImVec2 gb(mousePos.x + sz.x * 0.5f + 6, mousePos.y + 12);
        ctx.dl->AddRectFilled(ga, gb, palette::k_orange_dim, 4.0f);
        ctx.dl->AddText(ImVec2(mousePos.x - sz.x * 0.5f, mousePos.y - 7),
                          palette::k_white, name.c_str());
    }
}

// =============================================================
// F2H52 Bloque I — CONTAINER SPLIT MODE
// =============================================================
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

/// @brief Estado del drag entre las dos columnas del container split.
///        Static local — persiste mientras el container este abierto.
struct SplitDragState {
    int  side      = -1;  // 0 = player, 1 = container, -1 = no drag
    int  fromIndex = -1;  // index en entries[] del lado source
    bool wasDown   = false;
};

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
                            "tras dst.add ok — posible corrupcion de state");
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

} // namespace

void drawInventoryPanel(const HudContext& ctx) {
    if (ctx.scene == nullptr || ctx.assets == nullptr) return;

    Entity player = findPlayerForHud(*ctx.scene);
    if (!player) {
        // Sin player no podemos resolver el inventory — pero el menu podria
        // haber quedado abierto del frame anterior (ej. el dev borro el
        // player entity). Lo cerramos para evitar invocar use con player
        // invalido.
        if (s_ctxMenu.open) s_ctxMenu.open = false;
        return;
    }

    auto& inv = player.getComponent<InventoryComponent>().state;

    // F2H52 Bloque I: container split. Si hud.container_target apunta a
    // un entity vivo con InventoryComponent, renderear split mode en vez
    // del single-mode. Si el handle es stale (entity borrada / sin
    // inventory), auto-close para volver a single-mode sin warn ruidoso.
    Entity containerEntity;
    if (ctx.hud != nullptr && ctx.hud->container_target != 0) {
        auto& reg = ctx.scene->registry();
        const entt::entity h =
            static_cast<entt::entity>(ctx.hud->container_target);
        if (reg.valid(h) && reg.all_of<InventoryComponent>(h)) {
            containerEntity = Entity(h, ctx.scene);
        } else {
            ctx.hud->container_target = 0;
        }
    }

    // F2H52 Bloque J: si el dev registro un renderer custom, cederle todo
    // el control y skipear nuestro render default. El context menu / tooltip
    // del motor tampoco se dibujan — el dev se encarga de su UX.
    if (Inventory::Hooks::hasRenderHook()) {
        Inventory::Hooks::invokeRender(player, containerEntity);
        return;
    }

    if (containerEntity) {
        auto& containerInv =
            containerEntity.getComponent<InventoryComponent>().state;
        drawInventorySplit(ctx, inv, containerInv, *ctx.assets);
    } else {
        switch (inv.mode) {
            case Inventory::LayoutMode::FlatList:
                drawInventoryFlatList(ctx, inv, *ctx.assets); break;
            case Inventory::LayoutMode::Grid2D:
                drawInventoryGrid2D(ctx, inv, *ctx.assets); break;
            case Inventory::LayoutMode::EquipmentSlots:
                drawInventoryEquipment(ctx, inv, *ctx.assets); break;
        }
    }

    // F2H52 H5: context menu se dibuja ENCIMA del modo activo. Su input
    // se procesa aca (no en los modes) para que sea single-source-of-truth.
    drawContextMenu(ctx, player, inv);
}

} // namespace Mood::GameOverlay
