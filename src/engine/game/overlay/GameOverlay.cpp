#include "engine/game/overlay/GameOverlay.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"  // F2H52 H
#include "engine/dialog/DialogAsset.h"   // F2H48
#include "engine/dialog/DialogSystem.h"  // F2H48
#include "engine/game/state/GameState.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/inventory/InventoryState.h"   // F2H52 H
#include "engine/inventory/ItemAsset.h"        // F2H52 H
#include "engine/scene/components/Components.h"  // F2H52 H
#include "engine/scene/core/Entity.h"            // F2H52 H
#include "engine/scene/core/Scene.h"             // F2H52 H

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Mood::GameOverlay {

// =============================================================
// PALETA HALF-LIFE — el motor se inspira en HL, el HUD honra eso
// =============================================================
namespace palette {
    // Naranja Valve (selected outline + acento principal).
    constexpr ImU32 k_orange      = IM_COL32(245, 130,  32, 255);
    constexpr ImU32 k_orange_dim  = IM_COL32(245, 130,  32, 180);
    // Amarillo HL (pickup + interact prompts).
    constexpr ImU32 k_yellow      = IM_COL32(255, 204,   0, 255);
    constexpr ImU32 k_yellow_dim  = IM_COL32(255, 204,   0, 200);
    // Cyan hit marker (CoD/Doom).
    constexpr ImU32 k_cyan        = IM_COL32( 16, 240, 255, 255);
    // Rojo damage (low HP shift + vignette).
    constexpr ImU32 k_red         = IM_COL32(255,  48,  48, 255);
    constexpr ImU32 k_red_dim     = IM_COL32(255,  48,  48, 180);
    // F2H41: stamina (verde-azul Skyrim/Metro) + warn amarillo.
    constexpr ImU32 k_stamina     = IM_COL32( 80, 200, 180, 255);
    constexpr ImU32 k_stamina_lo  = IM_COL32(255, 220,  80, 255);
    // F2H41: scanline tint verdoso (Pip-Boy CRT).
    constexpr ImU32 k_scanline    = IM_COL32(120, 180,  80,  35);
    // Blanco texto neutro.
    constexpr ImU32 k_white       = IM_COL32(248, 248, 248, 255);
    constexpr ImU32 k_white_dim   = IM_COL32(248, 248, 248, 180);
    // Backgrounds.
    constexpr ImU32 k_bg_box      = IM_COL32(  0,   0,   0, 180);
    constexpr ImU32 k_bg_pause    = IM_COL32(  0,   0,   0, 220);
    constexpr ImU32 k_bg_panel    = IM_COL32( 20,  22,  28, 240);
    constexpr ImU32 k_border      = IM_COL32(255, 255, 255, 110);
}

// =============================================================
// HELPERS
// =============================================================
namespace {

// Texto centrado horizontalmente en un rect, vertical-alineado por baseline arriba.
void drawTextCentered(ImDrawList* dl, float cx, float y, const char* text, ImU32 col) {
    const ImVec2 sz = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(cx - sz.x * 0.5f, y), col, text);
}

// Texto escalado (font scale != 1.0) usando AddText con FontSize override.
// Util para los numeros grandes de HP/MAG/etc.
void drawTextScaled(ImDrawList* dl, ImVec2 pos, const char* text,
                    ImU32 col, float scale) {
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * scale;
    dl->AddText(font, fontSize, pos, col, text);
}

// CalcTextSize escalado.
ImVec2 calcTextSizeScaled(const char* text, float scale) {
    const ImVec2 base = ImGui::CalcTextSize(text);
    return ImVec2(base.x * scale, base.y * scale);
}

// Lerp lineal de color (componente a componente).
ImU32 lerpColor(ImU32 a, ImU32 b, float t) {
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

// Override del alpha de un color (resto preservado).
ImU32 withAlpha(ImU32 c, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    const ImU32 mask = 0x00FFFFFFu;
    const ImU32 a = static_cast<ImU32>(alpha * 255.0f) << 24;
    return (c & mask) | a;
}

// =============================================================
// WIDGETS — cada uno es una funcion libre que pinta sobre HudContext
// =============================================================

// 1. HEALTH NUMBER — bottom-left, numero grande naranja, color shift
//    a rojo cuando hp < 25%.
void drawHealthNumber(const HudContext& ctx) {
    const HudState& h = *ctx.hud;
    const float pad = 24.0f;
    const float originX = ctx.x0 + pad;
    const float originY = ctx.y0 + ctx.h - pad - 80.0f;

    // Color shift: lerp de naranja a rojo proporcional a (1 - hp%).
    const float ratio = (h.max_hp > 0)
        ? std::clamp(static_cast<float>(h.hp) / static_cast<float>(h.max_hp), 0.0f, 1.0f)
        : 0.0f;
    const float lowFactor = (ratio < 0.25f) ? (1.0f - ratio / 0.25f) : 0.0f;
    const ImU32 valCol = lerpColor(palette::k_orange, palette::k_red, lowFactor);

    // Background box semi-transparente.
    constexpr float bw = 180.0f;
    constexpr float bh = 80.0f;
    ctx.dl->AddRectFilled(ImVec2(originX, originY),
                           ImVec2(originX + bw, originY + bh),
                           palette::k_bg_box, 4.0f);
    ctx.dl->AddRect(ImVec2(originX, originY),
                     ImVec2(originX + bw, originY + bh),
                     palette::k_border, 4.0f, 0, 1.5f);

    // Label "HEALTH" chiquito arriba (estilo HL). F2H43: i18n via T().
    const std::string healthLbl = I18n::T("hud.label.health");
    ctx.dl->AddText(ImVec2(originX + 12.0f, originY + 8.0f),
                     palette::k_white_dim, healthLbl.c_str());

    // Numero grande del HP — escalado 2.5x.
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", h.hp);
    drawTextScaled(ctx.dl, ImVec2(originX + 12.0f, originY + 24.0f),
                   buf, valCol, 2.8f);

    // Barra horizontal procedural delgada al lado del numero.
    const float barX = originX + 100.0f;
    const float barY = originY + 38.0f;
    const float barW = bw - 110.0f;
    const float barH = 18.0f;
    ctx.dl->AddRect(ImVec2(barX, barY),
                     ImVec2(barX + barW, barY + barH),
                     palette::k_border, 2.0f, 0, 1.0f);
    ctx.dl->AddRectFilled(ImVec2(barX + 2.0f, barY + 2.0f),
                           ImVec2(barX + 2.0f + (barW - 4.0f) * ratio,
                                  barY + barH - 2.0f),
                           valCol, 1.0f);
}

// 2. AMMO COUNTER — bottom-right. Mag/reserve split estilo HL/CoD.
//    "MAG: 30/30" grande + "RESERVE: 90" abajo mas chico + icono
//    procedural de bala (rectangulo + triangulo) al lado.
void drawAmmoCounter(const HudContext& ctx) {
    const HudState& h = *ctx.hud;
    const float pad = 24.0f;
    constexpr float bw = 220.0f;
    constexpr float bh = 80.0f;
    const float originX = ctx.x0 + ctx.w - pad - bw;
    const float originY = ctx.y0 + ctx.h - pad - bh;

    // Background box.
    ctx.dl->AddRectFilled(ImVec2(originX, originY),
                           ImVec2(originX + bw, originY + bh),
                           palette::k_bg_box, 4.0f);
    ctx.dl->AddRect(ImVec2(originX, originY),
                     ImVec2(originX + bw, originY + bh),
                     palette::k_border, 4.0f, 0, 1.5f);

    // Label "AMMO" arriba derecha. F2H43: i18n.
    const std::string ammoLbl = I18n::T("hud.label.ammo");
    const ImVec2 lblSz = ImGui::CalcTextSize(ammoLbl.c_str());
    ctx.dl->AddText(ImVec2(originX + bw - 12.0f - lblSz.x, originY + 8.0f),
                     palette::k_white_dim, ammoLbl.c_str());

    // Numero MAG grande — escalado 2.8x.
    char magBuf[24];
    std::snprintf(magBuf, sizeof(magBuf), "%d", h.mag);
    const ImVec2 magSz = calcTextSizeScaled(magBuf, 2.8f);
    drawTextScaled(ctx.dl, ImVec2(originX + 60.0f, originY + 22.0f),
                   magBuf, palette::k_orange, 2.8f);

    // "/ MAX" pequeno al lado.
    char maxBuf[16];
    std::snprintf(maxBuf, sizeof(maxBuf), "/%d", h.max_mag);
    ctx.dl->AddText(ImVec2(originX + 60.0f + magSz.x + 4.0f, originY + 28.0f),
                     palette::k_white_dim, maxBuf);

    // "RESERVE: N" abajo, mas chico. F2H43: i18n con interpolacion fmt.
    const std::string resStr = I18n::T("hud.label.reserve", h.reserve);
    const ImVec2 resSz = ImGui::CalcTextSize(resStr.c_str());
    ctx.dl->AddText(ImVec2(originX + bw - 12.0f - resSz.x, originY + bh - 18.0f),
                     palette::k_white_dim, resStr.c_str());

    // Icono procedural de bala (rectangulo + triangulo) a la izquierda.
    const float iconX = originX + 16.0f;
    const float iconY = originY + bh * 0.5f - 10.0f;
    // Cuerpo: rectangulo amarillo.
    ctx.dl->AddRectFilled(ImVec2(iconX, iconY),
                           ImVec2(iconX + 22.0f, iconY + 18.0f),
                           palette::k_yellow_dim, 2.0f);
    // Punta: triangulo gris-blanco.
    ctx.dl->AddTriangleFilled(ImVec2(iconX + 22.0f, iconY),
                                ImVec2(iconX + 22.0f, iconY + 18.0f),
                                ImVec2(iconX + 32.0f, iconY + 9.0f),
                                palette::k_white_dim);
}

// 3. CROSSHAIR — dot central + cruz delgada outline. Estilo HL2/CoD.
void drawCrosshair(const HudContext& ctx) {
    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float cy = ctx.y0 + ctx.h * 0.5f;
    constexpr float r       = 8.0f;
    constexpr float gap     = 3.0f;  // hueco entre dot y cruz
    constexpr float thick   = 1.5f;
    constexpr float thickOut= 3.0f;

    // Outline negro detras (legibilidad sobre fondos claros).
    const ImU32 outline = IM_COL32(0, 0, 0, 200);
    ctx.dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx - gap, cy), outline, thickOut);
    ctx.dl->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + r, cy), outline, thickOut);
    ctx.dl->AddLine(ImVec2(cx, cy - r), ImVec2(cx, cy - gap), outline, thickOut);
    ctx.dl->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + r), outline, thickOut);

    // Cruz blanca encima.
    ctx.dl->AddLine(ImVec2(cx - r, cy), ImVec2(cx - gap, cy), palette::k_white, thick);
    ctx.dl->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + r, cy), palette::k_white, thick);
    ctx.dl->AddLine(ImVec2(cx, cy - r), ImVec2(cx, cy - gap), palette::k_white, thick);
    ctx.dl->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + r), palette::k_white, thick);

    // Dot central.
    ctx.dl->AddCircleFilled(ImVec2(cx, cy), 1.5f, palette::k_white);
}

