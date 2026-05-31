#pragma once

// Estado global del juego accesible desde C++ y desde Lua. Hito 20:
// los scripts pueden leer/escribir el HUD (hp/mag/reserve) y el flag de
// pausa via la tabla `hud` registrada en LuaBindings.
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

/// @brief Una entry en la kill feed (Doom/CoD style). Mismo patron
///        ttl que PickupNotification — el widget decrementa cada
///        frame y popea cuando ttl<=0.
struct KillEntry {
    std::string text;
    /// RGBA packed (IM_COL32). Default blanco; pushKillColored permite
    /// custom para diferenciar enemy types o team colors.
    unsigned int color = 0xFFF8F8F8;
    f32         ttl = 0.0f;
};

struct HudState {
    // --- Hito 20 (preservado) ---
    int hp = 100;

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

    // --- F2H70.3 H: velocimetro del vehiculo montado ---
    /// Velocidad escalar en km/h. La setea EditorPlayMode mientras el player
    /// conduce; el widget `speedometer` (default OFF, on al montar) la dibuja.
    f32 vehicle_speed_kmh = 0.0f;
    /// Posicion del popup del velocimetro en pantalla, normalizada [0,1]
    /// (x desde izquierda, y desde arriba). EditorPlayMode proyecta la pose
    /// world del auto a pantalla cada frame para que el popup lo siga.
    f32  vehicle_marker_x = 0.5f;
    f32  vehicle_marker_y = 0.5f;
    /// true si el auto esta dentro del frustum (sino el widget no dibuja).
    bool vehicle_marker_onscreen = false;

    // --- F4H4: Armadura del player. drawArmorNumber widget.
    int armor     = 0;
    int max_armor = 100;

    // --- F4H6: Camera shake state (intensidad procedural con decay) ---
    /// @brief Amplitud del shake en metros. Apply: offset del camera position
    ///        = `sin(time*freqX)*ampX + sin(time*freqY)*ampY` escalado por
    ///        `shake_t / shake_max_t`. 0 = sin shake.
    f32 shake_amp     = 0.0f;
    /// @brief Countdown timer del shake (segundos). Mientras > 0 el offset
    ///        se aplica al m_playCamera. Decrementado en el bridge.
    f32 shake_t       = 0.0f;
    /// @brief Duracion total del shake al triggerearse — para normalizar el
    ///        decay (`shake_t / shake_max_t`). Setear junto con shake_amp.
    f32 shake_max_t   = 0.0f;

    // --- F4H6: Pain reaction (pitch wobble + roll random al recibir damage)
    /// @brief Amplitud del pitch en grados. Apply: agregar `amp * (t/max_t)`
    ///        al pitch de la camara del player ANTES de la view matrix.
    f32 pain_pitch_amp = 0.0f;
    /// @brief Roll random en grados (se setea en el trigger via rand). Apply
    ///        igual que pitch — agregar al roll de la camara.
    f32 pain_roll_offset = 0.0f;
    /// @brief Timer del pain pitch. Compartido entre pitch + roll.
    f32 pain_pitch_t   = 0.0f;
    f32 pain_pitch_max_t = 0.0f;

    // --- F4H6: Crosshair spread visual (gap dinamico).
    /// @brief Spread en grados del arma activa. El widget drawCrosshair
    ///        computa `gap = 8 + min(16, spread * 1.5)`. Sync desde el bridge
    ///        cada frame con `spec.spreadDeg` del slot activo.
    f32 crosshair_spread_deg = 0.0f;

    // --- F4H4: Arsenal overlay transient al hacer swap.
    /// Countdown 3s. Mientras > 0, se dibuja el indicador del arsenal
    /// con los 4 slots arriba-centro (activo highlighted, fade alpha).
    f32 arsenal_overlay_t = 0.0f;
    /// 4 names de las armas en cada slot (display name del WeaponSpec o
    /// "" si vacio). Synced cada frame en Play desde el WeaponComponent
    /// del player.
    std::string arsenal_slots[4];
    /// Slot activo 0..3 (highlighted).
    u32 arsenal_active_slot = 0;

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

    // --- F2H41: StaminaBar (Skyrim/Metro) ---
    /// Stamina actual y maxima. Color shift a amarillo cuando
    /// stamina < 30%. Drena al correr / atacar (gameplay decide).
    int stamina     = 100;
    int max_stamina = 100;

    // --- F2H41: ObjectiveText (HL2/CoD) ---
    /// Texto del objetivo actual. Vacio = no se dibuja. El widget
    /// agrega el prefijo "OBJETIVO: " automaticamente.
    std::string objective_text;

    // --- F2H41: KillFeed (Doom/CoD) ---
    /// Cola de kills transient. Cap implicito ~5. Lifetime 4s con
    /// fade in/out. Mismo patron que pickup_queue.
    std::deque<KillEntry> kill_feed;

    // --- F2H39: Toggles per-widget ---
    /// Map name -> bool. Ausente = enabled (default true). Lua puede
    /// togglear con `hud.set_widget("name", false)` para esconder
    /// widgets segun el contexto (ej. cinematics, menus).
    /// F2H41: defaults explicitos a false para widgets que deben
    /// estar OFF por default (ej. crt_scanline — efecto opcional
    /// que pocos juegos quieren prendido siempre).
    std::unordered_map<std::string, bool> widget_enabled{
        {"crt_scanline",    false},
        {"speedometer",     false},  // F2H70.3 H: default OFF; on al montar
        {"inventory_panel", false},  // F2H52 H: default OFF; Tab lo togglea
        {"quest_log_panel", false},  // F2H53 G: default OFF; J lo togglea
    };

