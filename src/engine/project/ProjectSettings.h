#pragma once

// Configuracion per-proyecto editable desde el panel "Project Settings"
// (F3H1+). Vive en .moodproj bajo la key "settings".
//
// Regla "nada hardcodeado" de Fase 3: cualquier valor de comportamiento
// que el dev deberia poder tunear sin recompilar entra aca. F3H1 es el
// chasis (2 fields prueba: targetFps + description); F3H4+ migra
// hardcodes existentes (spawn defaults, lighting defaults, etc.).
//
// Back-compat: campos opcionales (solo se persisten si != default).
// Schema sin bump: agregar fields nuevos no rompe .moodproj viejos
// (mismo patron que `coyoteWindowSec`/`jumpBufferWindowSec` del Hito 40
// G y el cleanup de `HudState.ammo` post-v2.0.2). Forward-compat: keys
// que el codigo no conoce se ignoran silenciosamente.

#include "core/Types.h"

#include <nlohmann/json_fwd.hpp>

#include <string>

namespace Mood {

struct ProjectSettings {
    // === General ===

    /// FPS objetivo del runtime. NO se consume todavia: F3H1 solo
    /// almacena el valor; el cap real (frame pacing) entra en F3H4+
    /// cuando se cableee al frame loop.
    int targetFps = 60;

    /// Descripcion libre del proyecto (autor, proposito, notas).
    /// Vacio por default; persistido solo si no-vacio.
    std::string description;

    // === Sub-secciones futuras (placeholder, vacias en F3H1) ===
    // SpawnDefaults spawnDefaults;     // F3H4
    // RenderingSettings rendering;     // F3H4
    // PhysicsSettings physics;         // F3H4
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