// 4. HIT MARKER — cruz cyan + (mas grande que el crosshair) transient
//    cuando hit_marker_t > 0. Decae cada frame con dt.
void drawHitMarker(const HudContext& ctx) {
    HudState& h = *ctx.hud;
    if (h.hit_marker_t <= 0.0f) return;
    h.hit_marker_t = std::max(0.0f, h.hit_marker_t - ctx.dt);

    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float cy = ctx.y0 + ctx.h * 0.5f;
    // El marker dura 0.3s; alpha fade en los ultimos 0.15s.
    const float alpha = std::clamp(h.hit_marker_t / 0.3f, 0.0f, 1.0f);
    const ImU32 col = withAlpha(palette::k_cyan, alpha);

    // Cruz X grande (45 deg) — 4 lineas desde el centro.
    constexpr float inner = 6.0f;
    constexpr float outer = 14.0f;
    constexpr float thick = 2.5f;
    const float dInner = inner * 0.7071f; // cos/sin 45deg
    const float dOuter = outer * 0.7071f;
    ctx.dl->AddLine(ImVec2(cx - dOuter, cy - dOuter),
                     ImVec2(cx - dInner, cy - dInner), col, thick);
    ctx.dl->AddLine(ImVec2(cx + dInner, cy - dInner),
                     ImVec2(cx + dOuter, cy - dOuter), col, thick);
    ctx.dl->AddLine(ImVec2(cx - dOuter, cy + dOuter),
                     ImVec2(cx - dInner, cy + dInner), col, thick);
    ctx.dl->AddLine(ImVec2(cx + dInner, cy + dInner),
                     ImVec2(cx + dOuter, cy + dOuter), col, thick);
}

