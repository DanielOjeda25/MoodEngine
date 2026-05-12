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

#include <glm/vec3.hpp>

#include <functional>

// Forward decl en namespace global — `ImDrawList` vive en el toplevel
// (lo define `imgui.h`). Sin esto el compilador lo busca dentro de
// `Mood::GameOverlay` y falla con "use of undefined type".
struct ImDrawList;

namespace Mood {

struct HudState;
class Scene;          // F2H52 H: inventory_panel widget necesita scene
class AssetManager;   // F2H52 H: + assets para resolver items

/// @brief Contexto pasado a cada widget. Vida del frame actual —
///        no guardar referencias entre frames.
struct HudContext {
    ::ImDrawList* dl;
    float x0, y0, w, h;     // rect del viewport en screen-space
    float dt;               // delta time del frame en segundos
    HudState* hud;          // estado del HUD (mutable: widgets pueden
                            // decrementar timers, popear queue)
    float now;              // tiempo absoluto desde boot (segundos)
    /// F2H41: forward de la camara del player. Usado por CompassBar
    /// para derivar yaw. Default `(0,0,-1)` = mirando -Z (convencion
    /// OpenGL right-handed). Caller pasa `m_playCamera.forward()`.
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
    /// F2H52 H5: posicion world-space de la camara del player. Usado por
    /// el "Tirar" del inventory_panel para spawnear el pickup enfrente
    /// del player (cameraPos + cameraForward * 1.5). Default origen.
    glm::vec3 cameraPosition{0.0f, 0.0f, 0.0f};
    /// F2H52 H: scene + assets opcionales para widgets que leen estado
    /// del mundo (ej. `inventory_panel` necesita el InventoryComponent
    /// del player + ItemAsset metadata para mostrar nombres/iconos).
    /// Default nullptr; widgets que los necesitan deben chequear antes
    /// de usar. En tests headless quedan vacios y el widget se skipea.
    Scene* scene = nullptr;
    AssetManager* assets = nullptr;
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
          const glm::vec3& cameraForward,
          const char* exitButtonLabel,
          const std::function<void()>& onExitRequested,
          Scene* scene = nullptr,
          AssetManager* assets = nullptr,
          const glm::vec3& cameraPosition = glm::vec3(0.0f));

// NOTA: los helpers de mutacion (triggerHitMarker, triggerDamageFlash,
// pushPickup, clearInteractPrompt) viven en `GameState::*` (no aqui)
// para que LuaBindings los pueda invocar sin arrastrar ImGui a las
// dependencias de tests.

} // namespace GameOverlay

} // namespace Mood
