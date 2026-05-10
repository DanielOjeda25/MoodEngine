#pragma once

// Estado global del juego accesible desde C++ y desde Lua. Hito 20:
// los scripts pueden leer/escribir el HUD (hp/ammo) y el flag de pausa
// via la tabla `hud` registrada en LuaBindings.
//
// F2H39: HudState extendido con campos para los nuevos widgets del
// HUD framework (DamageVignette, HitMarker, InteractPrompt, Pickup
// Notification) + map de toggles per-widget. Backward-compat con
// SaveLoad — campos nuevos default-init si no estan en el JSON.
//
// No es thread-safe (el motor es single-threaded). El reset() lo llama
// `EditorApplication::exitPlayMode` para volver al baseline al salir
// del Play Mode.

#include "core/Types.h"

#include <glm/vec2.hpp>

#include <deque>
#include <string>
#include <unordered_map>

namespace Mood {

/// @brief Una notificacion transient en la pickup queue.
///        ttl: countdown en segundos. El widget la decrementa con dt
///        cada frame; <= 0 = expirada (popear). Se inicializa al
///        valor de lifetime estandar (~2.5s) en `pushPickup`.
struct PickupNotification {
    std::string text;
    f32         ttl = 0.0f;
};

struct HudState {
    // --- Hito 20 (preservado) ---
    int hp   = 100;
    int ammo = 30;  // legacy, no se usa post-F2H39 (ahora mag+reserve)

    // --- F2H39: Health extendido ---
    int max_hp = 100;

    // --- F2H39: Ammo split (mag/reserve) estilo HL/CoD ---
    int mag      = 30;
    int max_mag  = 30;
    int reserve  = 90;

    // --- F2H39: Hit marker transient (cyan crosshair flash al pegar) ---
    /// Countdown timer en segundos. Mientras > 0 se dibuja el marker.
    /// Reseteado por `hud.show_hit_marker()`. Decrementado por el caller
    /// del overlay con dt.
    f32 hit_marker_t = 0.0f;

    // --- F2H39: Damage indicator (vignette + arc direccional) ---
    /// Direccion 2D (x: derecha, y: enfrente) normalizada del atacante.
    /// (0,1) = enemigo enfrente. (1,0) = derecha. (-1,0) = izquierda.
    /// (0,-1) = atras. Si magnitud=0, vignette radial sin arco.
    glm::vec2 damage_dir{0.0f};
    /// Countdown timer en segundos. Mientras > 0 se dibuja vignette.
    f32       damage_t = 0.0f;

    // --- F2H39: Interact prompt (texto centro pantalla) ---
    /// Vacio = no se dibuja. Estilo "Levantar X" / "Abrir puerta".
    std::string interact_prompt;

    // --- F2H39: Pickup queue ---
    /// Cola de mensajes transient. Cap implicito ~5; el push descarta
    /// el mas viejo si la cola esta llena (ver `GameOverlay::pushPickup`).
    std::deque<PickupNotification> pickup_queue;

    // --- F2H39: Toggles per-widget ---
    /// Map name -> bool. Ausente = enabled (default true). Lua puede
    /// togglear con `hud.set_widget("name", false)` para esconder
    /// widgets segun el contexto (ej. cinematics, menus).
    std::unordered_map<std::string, bool> widget_enabled;

    /// @brief Ayuda al overlay: testea si un widget esta enabled
    ///        (default true si el name no esta en el map).
    bool isWidgetEnabled(const std::string& name) const {
        auto it = widget_enabled.find(name);
        return it == widget_enabled.end() || it->second;
    }
};

namespace GameState {

HudState& hud();

/// @brief Flag de pausa del gameplay. True congela `updateCameras` y
///        muestra el menu modal en `drawGameOverlay`.
bool& paused();

/// @brief Resetea HUD a defaults y paused=false.
void reset();

// --- F2H39: helpers de mutacion del HUD (puros, sin deps graficas) ---
// Vivien aqui (no en GameOverlay) para que LuaBindings los pueda llamar
// sin arrastrar ImGui al modulo de tests.

/// @brief Resetea el timer del hit marker (0.3s lifetime).
void triggerHitMarker();

/// @brief Setea direccion + reset del timer del damage indicator.
///        `dir` se normaliza internamente. Magnitud 0 = vignette
///        radial sin arco. Lifetime 0.5s.
void triggerDamageFlash(float dirX, float dirY);

/// @brief Empuja un pickup notification a la queue. Lifetime 2.5s.
///        La queue tiene cap implicito ~5 — push sobre lleno descarta
///        el mas viejo (FIFO).
void pushPickup(const char* text);

/// @brief Limpia el interact prompt (azucar — equivalente a setear
///        string vacio).
void clearInteractPrompt();

} // namespace GameState

} // namespace Mood