// 5. DAMAGE VIGNETTE — radial rojo + arco direccional cuando damage_t > 0.
//    Decae cada frame con dt. damage_dir = vec2 normalized (de donde viene).
void drawDamageVignette(const HudContext& ctx) {
    HudState& h = *ctx.hud;
    if (h.damage_t <= 0.0f) return;
    h.damage_t = std::max(0.0f, h.damage_t - ctx.dt);

    // Alpha fade — el flash dura 0.5s con curva quadratica para que el
    // peak inicial sea fuerte y caiga rapido.
    const float t = std::clamp(h.damage_t / 0.5f, 0.0f, 1.0f);
    const float alpha = t * t * 0.5f; // peak 0.5

    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float cy = ctx.y0 + ctx.h * 0.5f;

    // Arco direccional: si magnitud > 0, dibujar 4 quad segments
    // gruesos en el lado correspondiente del rect. Implementacion
    // simple: 4 rect bands en cada borde, alpha modulada por dot
    // product con damage_dir.
    const float magnitude = std::sqrt(h.damage_dir.x * h.damage_dir.x +
                                        h.damage_dir.y * h.damage_dir.y);
    if (magnitude > 0.01f) {
        const glm::vec2 d = h.damage_dir / magnitude;
        // Dot product con cada borde: top=(0,1), right=(1,0), bottom=(0,-1), left=(-1,0).
        const float top    = std::max(0.0f,  d.y);
        const float right  = std::max(0.0f,  d.x);
        const float bottom = std::max(0.0f, -d.y);
        const float left   = std::max(0.0f, -d.x);
        constexpr float bandThick = 60.0f;
        const ImU32 baseCol = palette::k_red;

        // Top band.
        if (top > 0.01f) {
            ctx.dl->AddRectFilledMultiColor(
                ImVec2(ctx.x0, ctx.y0),
                ImVec2(ctx.x0 + ctx.w, ctx.y0 + bandThick),
                withAlpha(baseCol, alpha * top),
                withAlpha(baseCol, alpha * top),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, 0.0f));
        }
        // Bottom band.
        if (bottom > 0.01f) {
            ctx.dl->AddRectFilledMultiColor(
                ImVec2(ctx.x0, ctx.y0 + ctx.h - bandThick),
                ImVec2(ctx.x0 + ctx.w, ctx.y0 + ctx.h),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, alpha * bottom),
                withAlpha(baseCol, alpha * bottom));
        }
        // Right band.
        if (right > 0.01f) {
            ctx.dl->AddRectFilledMultiColor(
                ImVec2(ctx.x0 + ctx.w - bandThick, ctx.y0),
                ImVec2(ctx.x0 + ctx.w, ctx.y0 + ctx.h),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, alpha * right),
                withAlpha(baseCol, alpha * right),
                withAlpha(baseCol, 0.0f));
        }
        // Left band.
        if (left > 0.01f) {
            ctx.dl->AddRectFilledMultiColor(
                ImVec2(ctx.x0, ctx.y0),
                ImVec2(ctx.x0 + bandThick, ctx.y0 + ctx.h),
                withAlpha(baseCol, alpha * left),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, 0.0f),
                withAlpha(baseCol, alpha * left));
        }
        (void)cx; (void)cy;
    } else {
        // Vignette radial: 4 bandas suaves desde los bordes al centro.
        constexpr float bandThick = 100.0f;
        const ImU32 col = withAlpha(palette::k_red, alpha * 0.6f);
        const ImU32 transp = withAlpha(palette::k_red, 0.0f);
        ctx.dl->AddRectFilledMultiColor(
            ImVec2(ctx.x0, ctx.y0),
            ImVec2(ctx.x0 + ctx.w, ctx.y0 + bandThick),
            col, col, transp, transp);
        ctx.dl->AddRectFilledMultiColor(
            ImVec2(ctx.x0, ctx.y0 + ctx.h - bandThick),
            ImVec2(ctx.x0 + ctx.w, ctx.y0 + ctx.h),
            transp, transp, col, col);
        ctx.dl->AddRectFilledMultiColor(
            ImVec2(ctx.x0, ctx.y0),
            ImVec2(ctx.x0 + bandThick, ctx.y0 + ctx.h),
            col, transp, transp, col);
        ctx.dl->AddRectFilledMultiColor(
            ImVec2(ctx.x0 + ctx.w - bandThick, ctx.y0),
            ImVec2(ctx.x0 + ctx.w, ctx.y0 + ctx.h),
            transp, col, col, transp);
    }
}

