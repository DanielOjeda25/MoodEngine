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
