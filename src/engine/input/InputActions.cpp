#include "engine/input/InputActions.h"

#include "core/UserSettings.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Mood::InputActions {

namespace {

// Trim + lowercase.
std::string normalize(const std::string& s) {
    auto begin = s.begin();
    auto end   = s.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) ++begin;
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) --end;
    std::string out(begin, end);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Tabla de teclas especiales. Letras a-z se manejan por delta de char.
const std::unordered_map<std::string, int>& keyTable() {
    static const std::unordered_map<std::string, int> kTable = {
        {"space",   SDL_SCANCODE_SPACE},
        {"enter",   SDL_SCANCODE_RETURN},
        {"return",  SDL_SCANCODE_RETURN},
        {"escape",  SDL_SCANCODE_ESCAPE},
        {"esc",     SDL_SCANCODE_ESCAPE},
        {"tab",     SDL_SCANCODE_TAB},
        {"lshift",  SDL_SCANCODE_LSHIFT},
        {"rshift",  SDL_SCANCODE_RSHIFT},
        {"lctrl",   SDL_SCANCODE_LCTRL},
        {"rctrl",   SDL_SCANCODE_RCTRL},
        {"lalt",    SDL_SCANCODE_LALT},
        {"ralt",    SDL_SCANCODE_RALT},
    };
    return kTable;
}

} // namespace

Binding resolveBinding(const std::string& binding) {
    const std::string s = normalize(binding);
    if (s.empty()) return {};

    // Mouse buttons.
    if (s == "mouse_left")   return {BindingType::MouseButton, SDL_BUTTON_LEFT};
    if (s == "mouse_right")  return {BindingType::MouseButton, SDL_BUTTON_RIGHT};
    if (s == "mouse_middle") return {BindingType::MouseButton, SDL_BUTTON_MIDDLE};

    // F4H3: mouse wheel. code = +1 (up) / -1 (down).
    if (s == "mouse_wheel_up")   return {BindingType::MouseWheel, +1};
    if (s == "mouse_wheel_down") return {BindingType::MouseWheel, -1};

    // Letras a-z.
    if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z') {
        return {BindingType::Key, SDL_SCANCODE_A + (s[0] - 'a')};
    }

    // F4H3: digitos 1-9 (para weapon_1..weapon_9 numericos opcionales).
    if (s.size() == 1 && s[0] >= '1' && s[0] <= '9') {
        return {BindingType::Key, SDL_SCANCODE_1 + (s[0] - '1')};
    }
    if (s == "0") return {BindingType::Key, SDL_SCANCODE_0};

    // Teclas especiales por nombre.
    const auto& tbl = keyTable();
    auto it = tbl.find(s);
    if (it != tbl.end()) return {BindingType::Key, it->second};

    return {};  // Type::None
}

bool isActionPressed(const std::string& action) {
    const auto& kb = UserSettings::input().keybindings;
    auto it = kb.find(action);
    if (it == kb.end()) return false;
    const Binding b = resolveBinding(it->second);
    switch (b.type) {
        case BindingType::Key: {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            if (keys == nullptr) return false;
            return keys[b.code] != 0;
        }
        case BindingType::MouseButton: {
            const Uint32 state = SDL_GetMouseState(nullptr, nullptr);
            return (state & SDL_BUTTON(b.code)) != 0;
        }
        case BindingType::MouseWheel:
            // Scroll no se "sostiene" — siempre false en isActionPressed.
            // Para detectar swap usar wasActionTriggered.
            return false;
        case BindingType::None:
        default:
            return false;
    }
}

// F4H3 — Estado interno para wasActionTriggered + scroll.
namespace {

// Acumulador del delta de scroll del frame actual. Se consume con
// pollScrollDelta() y se resetea con endFrame().
int g_scrollDeltaThisFrame = 0;

// Mapa action -> estado "presionada en el frame previo". Permite
// detectar transicion released->pressed en wasActionTriggered.
std::unordered_map<std::string, bool>& prevPressed() {
    static std::unordered_map<std::string, bool> m;
    return m;
}

// Detecta si el binding esta "presionado" AHORA (sin consumir scroll).
// Para keys/mouse buttons: igual que isActionPressed. Para scroll:
// el delta del frame matchea el signo del binding.
bool isBindingPressedRaw(const Binding& b) {
    switch (b.type) {
        case BindingType::Key: {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            if (keys == nullptr) return false;
            return keys[b.code] != 0;
        }
        case BindingType::MouseButton: {
            const Uint32 state = SDL_GetMouseState(nullptr, nullptr);
            return (state & SDL_BUTTON(b.code)) != 0;
        }
        case BindingType::MouseWheel:
            // True si el delta del frame coincide con el signo del binding.
            return (b.code > 0 && g_scrollDeltaThisFrame > 0)
                || (b.code < 0 && g_scrollDeltaThisFrame < 0);
        case BindingType::None:
        default:
            return false;
    }
}

} // namespace

bool wasActionTriggered(const std::string& action) {
    const auto& kb = UserSettings::input().keybindings;
    auto it = kb.find(action);
    if (it == kb.end()) return false;
    const Binding b = resolveBinding(it->second);
    if (b.type == BindingType::None) return false;

    const bool nowPressed = isBindingPressedRaw(b);
    // Para scroll wheel, "triggered" = hay delta del signo correcto este
    // frame. Es one-shot por naturaleza (el evento es discreto). No
    // necesita comparar con prevPressed.
    if (b.type == BindingType::MouseWheel) {
        return nowPressed;
    }
    // Keys/mouse buttons: transicion released -> pressed.
    auto& prev = prevPressed();
    auto pIt = prev.find(action);
    const bool wasPressedBefore = (pIt != prev.end()) ? pIt->second : false;
    return nowPressed && !wasPressedBefore;
}

void notifyScrollEvent(int delta) {
    g_scrollDeltaThisFrame += delta;
}

int pollScrollDelta() {
    const int out = g_scrollDeltaThisFrame;
    g_scrollDeltaThisFrame = 0;
    return out;
}

void endFrame() {
    // Capturar el estado de cada action mapeada como "presionada este
    // frame" para que el siguiente frame detecte transiciones.
    const auto& kb = UserSettings::input().keybindings;
    auto& prev = prevPressed();
    for (const auto& [action, _] : kb) {
        const Binding b = resolveBinding(kb.at(action));
        // No trackear scroll en prevPressed (es one-shot por evento).
        if (b.type == BindingType::MouseWheel || b.type == BindingType::None) {
            prev[action] = false;
            continue;
        }
        prev[action] = isBindingPressedRaw(b);
    }
    g_scrollDeltaThisFrame = 0;
}

void resetState() {
    prevPressed().clear();
    g_scrollDeltaThisFrame = 0;
}

} // namespace Mood::InputActions
