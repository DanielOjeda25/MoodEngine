#include "engine/game/state/GameState.h"

#include <cstring>

namespace Mood::GameState {

HudState& hud() {
    static HudState s_hud{};
    return s_hud;
}

bool& paused() {
    static bool s_paused = false;
    return s_paused;
}

bool& dialogActive() {
    static bool s_active = false;
    return s_active;
}

std::unordered_map<std::string, std::string>& dialogVars() {
    static std::unordered_map<std::string, std::string> s_vars;
    return s_vars;
}

void reset() {
    hud()           = HudState{};
    paused()        = false;
    dialogActive()  = false;
    dialogVars().clear();
}

bool isInputBlocked() {
    if (paused()) return true;
    // F2H52 M-fix: si el inventory_panel esta abierto necesitamos
    // cursor libre para hover/click/drag — bloqueamos el input de
    // camara/movimiento como si fuese pausa.
    if (hud().isWidgetEnabled("inventory_panel")) return true;
    return false;
}

// --- F2H39: helpers puros ---

void triggerHitMarker() {
    hud().hit_marker_t = 0.3f;
}

void triggerDamageFlash(float dirX, float dirY) {
    auto& h = hud();
    h.damage_dir.x = dirX;
    h.damage_dir.y = dirY;
    h.damage_t = 0.5f;
}

void triggerArsenalOverlay() {
    hud().arsenal_overlay_t = 3.0f;
}

void triggerCameraShake(float amplitude, float duration) {
    auto& h = hud();
    // Anti-spam: si hay un shake activo mas fuerte, no lo pisamos. Si el
    // nuevo es mas fuerte o el actual ya casi expira, reemplaza.
    const bool currentlyActive = h.shake_t > 0.0f && h.shake_max_t > 0.0f;
    const bool newIsStronger   = amplitude > h.shake_amp;
    if (currentlyActive && !newIsStronger
        && h.shake_t > 0.05f) {
        return;
    }
    h.shake_amp     = amplitude;
    h.shake_t       = duration;
    h.shake_max_t   = duration;
}

void triggerPainReaction() {
    auto& h = hud();
    // Anti-spam: si el pitch ya activo y > 0.1s restantes, skip — evita
    // oscilacion en damage continuo.
    if (h.pain_pitch_t > 0.1f) return;
    h.pain_pitch_amp     = 2.0f;  // deg
    h.pain_pitch_t       = 0.25f;
    h.pain_pitch_max_t   = 0.25f;
    // Roll random ±1deg. Determinismo: no usamos rand() global por test
    // reproducibility; usamos un xorshift sembrado por el frame count
    // implicito al sumar el current shake_t (que cambia frame a frame).
    static u32 painRoll = 0xA02BDBF7u;
    painRoll ^= painRoll << 13;
    painRoll ^= painRoll >> 17;
    painRoll ^= painRoll << 5;
    const f32 normalized = static_cast<f32>(painRoll) / 4294967296.0f;
    h.pain_roll_offset   = (normalized * 2.0f - 1.0f);  // [-1, 1] deg
}

void pushPickup(const char* text) {
    if (text == nullptr || std::strlen(text) == 0) return;
    auto& q = hud().pickup_queue;
    if (q.size() >= 5) q.pop_front();
    PickupNotification n;
    n.text = text;
    n.ttl  = 2.5f;
    q.push_back(std::move(n));
}

void clearInteractPrompt() {
    hud().interact_prompt.clear();
}

void pushKill(const char* text) {
    pushKillColored(text, 0xFFF8F8F8u); // default blanco
}

void pushKillColored(const char* text, unsigned int color) {
    if (text == nullptr || std::strlen(text) == 0) return;
    auto& q = hud().kill_feed;
    if (q.size() >= 5) q.pop_front();
    KillEntry e;
    e.text  = text;
    e.color = color;
    e.ttl   = 4.0f;
    q.push_back(std::move(e));
}

void clearObjective() {
    hud().objective_text.clear();
}

} // namespace Mood::GameState
