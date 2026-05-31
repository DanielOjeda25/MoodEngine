#pragma once

// F4H5 — Projectile system. Engine-generic.
//
// Mueve entities con ProjectileComponent cada frame, detecta colision via
// raycast prevPos -> currentPos, dispara `explode()` (splash damage radial
// con falloff lineal + impact burst + audio impact). Granadas (gravity > 0
// + bounceCount > 0) rebotan en superficies hasta agotar bouncesLeft.

#include "core/Types.h"

#include <glm/vec3.hpp>

namespace Mood {

class Scene;
class PhysicsWorld;
class AudioDevice;
class AssetManager;

namespace Projectile {

/// @brief F4H6: stats del frame para que el bridge dispare game feel triggers.
struct TickStats {
    int explosionCount = 0;       // explosiones este frame
    int damageTargetsHit = 0;     // total entities con Health afectadas
    glm::vec3 lastExplosionCenter{0.0f}; // ultimo center, para shake escalado a distancia
};

/// @brief Tick del sistema. Llamado cada frame en Play mode. Mueve los
///        projectiles, detecta colision, dispara explosion + cleanup.
///
///        `physics` opcional: si nullptr, el sistema mueve igual pero NO
///        detecta colision (proyectiles vuelan hasta agotar lifetime y
///        explotan en su posicion final). Aceptable para tests headless.
///        `audio` opcional: sin device, salta el impactSound.
///
///        Retorna stats del frame (explosiones + targets dañados +
///        ultimo center) para que el bridge trigger camera shake + hit
///        marker escalados al evento. Empty si no hay proyectiles.
TickStats tickSystem(Scene& scene, f32 dt, PhysicsWorld* physics,
                      AudioDevice* audio, AssetManager& assets);

/// @brief Aplica splash damage radial con falloff lineal: entities con
///        HealthComponent dentro del `radius` del `center` reciben
///        `baseDamage * max(0, 1 - dist/radius)`. La entity `ignoreOwner`
///        (si valida) se skipea — convencion para opt-out del rocket-jump
///        cuando `WeaponSpec.ignoreOwner=true`.
///
///        Engine-generic: el caller decide qué constituye "splash"
///        (rocket, barrel explosion, environment hazard, etc.).
///        Retorna el numero de entities dañadas (para hit marker etc).
int applySplashDamage(Scene& scene, const glm::vec3& center, f32 radius,
                       f32 baseDamage, u32 ignoreOwner /*entt::entity raw*/);

} // namespace Projectile
} // namespace Mood