// 6. INTERACT PROMPT — texto centrado abajo del crosshair. Visible
//    mientras hud.interact_prompt no este vacio.
void drawInteractPrompt(const HudContext& ctx) {
    const HudState& h = *ctx.hud;
    if (h.interact_prompt.empty()) return;

    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float cy = ctx.y0 + ctx.h * 0.5f + 60.0f; // 60 px debajo del crosshair

    // Background box ajustado al texto + padding.
    const ImVec2 sz = ImGui::CalcTextSize(h.interact_prompt.c_str());
    constexpr float padX = 16.0f, padY = 6.0f;
    const ImVec2 a(cx - sz.x * 0.5f - padX, cy - padY);
    const ImVec2 b(cx + sz.x * 0.5f + padX, cy + sz.y + padY);
    ctx.dl->AddRectFilled(a, b, palette::k_bg_box, 3.0f);
    ctx.dl->AddRect(a, b, palette::k_yellow_dim, 3.0f, 0, 1.5f);

    drawTextCentered(ctx.dl, cx, cy, h.interact_prompt.c_str(),
                      palette::k_yellow);
}

// 7. PICKUP NOTIFICATION — toast en columna bottom-center. Cada
//    notification tiene ttl decreciente (init 2.5s en pushPickup);
//    fade in/out segun ttl. Las expiradas se popean por TTL <= 0.
//    Maximo ~5 visibles.
void drawPickupNotifications(const HudContext& ctx) {
    HudState& h = *ctx.hud;
    constexpr float k_lifetime = 2.5f;
    constexpr float k_fadeIn   = 0.15f;
    constexpr float k_fadeOut  = 0.6f;

    // Decrementar ttl + popear expiradas (front; FIFO).
    for (auto& n : h.pickup_queue) n.ttl -= ctx.dt;
    while (!h.pickup_queue.empty() && h.pickup_queue.front().ttl <= 0.0f) {
        h.pickup_queue.pop_front();
    }
    if (h.pickup_queue.empty()) return;

    // Layout: stack vertical desde abajo, cerca del HP (no pisar).
    const float baseY = ctx.y0 + ctx.h - 130.0f;
    const float cx = ctx.x0 + ctx.w * 0.5f;
    constexpr float rowH = 26.0f;

    int i = 0;
    for (const auto& n : h.pickup_queue) {
        const float age = k_lifetime - n.ttl;
        // Alpha fade in/out.
        float alpha = 1.0f;
        if (age < k_fadeIn) alpha = age / k_fadeIn;
        else if (n.ttl < k_fadeOut)
            alpha = n.ttl / k_fadeOut;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        const float y = baseY - i * rowH;
        const ImVec2 sz = ImGui::CalcTextSize(n.text.c_str());
        constexpr float padX = 12.0f, padY = 4.0f;
        const ImVec2 a(cx - sz.x * 0.5f - padX, y - padY);
        const ImVec2 b(cx + sz.x * 0.5f + padX, y + sz.y + padY);
        ctx.dl->AddRectFilled(a, b,
                                withAlpha(palette::k_bg_box, alpha * 0.85f),
                                3.0f);
        drawTextCentered(ctx.dl, cx, y, n.text.c_str(),
                          withAlpha(palette::k_yellow, alpha));
        ++i;
        if (i >= 5) break; // cap visual
    }
}

// 8. PAUSE MENU — dim background + titulo grande + 3 botones con
//    border geometrico (chevrons en los bordes). Doom-style.
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

// =============================================================
// F2H41 WIDGETS — extension del paquete inicial F2H39
// =============================================================

// 9. STAMINA BAR — bottom-left, debajo del HealthNumber. Barra
//    horizontal pequena verde-azul (Skyrim/Metro). Color shift a
//    amarillo cuando stamina < 30%.
void drawStaminaBar(const HudContext& ctx) {
    const HudState& h = *ctx.hud;
    if (h.max_stamina <= 0) return; // bypass si gameplay no usa stamina
    const float ratio = std::clamp(
        static_cast<float>(h.stamina) / static_cast<float>(h.max_stamina),
        0.0f, 1.0f);
    const ImU32 col = (ratio < 0.30f) ? palette::k_stamina_lo : palette::k_stamina;

    // Justo encima del HealthNumber (que ocupa 80 px alto desde
    // bottom). StaminaBar es chica: 180 ancho x 14 alto. La pegamos
    // arriba del HP box para hacer un grupo visual.
    const float pad = 24.0f;
    const float originX = ctx.x0 + pad;
    const float originY = ctx.y0 + ctx.h - pad - 80.0f - 22.0f;
    constexpr float bw = 180.0f;
    constexpr float bh = 14.0f;

    // Background + border.
    ctx.dl->AddRectFilled(ImVec2(originX, originY),
                           ImVec2(originX + bw, originY + bh),
                           palette::k_bg_box, 3.0f);
    ctx.dl->AddRect(ImVec2(originX, originY),
                     ImVec2(originX + bw, originY + bh),
                     palette::k_border, 3.0f, 0, 1.0f);

    // Fill horizontal proporcional.
    if (ratio > 0.0f) {
        ctx.dl->AddRectFilled(
            ImVec2(originX + 2.0f, originY + 2.0f),
            ImVec2(originX + 2.0f + (bw - 4.0f) * ratio, originY + bh - 2.0f),
            col, 2.0f);
    }

    // Label "STAMINA" mini al ras del extremo izquierdo. F2H43: i18n.
    const std::string staminaLbl = I18n::T("hud.label.stamina");
    ctx.dl->AddText(ImVec2(originX + 6.0f, originY - 2.0f),
                     palette::k_white_dim, staminaLbl.c_str());
}

