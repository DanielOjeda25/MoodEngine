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

struct ProjectSettings {
    // === General ===

    /// FPS objetivo del runtime. F3H1 solo almacena el valor; el cap
    /// real (frame pacing) se cabllea cuando se migre el `Window`
    /// hardcoded a leer de aca (hito siguiente).
    int targetFps = 60;

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
