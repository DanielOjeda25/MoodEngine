#pragma once

// F4H7 — Enemy state machine system. Engine-generic.
//
// Cada frame en Play mode recorre las entities con EnemyComponent +
// HealthComponent y aplica transiciones de la maquina de estados:
//
//   Idle  ──(dist <= aggroRange)──>  Alert
//   Alert ─(F4H8: movement)────────> Chase
//   Chase ─(dist <= attackRange)──>  Attack
//   Attack ─(cooldown)─────────────> Alert / Chase
//   *     ──(damage taken)─────────> Pain ──(duration)──> previous
//   *     ──(HP <= 0)──────────────> Dead (terminal, auto-add Dynamic RB)
//
// F4H7 NO mueve al enemigo (Chase no-op) ni lo hace atacar (Attack no-op).
// La maquina queda armada para que F4H8 active el branch de movimiento sin
// refactor de transiciones.

#include "core/Types.h"

namespace Mood {

class Scene;
class Entity;
class AudioDevice;
class AssetManager;

namespace Enemy {

/// @brief Tick del sistema. Llamar 1 vez por frame en Play mode.
///
/// @param playerEntity Entity tag "player" (resuelto por el bridge). Si
///        invalida, los enemies se quedan en Idle (sin target).
/// @param audio Opcional: si nullptr, se omiten hit/death sounds.
/// @param assets Necesario para resolver `enemyAssetId` → `EnemySpec`.
void tickSystem(Scene& scene, f32 dt, Entity playerEntity,
                AudioDevice* audio, const AssetManager& assets);

} // namespace Enemy
} // namespace Mood