// 10. OBJECTIVE TEXT — top-left, debajo del MenuBar (~24 px desde
//     top). Caja oscura con border amarillo HL + texto blanco. El
//     widget agrega el prefijo "OBJETIVO: " automaticamente.
void drawObjectiveText(const HudContext& ctx) {
    const HudState& h = *ctx.hud;
    if (h.objective_text.empty()) return;

    constexpr float pad = 24.0f;
    const float originX = ctx.x0 + pad;
    const float originY = ctx.y0 + pad;

    // F2H43: prefix i18n con interpolacion fmt.
    const std::string objStr = I18n::T("hud.label.objective_prefix",
                                         h.objective_text);
    const ImVec2 sz = ImGui::CalcTextSize(objStr.c_str());
    constexpr float padX = 14.0f, padY = 8.0f;
    const ImVec2 a(originX, originY);
    const ImVec2 b(originX + sz.x + padX * 2.0f, originY + sz.y + padY * 2.0f);
    ctx.dl->AddRectFilled(a, b, palette::k_bg_box, 3.0f);
    ctx.dl->AddRect(a, b, palette::k_yellow, 3.0f, 0, 1.5f);
    ctx.dl->AddText(ImVec2(originX + padX, originY + padY),
                     palette::k_white, objStr.c_str());
}

// 11. KILL FEED — top-right, columna stack vertical. Toast rojo con
//     texto custom-color. Cap visual 5 entradas. Lifetime 4s con
//     fade in/out (mismo patron que pickup_queue).
void drawKillFeed(const HudContext& ctx) {
    HudState& h = *ctx.hud;
    constexpr float k_lifetime = 4.0f;
    constexpr float k_fadeIn   = 0.15f;
    constexpr float k_fadeOut  = 0.8f;

    // Decrementar ttl + popear expiradas (front; FIFO).
    for (auto& e : h.kill_feed) e.ttl -= ctx.dt;
    while (!h.kill_feed.empty() && h.kill_feed.front().ttl <= 0.0f) {
        h.kill_feed.pop_front();
    }
    if (h.kill_feed.empty()) return;

    // Layout: stack vertical desde arriba derecha (comienza ~24 px
    // bajo el menu bar — el ObjectiveText ya cubre top-left).
    constexpr float pad = 24.0f;
    constexpr float rowH = 26.0f;
    const float baseY = ctx.y0 + pad + 60.0f; // bajo objective text
    const float rightX = ctx.x0 + ctx.w - pad;

    int i = 0;
    for (const auto& e : h.kill_feed) {
        const float age = k_lifetime - e.ttl;
        float alpha = 1.0f;
        if (age < k_fadeIn) alpha = age / k_fadeIn;
        else if (e.ttl < k_fadeOut) alpha = e.ttl / k_fadeOut;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        const float y = baseY + i * rowH;
        const ImVec2 sz = ImGui::CalcTextSize(e.text.c_str());
        constexpr float padX = 12.0f, padY = 4.0f;
        const ImVec2 a(rightX - sz.x - padX * 2.0f, y);
        const ImVec2 b(rightX, y + sz.y + padY * 2.0f);
        ctx.dl->AddRectFilled(a, b,
                                withAlpha(palette::k_red_dim, alpha * 0.85f),
                                3.0f);
        ctx.dl->AddText(ImVec2(a.x + padX, y + padY),
                         withAlpha(e.color, alpha), e.text.c_str());
        ++i;
        if (i >= 5) break; // cap visual
    }
}

