#include "engine/game/state/GameState.h"

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

} // namespace Mood::GameState
