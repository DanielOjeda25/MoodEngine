#pragma once

// Header interno del modulo GameOverlay — compartido entre los .cpp
// que componen los widgets del HUD. NO publico (no incluir desde fuera
// de `src/engine/game/overlay/`).
//
// Existe para que GameOverlay_Inventory.cpp (F2H52 H) pueda reusar la
// paleta de colores + helpers de texto + invocar widgets registrados
// sin re-definirlos. El split se hizo cuando GameOverlay.cpp paso los
// 1300 lineas (limite duro 800).

#include "engine/game/overlay/GameOverlay.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace Mood::GameOverlay {

// =============================================================
// PALETA HALF-LIFE — usada por todos los widgets del HUD
// =============================================================
namespace palette {
    inline constexpr ImU32 k_orange      = IM_COL32(245, 130,  32, 255);
    inline constexpr ImU32 k_orange_dim  = IM_COL32(245, 130,  32, 180);
    inline constexpr ImU32 k_yellow      = IM_COL32(255, 204,   0, 255);
    inline constexpr ImU32 k_yellow_dim  = IM_COL32(255, 204,   0, 200);
    inline constexpr ImU32 k_cyan        = IM_COL32( 16, 240, 255, 255);
    inline constexpr ImU32 k_red         = IM_COL32(255,  48,  48, 255);
    inline constexpr ImU32 k_red_dim     = IM_COL32(255,  48,  48, 180);
    inline constexpr ImU32 k_stamina     = IM_COL32( 80, 200, 180, 255);
    inline constexpr ImU32 k_stamina_lo  = IM_COL32(255, 220,  80, 255);
    inline constexpr ImU32 k_scanline    = IM_COL32(120, 180,  80,  35);
    inline constexpr ImU32 k_white       = IM_COL32(248, 248, 248, 255);
    inline constexpr ImU32 k_white_dim   = IM_COL32(248, 248, 248, 180);
    inline constexpr ImU32 k_bg_box      = IM_COL32(  0,   0,   0, 180);
    inline constexpr ImU32 k_bg_pause    = IM_COL32(  0,   0,   0, 220);
    inline constexpr ImU32 k_bg_panel    = IM_COL32( 20,  22,  28, 240);
    inline constexpr ImU32 k_border      = IM_COL32(255, 255, 255, 110);
} // namespace palette

// =============================================================
// HELPERS DE TEXTO — usados por multiples widgets
// =============================================================

inline void drawTextCentered(ImDrawList* dl, float cx, float y,
                              const char* text, ImU32 col) {
    const ImVec2 sz = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(cx - sz.x * 0.5f, y), col, text);
}

inline void drawTextScaled(ImDrawList* dl, ImVec2 pos, const char* text,
                            ImU32 col, float scale) {
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    dl->AddText(font, fontSize, pos, col, text);
}

inline ImVec2 calcTextSizeScaled(const char* text, float scale) {
    const ImVec2 base = ImGui::CalcTextSize(text);
    return ImVec2(base.x * scale, base.y * scale);
}

inline ImU32 lerpColor(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto unpack = [](ImU32 c) {
        return ImVec4(
            static_cast<float>((c >>  0) & 0xFF) / 255.0f,
            static_cast<float>((c >>  8) & 0xFF) / 255.0f,
            static_cast<float>((c >> 16) & 0xFF) / 255.0f,
            static_cast<float>((c >> 24) & 0xFF) / 255.0f);
    };
    const ImVec4 ca = unpack(a), cb = unpack(b);
    const ImVec4 cr(
        ca.x + (cb.x - ca.x) * t,
        ca.y + (cb.y - ca.y) * t,
        ca.z + (cb.z - ca.z) * t,
        ca.w + (cb.w - ca.w) * t);
    return IM_COL32(static_cast<int>(cr.x * 255),
                    static_cast<int>(cr.y * 255),
                    static_cast<int>(cr.z * 255),
                    static_cast<int>(cr.w * 255));
}

inline ImU32 withAlpha(ImU32 c, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const ImU32 mask = 0x00FFFFFFu;
    const ImU32 a = static_cast<ImU32>(alpha * 255.0f) << 24;
    return (c & mask) | a;
}

// =============================================================
// WIDGETS DEFINIDOS EN OTROS .CPP DEL MODULO
// =============================================================

/// @brief Widget `inventory_panel` (F2H52 H). Definido en
///        `GameOverlay_Inventory.cpp`. Decide el modo (FlatList / Grid2D
///        / EquipmentSlots) y delega. Skipea silenciosamente si no hay
///        scene/assets o no hay player con InventoryComponent.
void drawInventoryPanel(const HudContext& ctx);

} // namespace Mood::GameOverlay
