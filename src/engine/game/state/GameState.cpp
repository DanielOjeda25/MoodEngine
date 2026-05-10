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

void reset() {
    hud()    = HudState{};
    paused() = false;
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

} // namespace Mood::GameState
