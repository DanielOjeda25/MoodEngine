#include "engine/game/overlay/GameOverlay.h"

#include "core/Log.h"
#include "engine/game/state/GameState.h"

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

    // Label "HEALTH" chiquito arriba (estilo HL).
    ctx.dl->AddText(ImVec2(originX + 12.0f, originY + 8.0f),
                     palette::k_white_dim, "HEALTH");

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

    // Label "AMMO" arriba derecha.
    const ImVec2 lblSz = ImGui::CalcTextSize("AMMO");
    ctx.dl->AddText(ImVec2(originX + bw - 12.0f - lblSz.x, originY + 8.0f),
                     palette::k_white_dim, "AMMO");

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

    // "RESERVE: N" abajo, mas chico.
    char resBuf[32];
    std::snprintf(resBuf, sizeof(resBuf), "RESERVE: %d", h.reserve);
    const ImVec2 resSz = ImGui::CalcTextSize(resBuf);
    ctx.dl->AddText(ImVec2(originX + bw - 12.0f - resSz.x, originY + bh - 18.0f),
                     palette::k_white_dim, resBuf);

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

    // Titulo "PAUSED" en grande arriba (font scale 4x).
    const float cx = ctx.x0 + ctx.w * 0.5f;
    const float titleY = ctx.y0 + ctx.h * 0.18f;
    const ImVec2 titleSz = calcTextSizeScaled("PAUSED", 4.0f);
    drawTextScaled(ctx.dl, ImVec2(cx - titleSz.x * 0.5f, titleY),
                   "PAUSED", palette::k_orange, 4.0f);

    // Subtitulo chico.
    drawTextCentered(ctx.dl, cx, titleY + titleSz.y + 8.0f,
                      "MoodEngine", palette::k_white_dim);

    // 3 botones con border geometrico.
    struct Btn { const char* label; int action; };
    const Btn btns[3] = {
        {"CONTINUAR", 0},
        {"OPCIONES",  1},
        {exitLabel,   2},
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
                case 1: Log::engine()->info("Pause: 'Opciones' aun no implementado."); break;
                case 2: if (onExitRequested) onExitRequested(); break;
                default: break;
            }
        }
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
    { "ammo_counter",     &drawAmmoCounter },
    { "crosshair",        &drawCrosshair },
    { "hit_marker",       &drawHitMarker },
    { "interact_prompt",  &drawInteractPrompt },
    { "pickup_queue",     &drawPickupNotifications },
};

} // namespace

// =============================================================
// API PUBLICA
// =============================================================

void draw(::ImDrawList* dl,
          float x0, float y0, float w, float h,
          float dt,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested) {
    HudState& hudRef = GameState::hud();
    HudContext ctx;
    ctx.dl = dl;
    ctx.x0 = x0; ctx.y0 = y0; ctx.w = w; ctx.h = h;
    ctx.dt = dt;
    ctx.hud = &hudRef;
    ctx.now = static_cast<float>(ImGui::GetTime());

    // Iterar widgets enabled. El default es enabled; Lua puede togglear.
    for (const HudWidget& wd : k_widgets) {
        if (!hudRef.isWidgetEnabled(wd.name)) continue;
        wd.draw(ctx);
    }

    // Pause menu encima de todo (cuando paused).
    drawPauseMenu(ctx, exitButtonLabel, onExitRequested);
}

} // namespace Mood::GameOverlay
