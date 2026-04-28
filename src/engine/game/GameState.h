#pragma once

// Estado global del juego accesible desde C++ y desde Lua. Hito 20:
// los scripts pueden leer/escribir el HUD (hp/ammo) y el flag de pausa
// via la tabla `hud` registrada en LuaBindings.
//
// No es thread-safe (el motor es single-threaded). El reset() lo llama
// `EditorApplication::exitPlayMode` para volver al baseline al salir
// del Play Mode.

namespace Mood {

struct HudState {
    int hp   = 100;
    int ammo = 30;
};

namespace GameState {

HudState& hud();

/// @brief Flag de pausa del gameplay. True congela `updateCameras` y
///        muestra el menu modal en `drawGameOverlay`.
bool& paused();

/// @brief Resetea HUD a defaults (hp=100, ammo=30) y paused=false.
void reset();

} // namespace GameState

} // namespace Mood
