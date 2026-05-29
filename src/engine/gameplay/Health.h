#pragma once

// F4H1 — Sistema de salud/daño. Cimiento del combate de Fase 4.
//
// 2 funciones libres + 1 tick del sistema. No hay clase con estado —
// el HealthComponent vive en el registry, las APIs son puras sobre Scene.

#include "core/Types.h"

#include <glm/vec3.hpp>

namespace Mood {

class Scene;
class Entity;

namespace Health {

/// @brief Aplica `amount` de daño a la entidad. Si tiene
///        `HealthComponent`, resta a `current` (clamp >= 0), y al
///        cruzar 0 setea `dead=true` la primera vez (idempotente).
///        Triggea un flash blanco breve (80ms vida, 250ms muerte).
///        `dir` es la dirección del impacto en world space (placeholder
///        para F4H2 — F4H1 no la usa todavía).
///        Sin HealthComponent: no-op silencioso.
///        Loguea al canal `engine` info-level.
void applyDamage(Scene& scene, Entity target, f32 amount,
                  const glm::vec3& dir = glm::vec3(0.0f, 1.0f, 0.0f));

/// @brief Suma `amount` a `current` (clamp a `max`). Si la entidad
///        estaba muerta, NO la revive (decisión: morir es definitivo
///        en F4H1; futura "revive" sería API explícita aparte).
///        Loguea info-level.
void heal(Scene& scene, Entity target, f32 amount);

/// @brief Tick del sistema. Decae `hitFlashTimer` por `dt`, y para
///        las entidades recién muertas que NO tengan `RigidBodyComponent`,
///        agrega uno Dynamic (D2: cae con física). El frame siguiente
///        `EditorApplication::updateRigidBodies` lo materializa en Jolt.
///        Llamar 1 vez por frame en Play mode.
void tickSystem(Scene& scene, f32 dt);

} // namespace Health
} // namespace Mood
