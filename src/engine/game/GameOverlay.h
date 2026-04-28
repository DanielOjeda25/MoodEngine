#pragma once

// HUD del juego (HP/Ammo/crosshair) + menu de pausa, dibujado con
// ImDrawList encima del framebuffer del viewport. Hito 21 Bloque 3
// Fase B: extraido de `EditorPlayMode.cpp::drawGameOverlay` para
// que el editor (en Play Mode) y `MoodPlayer` lo compartan.
//
// Lee `GameState::hud()` y `GameState::paused()` (singleton). El
// boton "Salir" cambia segun el host: en el editor vuelve a Editor
// Mode; en el player cierra la app. Por eso recibe un callback.

#include <functional>

// Forward decl en namespace global — `ImDrawList` vive en el toplevel
// (lo define `imgui.h`). Sin esto el compilador lo busca dentro de
// `Mood::GameOverlay` y falla con "use of undefined type".
struct ImDrawList;

namespace Mood {

namespace GameOverlay {

/// @brief Pinta crosshair + bloques HP/AMMO; si `GameState::paused()`
///        esta activo, dibuja overlay oscuro + panel modal con 3
///        botones (Continuar / Opciones / Salir).
///
///   - "Continuar" desactiva `GameState::paused()`. El sync de cursor
///     SDL lo hace el caller en su update de camara detectando la
///     transicion del flag.
///   - "Opciones" loguea TBD (placeholder).
///   - "Salir" invoca `onExitRequested()`. Su label es configurable
///     ("Salir al editor" en el editor, "Salir del juego" en el player).
///
/// `dl` es el drawlist del frame actual; `(x0,y0,w,h)` el rect de la
/// imagen del viewport en screen-space.
void draw(::ImDrawList* dl,
          float x0, float y0, float w, float h,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested);

} // namespace GameOverlay

} // namespace Mood