// 12. COMPASS BAR — top-center, debajo del MenuBar. Barra horizontal
//     con tickmarks cada 15 grados y letras N/E/S/W. Indicador
//     central (triangulo naranja apuntando abajo) marca yaw actual.
//     Yaw derivado del cameraForward del HudContext.
void drawCompassBar(const HudContext& ctx) {
    // Yaw en grados desde el forward XZ. Convencion: yaw=0 mira -Z
    // (norte), yaw=90 mira +X (este), yaw=180 mira +Z (sur),
    // yaw=270 mira -X (oeste). atan2 con (forward.x, -forward.z)
    // satisface esta convencion.
    const glm::vec3& f = ctx.cameraForward;
    if (std::abs(f.x) < 1e-6f && std::abs(f.z) < 1e-6f) return; // cam vertical
    const float yawRad = std::atan2(f.x, -f.z);
    const float yawDeg = yawRad * 57.2957795f; // rad -> deg
    // Normalizar a [0, 360).
    auto normDeg = [](float d) {
        while (d < 0.0f)    d += 360.0f;
        while (d >= 360.0f) d -= 360.0f;
        return d;
    };
    const float yaw = normDeg(yawDeg);

    // Layout: barra de 480 px ancho centrada arriba.
    constexpr float pad = 24.0f;
    constexpr float barW = 480.0f;
    constexpr float barH = 28.0f;
    const float barX = ctx.x0 + (ctx.w - barW) * 0.5f;
    const float barY = ctx.y0 + pad;

    // Background + border.
    ctx.dl->AddRectFilled(ImVec2(barX, barY),
                           ImVec2(barX + barW, barY + barH),
                           palette::k_bg_box, 2.0f);
    ctx.dl->AddRect(ImVec2(barX, barY),
                     ImVec2(barX + barW, barY + barH),
                     palette::k_border, 2.0f, 0, 1.0f);

    // Visualizamos +-90 grados desde el yaw actual (180 grados de
    // rango total). Cada grado son barW/180 px.
    constexpr float visibleRange = 180.0f;
    const float pxPerDeg = barW / visibleRange;
    const float cx = barX + barW * 0.5f;

    // Tickmarks cada 15 grados.
    for (int t = 0; t < 360; t += 15) {
        const float deltaSigned = static_cast<float>(t) - yaw;
        // wrap a [-180, 180].
        float d = deltaSigned;
        while (d >  180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        if (std::abs(d) > visibleRange * 0.5f) continue;

        const float px = cx + d * pxPerDeg;
        const bool isCardinal = (t % 90) == 0;
        const float tickH = isCardinal ? barH * 0.6f : barH * 0.3f;
        const ImU32 tickCol = isCardinal ? palette::k_white : palette::k_white_dim;
        ctx.dl->AddLine(ImVec2(px, barY + barH * 0.5f),
                          ImVec2(px, barY + barH * 0.5f + tickH),
                          tickCol, isCardinal ? 1.5f : 1.0f);

        if (isCardinal) {
            const char* lbl = "?";
            switch (t) {
                case   0: lbl = "N"; break;
                case  90: lbl = "E"; break;
                case 180: lbl = "S"; break;
                case 270: lbl = "W"; break;
            }
            const ImVec2 lblSz = ImGui::CalcTextSize(lbl);
            ctx.dl->AddText(ImVec2(px - lblSz.x * 0.5f, barY + 2.0f),
                              palette::k_white, lbl);
        }
    }

    // Indicador central (triangulo naranja apuntando abajo) — marca yaw actual.
    constexpr float triH = 8.0f;
    constexpr float triW = 6.0f;
    ctx.dl->AddTriangleFilled(
        ImVec2(cx - triW, barY - 2.0f),
        ImVec2(cx + triW, barY - 2.0f),
        ImVec2(cx, barY - 2.0f + triH),
        palette::k_orange);
}

// 13. CRT SCANLINE OVERLAY — full-screen overlay con lineas
//     horizontales semi-transparentes verdosas (~12% alpha), cada
//     3 px. Default OFF (toggle desde Lua via setWidget).
void drawCrtScanline(const HudContext& ctx) {
    constexpr float spacing = 3.0f;
    const float yEnd = ctx.y0 + ctx.h;
    for (float y = ctx.y0; y < yEnd; y += spacing) {
        ctx.dl->AddLine(ImVec2(ctx.x0,            y),
                          ImVec2(ctx.x0 + ctx.w,  y),
                          palette::k_scanline,
                          1.0f);
    }
}

// F2H48: DIALOG BOX — caja inferior estilo Half-Life 2 con el texto
// del NPC + las choices disponibles. Solo dibuja si DialogSystem::
// isActive(). Detecta el i18n resolution del label_key/text_key:
// label_literal/text_literal tiene prioridad sobre la key (paridad con
// la convencion de DialogAsset::Line).
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

// F2H52 H: INVENTORY PANEL — panel del inventario del player.
// Toggle con tecla Tab (input handler en el caller cambia
// widget_enabled["inventory_panel"]) o via Lua `hud.set_widget`.
// Default OFF. Skipea silenciosamente si scene/assets son null o
// no hay entity con tag "Player" + InventoryComponent.
//
// 3 modes segun `InventoryComponent.state.mode`:
//   - FlatList: tabla con icono(reservado) + nombre + qty.
//   - Grid2D: grid WxH de cells (TODO H3).
//   - EquipmentSlots: lista vertical labeled (TODO H4).
//
// Engine-grade: el widget SOLO lee `Inventory::State` y `Inventory::Asset`.
// Las acciones (use/drop) van por right-click menu (H5) que invoca los
// hooks Lua o llama directo a `inventory.remove` para drop.
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
        // Stem del path (sin extension).
        const auto slash = pathFallback.find_last_of('/');
        const auto dot   = pathFallback.find_last_of('.');
        const auto beg   = (slash == std::string::npos) ? 0 : slash + 1;
        const auto end   = (dot == std::string::npos || dot < beg)
                                ? pathFallback.size() : dot;
        return pathFallback.substr(beg, end - beg);
    }
    return "(unknown)";
}

/// @brief Renderea el modo FlatList: lista vertical de filas
///        "nombre x qty". Cada fila es un rect clickeable (placeholder
///        para H5 — right-click menu).
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

    // Background + border.
    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    // Titulo.
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

    // Filas.
    float y = panelY + titleH + padY;
    for (const auto& e : inv.entries) {
        const Inventory::Asset* asset = assets.getItem(e.itemId);
        const std::string path = assets.itemPathOf(e.itemId);
        const std::string name = asset != nullptr
            ? displayNameOfItem(*asset, path)
            : path;

        // Hover detection (placeholder para tooltip H5).
        const ImVec2 rowA(panelX + padX, y);
        const ImVec2 rowB(panelX + panelW - padX, y + rowH - 4.0f);
        const bool hovered = ImGui::IsMouseHoveringRect(rowA, rowB);
        if (hovered) {
            ctx.dl->AddRectFilled(rowA, rowB, palette::k_orange_dim, 2.0f);
        }

        // Nombre a la izquierda.
        ctx.dl->AddText(ImVec2(panelX + padX + 4.0f, y + 4.0f),
                          palette::k_white, name.c_str());

        // qty a la derecha (formato "x N", solo si > 1).
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
}

