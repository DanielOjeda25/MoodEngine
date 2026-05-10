#pragma once

// HUD del juego (HP/Ammo/crosshair/damage/hit marker/interact/pickup
// /pause menu) dibujado con ImDrawList encima del framebuffer del
// viewport. Hito 21 Bloque 3 Fase B: extraido de
// `EditorPlayMode.cpp::drawGameOverlay` para que el editor (en Play
// Mode) y `MoodPlayer` lo compartan.
//
// F2H39 — refactor a widget framework extensible:
//   - `HudContext` struct con todo lo que cada widget necesita
//     (drawlist, rect, dt, hud state).
//   - `HudWidget` struct: name + draw function + enabled flag (consultado
//     desde HudState::widget_enabled). Lista hardcoded de widgets
//     default; futuros (Lua-driven, diegetic 3D) se agregan extendiendo
//     la lista o llamando registerWidget desde fuera.
//   - `draw()` itera widgets enabled y los pinta en orden.
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

struct HudState;

/// @brief Contexto pasado a cada widget. Vida del frame actual —
///        no guardar referencias entre frames.
struct HudContext {
    ::ImDrawList* dl;
    float x0, y0, w, h;     // rect del viewport en screen-space
    float dt;               // delta time del frame en segundos
    HudState* hud;          // estado del HUD (mutable: widgets pueden
                            // decrementar timers, popear queue)
    float now;              // tiempo absoluto desde boot (segundos)
};

namespace GameOverlay {

/// @brief Renderea todos los widgets enabled del HUD + el menu de
///        pausa si `GameState::paused()` esta activo.
///
///   - "Continuar" desactiva `GameState::paused()`. El sync de cursor
///     SDL lo hace el caller en su update de camara detectando la
///     transicion del flag.
///   - "Opciones" loguea TBD (placeholder).
///   - "Salir" invoca `onExitRequested()`. Su label es configurable
///     ("Salir al editor" en el editor, "Salir del juego" en el player).
///
/// `dl` es el drawlist del frame actual; `(x0,y0,w,h)` el rect de la
/// imagen del viewport en screen-space. `dt` es el delta del frame
/// (segundos) — usado para decay de timers (hit_marker_t, damage_t)
/// y fade de pickup notifications.
void draw(::ImDrawList* dl,
          float x0, float y0, float w, float h,
          float dt,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested);

// NOTA: los helpers de mutacion (triggerHitMarker, triggerDamageFlash,
// pushPickup, clearInteractPrompt) viven en `GameState::*` (no aqui)
// para que LuaBindings los pueda invocar sin arrastrar ImGui a las
// dependencias de tests.

} // namespace GameOverlay

} // namespace Mood
