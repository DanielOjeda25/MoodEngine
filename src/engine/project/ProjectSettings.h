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

#include <vector>

namespace Mood {

/// F3H5: configuracion del character controller per-proyecto. Capsule
/// dimensions + eye height (offset desde centro de capsule) + headbob.
/// Defaults coinciden con los literales del Hito 30 (capsule) + F2H41
/// (headbob/eye). Migrados aca para que el dev pueda tunear el "feel"
/// del personaje sin recompilar (un proyecto FPS arcade quiere capsule
/// mas chica y headbob mas fuerte; uno realista quiere lo opuesto).
struct CharacterSettings {
    /// Half-height del capsule del player en standing en metros. Total
    /// height = (halfHeight + radius) * 2. Default 0.5 → standing 1.8m
    /// con radio 0.4.
    f32 halfHeightStand = 0.5f;

    /// Half-height del capsule en crouching. Default 0.1 → crouch 1.0m.
    f32 halfHeightCrouch = 0.1f;

    /// Radio del capsule en metros (no cambia entre standing/crouch).
    /// Default 0.4 — pasa por puertas standard FPS.
    f32 radius = 0.4f;

    /// Altura de los ojos desde el centro del capsule en standing, en
    /// metros. Default 0.7. El eyeOffset real lo calcula el char
    /// controller como `halfHeight + radius - 0.2` — el `-0.2` queda
    /// hardcoded por ahora (anotado en HARDCODED_AUDIT.md, futuro hito
    /// lo expone como `eyeOffsetBias`).
    f32 eyeHeightStand = 0.7f;

    /// Altura de los ojos en crouching. Default 0.3.
    f32 eyeHeightCrouch = 0.3f;

    /// Frecuencia del headbob en Hz. Default 3.5 — F2H41 cambio de 5.0
    /// a 3.5 explicitamente: "stride humana realista (~1.6m por paso a
    /// walkSpeed 5.5)". El Player runtime quedo en 5.0 pre-F3H5; F3H5
    /// unifica via single source of truth (mismo bug latente que walk
    /// speed cerrado en F3H3).
    f32 headbobFrequency = 3.5f;

    /// Amplitud del headbob en metros. Default 0.05 — F2H41 subio de
    /// 0.04 a 0.05 para "compensar la menor frecuencia y mantener
    /// visibilidad" tras bajar freq. Mismo unify Editor<->Player en F3H5.
    f32 headbobAmplitude = 0.05f;
};

/// F3H6: configuracion del snap del editor per-proyecto. El dev escoge
/// la escala segun el tipo de proyecto: mundo abierto pide pasos grandes
/// (16/32/64/128/256/512), interior detallado pide pasos chicos
/// (0.25/0.5/1/2). Hoy son int — si emerge demanda de fraccionarios,
/// migrar a f32 en sub-hito.
struct SnapSettings {
    /// Pasos disponibles en el snap step picker (Ctrl+= / Ctrl+- /
    /// Ctrl+ScrollWheel). Default Hammer-style {1,2,4,8,16,32,64,128}.
    /// Se serializa ordenado ascendente + dedupe + filter > 0.
    std::vector<int> stepsAvailable = {1, 2, 4, 8, 16, 32, 64, 128};

    /// Indice (en stepsAvailable) del paso inicial al cargar el editor.
    /// Default 4 → 16 unidades (Hammer-style). Clamped al cargar si
    /// el indice queda fuera de rango.
    int defaultStepIndex = 4;

    /// Threshold del snap-to-vertex en coords NDC. 0.02 ~ 8px / 800px
    /// aspect-typical: generoso para que el snap "se pegue" temprano
    /// sin precision al pixel.
    f32 snapToVertexThresholdNdc = 0.02f;

    /// Threshold minimo de broadphase en unidades de mundo. El
    /// broadphase real es `max(snap*2, broadphaseMinWorld)` para que
    /// snaps chicos no enumeren miles de vertices.
    f32 snapBroadphaseMinWorld = 16.0f;

    // ---- F3H20: toggles persistentes + increments. ----

    /// Snap to vertex (orthos del workspace "Editor de mapas" — pincel
    /// + block tool). Threshold screen-space para que el pincel se pegue
    /// a vertices de brushes existentes en los 3 ortos. El gizmo
    /// perspectivo NO usa este toggle (decidido en F3H20 iter 4: snap
    /// vertex en perspectiva resulto "medio raro" comparado a grid snap
    /// estilo Hammer). Default off (Hammer clasico solo grid).
    bool snapToVertexEnabled = false;

    /// F3H20 iter4: grid snap del gizmo perspectivo translate + modal G.
    /// Cuantiza el delta del drag a multiplos de `snapGridStep`. Workflow
    /// estilo Hammer/Source: el objeto se mueve en saltos limpios, sin
    /// target hunting. Convive con las orthos que ya tenian grid snap
    /// (m_hammerSnapStep) — aca expandimos al perspectivo.
    bool snapGridEnabled = false;

    /// Paso del grid del gizmo perspectivo, en unidades de mundo (= metros).
    /// Default 0.5 (granular para escenas chicas). Cycle Ctrl++/Ctrl+- en
    /// la lista [0.125, 0.25, 0.5, 1, 2, 4]. Sub-meter: ideal para FPS
    /// indoor; meter+: ideal para mapas grandes (combine con escala 1u=1m
    /// para que coincida con el grid de impresion mental del dev).
    f32 snapGridStep = 0.5f;

    /// Angle snap (rotate gizmo + modal R libre). Multiplos del incremento.
    /// Default off — el dev lo activa cuando quiere snap a 15/45/90.
    bool snapAngleEnabled = false;

    /// Incremento del angle snap en grados. Defaults Hammer-style 15°
    /// (presets razonables: 5/10/15/30/45/90). Clamp al cargar: > 0,
    /// <= 360.
    f32 snapAngleDegrees = 15.0f;
};

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

    /// F4H1: salud maxima por defecto de entidades con HealthComponent
    /// recien creadas (maniqui, futuros enemigos). Inspector y spawn
    /// usan este valor como base; el dev puede overridearlo per-entity.
    /// Sanitize clamp `[1, 10000]` en `fromJson`.
    f32 maxHealthDefault = 100.0f;
};

struct ProjectSettings {
    // === General ===

    /// FPS objetivo del runtime. F3H1 solo almacena el valor; el cap
    /// real (frame pacing) se cabllea cuando se migre el `Window`
    /// hardcoded a leer de aca (hito siguiente).
    int targetFps = 60;

    /// F3H4: gameplay tier 1 (walk/crouch/jump).
    GameplaySettings gameplay;

    /// F3H5: character controller (capsule + eye + headbob).
    CharacterSettings character;

    /// F3H6: snap del editor (steps + thresholds).
    SnapSettings snap;

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
