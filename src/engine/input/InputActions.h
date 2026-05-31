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
    MouseWheel  = 3,  ///< F4H3 — Scroll wheel. `code` = +1 (up) o -1 (down).
};

/// @brief Binding parseado. `code` es interpretable según `type`.
struct Binding {
    BindingType type = BindingType::None;
    int         code = 0;
};

/// @brief Parsea un string de keybinding (ej. "mouse_left", "r", "space",
///        "mouse_wheel_up", "mouse_wheel_down") a un `Binding` listo para
///        consultar SDL. Case-insensitive + ignora whitespace al inicio/fin.
///        Strings desconocidos → None.
Binding resolveBinding(const std::string& binding);

/// @brief True si la acción está presionada AHORA (lee SDL state). Si
///        la acción no está mapeada en UserSettings o el binding es
///        inválido, retorna false silenciosamente. Llamar dentro del
///        frame loop (después de `SDL_PumpEvents` o equivalente).
///
///        Scroll wheel siempre devuelve false en `isActionPressed` (no
///        se "sostiene" un scroll). Para detectar swap usar
///        `wasActionTriggered`.
bool isActionPressed(const std::string& action);

/// @brief F4H3 — True SOLO en el frame en que la acción transiciona de
///        "no presionada" a "presionada" (one-shot). Pensado para swap
///        de armas / interact / use, donde sostener el boton no debe
///        re-disparar la accion cada frame.
///
///        Para scroll wheel: true si hubo un evento de scroll con la
///        direccion del binding (`+1` up / `-1` down) durante este
///        frame. Requiere que el frame loop llame `notifyScrollEvent()`
///        al recibir SDL_MOUSEWHEEL events ANTES del tick que consulta
///        `wasActionTriggered`.
///
///        Requiere llamar `endFrame()` al final del tick para
///        actualizar el estado previo + resetear el delta del scroll.
bool wasActionTriggered(const std::string& action);

/// @brief F4H3 — Notifica al sistema un evento de scroll wheel (la app
///        recibe esto de SDL_MOUSEWHEEL events). El delta es acumulativo
///        dentro del frame (varios eventos suman).
/// @param delta Filas de scroll (+positivo = up, -negativo = down).
void notifyScrollEvent(int delta);

/// @brief F4H3 — Devuelve y consume el delta acumulado del scroll wheel
///        en este frame. Tipicamente llamado por el bridge antes de
///        despachar swap. Tambien se usa para detectar `mouse_wheel_*`
///        triggers — `wasActionTriggered` lo consume internamente.
int pollScrollDelta();

/// @brief F4H3 — Llamar al FINAL del tick para:
///        - Capturar el estado actual de keys/mouse a `prevPressed`
///          (para que `wasActionTriggered` detecte transiciones en el
///          siguiente frame).
///        - Resetear el delta del scroll a 0.
void endFrame();

/// @brief F4H3 — Limpia el estado interno (prev pressed + scroll delta).
///        Util en tests para garantizar aislamiento.
void resetState();

} // namespace Mood::InputActions