/// @brief F2H52 H3: estado de drag entre cells del Grid2D. Persiste
///        cross-frame (static local). slot_index = -1 = no drag activo.
///        Reset al exitPlayMode via `GameState::reset()` no toca esto,
///        pero el mouse up siempre limpia, asi que en la practica no
///        sobrevive entre sesiones de play.
struct GridDragState {
    int  fromSlot = -1;    // slot origen del drag (-1 = no drag)
    bool wasDown  = false; // tracker del flanco mouse-down
};

/// @brief Renderea el modo Grid2D (Resident Evil 4 style): grid WxH de
///        cells 64x64 con item placeholder + badge qty + drag entre cells
///        (moveSlot). Cells vacios son slots disponibles. Items sin
///        slot asignado (-1) se reparten al primer cell libre on-the-fly
///        para el render — pero NO mutan state.entries (es solo display).
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

    // Background + border.
    ctx.dl->AddRectFilled(ImVec2(panelX, panelY),
                           ImVec2(panelX + panelW, panelY + panelH),
                           palette::k_bg_panel, 4.0f);
    ctx.dl->AddRect(ImVec2(panelX, panelY),
                     ImVec2(panelX + panelW, panelY + panelH),
                     palette::k_border, 4.0f, 0, 1.5f);

    const std::string title = I18n::T("hud.inventory.title");
    drawTextCentered(ctx.dl, panelX + panelW * 0.5f, panelY + 6.0f,
                      title.c_str(), palette::k_orange);

    // Map slot_index -> Entry para lookup O(1).
    std::vector<const Inventory::Entry*> slotMap(W * H, nullptr);
    for (const auto& e : inv.entries) {
        if (e.slot_index >= 0 && e.slot_index < W * H) {
            slotMap[e.slot_index] = &e;
        }
    }

    // Estado del mouse para drag detection.
    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool   mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool   mouseClicked = mouseDown && !s_drag.wasDown;  // edge down
    const bool   mouseReleased = !mouseDown && s_drag.wasDown; // edge up
    s_drag.wasDown = mouseDown;

    int hoveredSlot = -1;

    // Render cells + hit-test.
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

            // Background del cell.
            const ImU32 cellBg = cellHovered
                ? palette::k_orange_dim
                : palette::k_bg_box;
            ctx.dl->AddRectFilled(a, b, cellBg, 2.0f);
            ctx.dl->AddRect(a, b, palette::k_border, 2.0f, 0, 1.0f);

            // Contenido del cell si hay item.
            const Inventory::Entry* entry = slotMap[slot];
            if (entry == nullptr) continue;
            // Si estoy dragueando este slot, dibujarlo "transparente" en
            // su pos original — el "ghost" va al cursor.
            const bool isDragSource = (s_drag.fromSlot == slot);
            const ImU32 textCol = isDragSource
                ? palette::k_white_dim
                : palette::k_white;

            // Placeholder "icon" = primera letra del nombre.
            const Inventory::Asset* asset = assets.getItem(entry->itemId);
            const std::string path = assets.itemPathOf(entry->itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            char iconBuf[8] = {0};
            if (!name.empty()) iconBuf[0] = name[0];
            const ImVec2 iconSz = calcTextSizeScaled(iconBuf, 2.5f);
            drawTextScaled(ctx.dl,
                            ImVec2(x + cellSize * 0.5f - iconSz.x * 0.5f,
                                   y + cellSize * 0.5f - iconSz.y * 0.5f),
                            iconBuf, textCol, 2.5f);

            // Badge qty si > 1 (esquina inferior-derecha).
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

    // Drag logic — flank events.
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
    // Cancelar drag si mouse salio del grid sin release pendiente.
    // (No cancelamos automaticamente — se cancela en mouseReleased fuera
    // del slot destino. mejor UX: el drag persiste mientras el boton
    // siga apretado, sin importar donde este el cursor.)

    // Dibujar el "ghost" del item dragueado en el cursor.
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
            // Translucent rect + text bajo el cursor.
            const ImVec2 ga(mousePos.x - 20, mousePos.y - 20);
            const ImVec2 gb(mousePos.x + 20, mousePos.y + 20);
            ctx.dl->AddRectFilled(ga, gb, palette::k_orange_dim, 4.0f);
            drawTextScaled(ctx.dl,
                ImVec2(mousePos.x - sz.x * 0.5f, mousePos.y - sz.y * 0.5f),
                iconBuf, palette::k_white, 2.5f);
        }
    }
}

