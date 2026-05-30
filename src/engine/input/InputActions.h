#pragma once

// F4H2 Bloque B: bridge data-driven entre keybindings de UserSettings y
// el estado de SDL. Provee 2 APIs:
//
//   InputActions::resolveBinding("mouse_left") -> {Type::MouseButton, ...}
//   InputActions::isActionPressed("fire")      -> bool
//
// El primero es puro testeable (no toca SDL). El segundo consulta el
// estado actual del teclado/mouse via SDL_GetKeyboardState/MouseState —
// para usar en runtime cuando ya hay un SDL context activo.
//
// Bindings soportados:
//   - "mouse_left" / "mouse_right" / "mouse_middle"
//   - Letras: "a"-"z"
//   - Teclas especiales: "space", "enter", "escape", "tab",
//                          "lshift", "lctrl", "lalt", "rshift", "rctrl", "ralt"
//   - Cualquier otro string → Type::None (no presiona nada).
//
// Lookup case-insensitive: "R" / "r" / " R " resuelven al mismo binding.

#include <string>

namespace Mood::InputActions {

/// @brief Tipo del binding resuelto.
enum class BindingType {
    None        = 0,  ///< String no reconocido.
    Key         = 1,  ///< SDL_Scancode en `code`.
    MouseButton = 2,  ///< SDL mouse button index en `code` (1=left, 3=right, 2=middle).
};

/// @brief Binding parseado. `code` es interpretable según `type`.
struct Binding {
    BindingType type = BindingType::None;
    int         code = 0;
};

/// @brief Parsea un string de keybinding (ej. "mouse_left", "r", "space")
///        a un `Binding` listo para consultar SDL. Case-insensitive +
///        ignora whitespace al inicio/fin. Strings desconocidos → None.
Binding resolveBinding(const std::string& binding);

/// @brief True si la acción está presionada AHORA (lee SDL state). Si
///        la acción no está mapeada en UserSettings o el binding es
///        inválido, retorna false silenciosamente. Llamar dentro del
///        frame loop (después de `SDL_PumpEvents` o equivalente).
bool isActionPressed(const std::string& action);

} // namespace Mood::InputActions