    // --- F2H52 Bloque I: container split del inventory_panel ---
    /// @brief Si true, el widget `inventory_panel` renderea en split mode
    ///        (player izquierda, container derecha). Drag entre lados
    ///        transfiere 1 unidad. False = single mode default.
    ///        Usamos un bool separado porque entt::entity 0 es un handle
    ///        VALIDO (la primera entity creada) — no podemos usar 0 como
    ///        sentinel de "no container".
    bool container_open = false;
    /// @brief Handle del entity-container actualmente abierto (cofre, mesa
    ///        de comercio, drop pile, etc.). Solo valido si
    ///        `container_open == true`. Es el u32 underlying de
    ///        `entt::entity` — el caller (Lua binding `hud.open_container`)
    ///        hace el cast.
    u32 container_target = 0;

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

/// @brief F2H48: flag global "hay un dialog activo". El char controller
///        (Editor + Player) lo chequea para desactivar input mientras
///        el jugador esta hablando con un NPC. Se setea on/off desde
///        `DialogSystem::start/stop`; el caller NO lo debe tocar
///        manualmente.
bool& dialogActive();

/// @brief F2H48: variables persistentes del dialog. `set_var/get_var`
///        de Lua escriben aca. Sobreviven entre dialogs (decision F2H48
///        #2) — habilita "el NPC recuerda que ya hablaste". Persistencia
///        en save: TODO en F2H48 si emerge necesidad inmediata, por
///        ahora vive solo en memoria.
std::unordered_map<std::string, std::string>& dialogVars();

/// @brief Resetea HUD a defaults y paused=false. F2H48: tambien limpia
///        dialogActive=false + dialogVars (al salir de Play Mode no
///        deberia quedar dialog state colgando).
void reset();

/// @brief F2H52 M-fix: true si el input del jugador (camera, movimiento)
///        deberia estar bloqueado. Pasa cuando:
///        - `paused()` (Esc / menu de pausa).
///        - El `inventory_panel` esta abierto (Tab) — necesitamos el
///          cursor libre para que el dev pueda hover items, right-click,
///          drag entre slots, etc.
///        Los callers (EditorApplication::updateCameras + PlayerApplication
///        ::updateCamera) usan este flag para liberar SDL_SetRelativeMouseMode
///        y skipear la actualizacion de camera.
bool isInputBlocked();

// --- F2H39: helpers de mutacion del HUD (puros, sin deps graficas) ---
// Vivien aqui (no en GameOverlay) para que LuaBindings los pueda llamar
// sin arrastrar ImGui al modulo de tests.

/// @brief Resetea el timer del hit marker (0.3s lifetime).
void triggerHitMarker();

/// @brief Setea direccion + reset del timer del damage indicator.
///        `dir` se normaliza internamente. Magnitud 0 = vignette
///        radial sin arco. Lifetime 0.5s.
void triggerDamageFlash(float dirX, float dirY);

/// @brief F4H4 — Resetea el timer del arsenal overlay a 3s. Llamado por
///        `Weapon::applySwap` cuando el shooter es el player. El widget
///        `arsenal_overlay` dibuja la fila de slots arriba-centro mientras
///        el timer > 0, con alpha fade.
void triggerArsenalOverlay();

/// @brief F4H6 — Camera shake procedural con decay lineal.
///        `amplitude` en metros (HL/COD sutil: 0.02 disparo / 0.08 damage /
///        0.15 explosion). `duration` en segundos (~0.1-0.4s tipico).
///        Anti-spam: si ya hay un shake activo con `shake_t > duration`,
///        no extiende (preserva el shake mas largo). Si el nuevo es mas
///        fuerte, reemplaza.
void triggerCameraShake(float amplitude, float duration);

/// @brief F4H6 — Pain reaction al recibir damage: pitch wobble +2deg +
///        roll random ±1deg sobre 0.25s, return a 0 en 0.3s. Anti-spam:
///        no re-trigger si timer > 0.1s (evita oscilacion en damage
///        continuo). NO mueve yaw (mantiene apuntar).
void triggerPainReaction();

/// @brief Empuja un pickup notification a la queue. Lifetime 2.5s.
///        La queue tiene cap implicito ~5 — push sobre lleno descarta
///        el mas viejo (FIFO).
void pushPickup(const char* text);

/// @brief Limpia el interact prompt (azucar — equivalente a setear
///        string vacio).
void clearInteractPrompt();

// --- F2H41: helpers KillFeed ---

/// @brief Empuja un kill entry a la kill feed con color default.
///        Lifetime 4s. Cap visual 5 — push sobre lleno descarta el
///        mas viejo (FIFO).
void pushKill(const char* text);

/// @brief Variante con color RGBA packed (IM_COL32) para diferenciar
///        enemy types / team colors.
void pushKillColored(const char* text, unsigned int color);

/// @brief Limpia el objective text (azucar).
void clearObjective();

} // namespace GameState

} // namespace Mood