/// @brief F2H52 H4: EquipmentSlots mode (Skyrim equipment-style). Slots
///        nombrados verticales, cada uno con tag_filter visible en
///        tooltip (la implementacion de tooltip llega en H5). Drag entre
///        slots solo si el item arrastrado tiene el tag del slot destino.
///        Hover destaca el slot. Mismo patron de drag que Grid2D — state
///        local static.
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

    // Map slot_index -> Entry (mismo idiom que Grid2D).
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

    // Si hay drag activo, conocer su tag para validar drops.
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
    float y = panelY + titleH + padY;
    for (int i = 0; i < nSlots; ++i) {
        const auto& slot = slots[i];

        // Label del slot a la izquierda.
        ctx.dl->AddText(ImVec2(panelX + padX, y + rowH * 0.5f - 7.0f),
                          palette::k_yellow_dim, slot.name.c_str());

        // Slot box a la derecha.
        const float sx = panelX + padX + labelW + 8.0f;
        const ImVec2 a(sx, y), b(sx + slotW, y + rowH - 4.0f);
        const bool slotHovered = ImGui::IsMouseHoveringRect(a, b);
        if (slotHovered) hoveredSlot = i;

        // Validacion del drag para colorear el slot.
        const bool dragOk = s_drag.fromSlot >= 0
            && (slot.tag_filter.empty() || slot.tag_filter == dragItemFirstTag);
        const ImU32 slotBg = slotHovered
            ? (dragOk ? palette::k_stamina : palette::k_orange_dim)
            : palette::k_bg_box;
        ctx.dl->AddRectFilled(a, b, slotBg, 2.0f);
        ctx.dl->AddRect(a, b, palette::k_border, 2.0f, 0, 1.0f);

        // Contenido.
        const Inventory::Entry* entry = slotMap[i];
        if (entry != nullptr) {
            const Inventory::Asset* asset = assets.getItem(entry->itemId);
            const std::string path = assets.itemPathOf(entry->itemId);
            const std::string name = asset != nullptr
                ? displayNameOfItem(*asset, path)
                : path;
            const bool isDragSource = (s_drag.fromSlot == i);
            ctx.dl->AddText(ImVec2(sx + 8.0f, y + 8.0f),
                              isDragSource
                                ? palette::k_white_dim
                                : palette::k_white,
                              name.c_str());
        } else if (!slot.tag_filter.empty()) {
            // Hint del tag_filter cuando el slot esta vacio.
            const std::string hint = "(" + slot.tag_filter + ")";
            ctx.dl->AddText(ImVec2(sx + 8.0f, y + 8.0f),
                              palette::k_white_dim, hint.c_str());
        }

        y += rowH;
    }

    // Drag flank events.
    if (mouseClicked && hoveredSlot >= 0 && slotMap[hoveredSlot] != nullptr) {
        s_drag.fromSlot = hoveredSlot;
    }
    if (mouseReleased) {
        if (s_drag.fromSlot >= 0 && hoveredSlot >= 0
            && hoveredSlot != s_drag.fromSlot) {
            // moveSlot tiene validacion interna del tag_filter — si no
            // matchea, retorna false sin mutar.
            inv.moveSlot(s_drag.fromSlot, hoveredSlot);
        }
        s_drag.fromSlot = -1;
    }

    // Ghost del item dragueado bajo el cursor.
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

} // namespace

/// @brief Widget principal del inventory_panel. Decide el modo y delega.
void drawInventoryPanel(const HudContext& ctx) {
    // Defensive: sin scene o sin assets no podemos resolver nada.
    if (ctx.scene == nullptr || ctx.assets == nullptr) return;

    Entity player = findPlayerForHud(*ctx.scene);
    if (!player) return;

    // Mutable porque Grid2D/Equipment llaman moveSlot (drag entre cells).
    auto& inv = player.getComponent<InventoryComponent>().state;
    switch (inv.mode) {
        case Inventory::LayoutMode::FlatList:
            drawInventoryFlatList(ctx, inv, *ctx.assets); break;
        case Inventory::LayoutMode::Grid2D:
            drawInventoryGrid2D(ctx, inv, *ctx.assets); break;
        case Inventory::LayoutMode::EquipmentSlots:
            drawInventoryEquipment(ctx, inv, *ctx.assets); break;
    }
}

// =============================================================
// REGISTRY DE WIDGETS — orden = orden de dibujado (back-to-front)
// =============================================================
struct HudWidget {
    const char* name;
    void (*draw)(const HudContext&);
};

const HudWidget k_widgets[] = {
    // Vignette PRIMERO para que quede detras del HUD numerico.
    { "damage_vignette",  &drawDamageVignette },
    { "health_number",    &drawHealthNumber },
    { "stamina_bar",      &drawStaminaBar },       // F2H41
    { "ammo_counter",     &drawAmmoCounter },
    { "objective_text",   &drawObjectiveText },    // F2H41
    { "kill_feed",        &drawKillFeed },         // F2H41
    { "compass_bar",      &drawCompassBar },       // F2H41
    { "crosshair",        &drawCrosshair },
    { "hit_marker",       &drawHitMarker },
    { "interact_prompt",  &drawInteractPrompt },
    { "pickup_queue",     &drawPickupNotifications },
    { "dialog_box",       &drawDialogBox },        // F2H48
    { "inventory_panel",  &drawInventoryPanel },   // F2H52 H (default OFF)
    { "crt_scanline",     &drawCrtScanline },      // F2H41 (default OFF)
};

} // namespace

// =============================================================
// API PUBLICA
// =============================================================

void draw(::ImDrawList* dl,
          float x0, float y0, float w, float h,
          float dt,
          const glm::vec3& cameraForward,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested,
          Scene* scene,
          AssetManager* assets) {
    HudState& hudRef = GameState::hud();
    HudContext ctx;
    ctx.dl = dl;
    ctx.x0 = x0; ctx.y0 = y0; ctx.w = w; ctx.h = h;
    ctx.dt = dt;
    ctx.hud = &hudRef;
    ctx.now = static_cast<float>(ImGui::GetTime());
    ctx.cameraForward = cameraForward;
    ctx.scene = scene;     // F2H52 H: inventory_panel + futuros widgets
    ctx.assets = assets;

    // Iterar widgets enabled. El default es enabled; Lua puede togglear.
    for (const HudWidget& wd : k_widgets) {
        if (!hudRef.isWidgetEnabled(wd.name)) continue;
        wd.draw(ctx);
    }

    // Pause menu encima de todo (cuando paused).
    drawPauseMenu(ctx, exitButtonLabel, onExitRequested);
}

} // namespace Mood::GameOverlay
