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

    // Letras a-z.
    if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z') {
        return {BindingType::Key, SDL_SCANCODE_A + (s[0] - 'a')};
    }

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
        case BindingType::None:
        default:
            return false;
    }
}

} // namespace Mood::InputActions
