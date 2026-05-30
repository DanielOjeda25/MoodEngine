// F4H2 Bloque B: registra la tabla `Input` en Lua. API:
//
//   Input.is_action_pressed("fire") -> bool
//
// Lookup data-driven contra `UserSettings::input().keybindings`. El dev
// puede rebindear en settings.json sin tocar el script.
//
// Por ahora es la única función; futuros: `Input.is_action_just_pressed`
// (edge detection), `Input.axis(name)` para sticks de gamepad, etc.

#include "engine/scripting/bindings/LuaBindings.h"

#include "engine/input/InputActions.h"

#include <sol/sol.hpp>

namespace Mood {

void setupInputBindings(sol::state& lua) {
    sol::table inputTable = lua.create_named_table("Input");
    inputTable.set_function("is_action_pressed",
        [](const std::string& action) {
            return InputActions::isActionPressed(action);
        });
}

} // namespace Mood
