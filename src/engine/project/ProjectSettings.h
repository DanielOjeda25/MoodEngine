#pragma once

// Configuracion per-proyecto editable desde el panel "Project Settings"
// (F3H1+). Vive en .moodproj bajo la key "settings".
//
// Regla "nada hardcodeado" de Fase 3: cualquier valor de comportamiento
// que el dev deberia poder tunear sin recompilar entra aca. F3H1 es el
// chasis (1 field prueba: targetFps); hitos siguientes migran hardcodes
// existentes (spawn defaults, lighting defaults, etc.).
//
// Back-compat: campos opcionales (solo se persisten si != default).
// Schema sin bump: agregar fields nuevos no rompe .moodproj viejos
// (mismo patron que `coyoteWindowSec`/`jumpBufferWindowSec` del Hito 40
// G y el cleanup de `HudState.ammo` post-v2.0.2). Forward-compat: keys
// que el codigo no conoce se ignoran silenciosamente.

#include "core/Types.h"

#include <nlohmann/json_fwd.hpp>

namespace Mood {

/// F3H4: configuracion de gameplay per-proyecto. Defaults coinciden con
/// el tuning de F2H41 (walk 5.5 m/s estilo HL2/CoD/Doom). Tras F3H3 fix,
/// Player y Editor leen ambos de aqui (paridad garantizada por un solo
/// source of truth).
struct GameplaySettings {
    /// Velocidad de caminata del jugador en m/s. Defaults 5.5 (F2H41
    /// fix: convencion FPS HL2~5.5, CoD~6, Doom Eternal~7).
    f32 walkSpeed = 5.5f;

    /// Velocidad de caminata agachado en m/s. Defaults 3.0 (proporcion
    /// ~55% del walk, mismo ratio que HL2/CoD).
    f32 crouchSpeed = 3.0f;

    /// Velocidad vertical instantanea del salto en m/s. Defaults 5.5
    /// (~1.5 m de altura con g=9.81).
    f32 jumpVelocity = 5.5f;

    /// Cooldown entre saltos en segundos. Defaults 0.2 — evita doble-
    /// salto por mantener la tecla apretada.
    f32 jumpCooldownSec = 0.2f;
};

struct ProjectSettings {
    // === General ===

    /// FPS objetivo del runtime. F3H1 solo almacena el valor; el cap
    /// real (frame pacing) se cabllea cuando se migre el `Window`
    /// hardcoded a leer de aca (hito siguiente).
    int targetFps = 60;

    /// F3H4: gameplay tier 1 (walk/crouch/jump).
    GameplaySettings gameplay;

    // Sub-secciones futuras (SpawnDefaults / Rendering / Physics) se
    // agregan como structs anidadas cuando entren hitos que las llenen.
    // No agregar placeholders vacios — solo lo que se usa hoy.
};

/// @brief Serializa los settings a JSON. Solo escribe fields que
///        difieren del default (mantiene .moodproj limpios para
///        proyectos basicos). Devuelve un object JSON vacio si todos
///        los fields son default — el caller puede chequear `.empty()`
///        para decidir si serializar la subkey en el .moodproj.
nlohmann::json toJson(const ProjectSettings& s);

/// @brief Carga settings desde JSON. Fields ausentes → default. JSON
///        con keys que el codigo no conoce → ignoradas en silencio
///        (forward-compat). Si `j` no es object → devuelve defaults.
ProjectSettings projectSettingsFromJson(const nlohmann::json& j);

} // namespace Mood
