#pragma once

// WeaponSystem (F4H2): logica engine-generic de armas hitscan.
//
// 3 funciones libres (+ helpers) sobre `Scene` + dependencias inyectadas.
// No hay clase con estado — el WeaponComponent vive en el registry; las
// APIs son puras sobre la scene + el PhysicsWorld + el AudioDevice + el
// AssetManager.
//
// Engine-generic: el sistema solo entiende `WeaponSpec::category ==
// "hitscan"` en F4H2. Otras categorias (`projectile`, `melee`) son no-op
// en `fire` — forward compat con hitos siguientes.
//
// Frontera con el juego:
// - Engine provee este sistema + el ABI de input (firing flag).
// - Juego provee los .moodweapon files + scripts que orquestan disparo.
// - PANDEMONIUM no aparece por nombre en ninguna parte del engine.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

class AssetManager;
class AudioDevice;
class Entity;
class PhysicsWorld;
class Scene;

namespace Weapon {

/// @brief Parametros del disparo — empaqueta lo que viene del jugador
///        (origin/direction del raycast, body a ignorar). Se pasa por
///        valor: struct chica + claridad en el callsite.
struct FireParams {
    glm::vec3 origin{0.0f};         // world-space del muzzle (tipicamente camera pos)
    glm::vec3 direction{0.0f, 0.0f, -1.0f}; // forward del shooter (no requiere normalizado)
    u32       ignoredBodyId = 0;    // body del shooter para no auto-detectarse
};

/// @brief Salida del disparo — quien recibio danno + donde pego. Util
///        para que el caller (scripts, HUD) reaccione (ej. "hit marker"
///        si `damagedTarget` != 0). Si el disparo no se ejecuto
///        (cooldown, sin munición, sin arma), `fired == false` y los
///        demas campos quedan en default.
struct FireResult {
    bool      fired         = false; // true si el disparo se ejecuto
    u32       pelletsFired  = 0;     // cantidad de raycasts disparados
    u32       pelletsHit    = 0;     // cuantos pegaron en algo
    u32       damagedTarget = 0;     // entity id del primer hit con HealthComponent (0 si none)
    glm::vec3 firstHitPoint{0.0f};   // world-space del primer impact (si hay)
};

/// @brief Aplica el ciclo de disparo. Si la entidad no tiene
///        `WeaponComponent`, retorna `fired=false`. Si el cooldown
///        (`fireTimer > 0`) no expiro, idem. Si `currentAmmo == 0`,
///        idem (caller debe llamar `reload`).
///
///        Para cada pellet del Spec:
///        1. Calcula direccion con dispersion conica (`spreadDeg`).
///        2. Raycast contra el PhysicsWorld (`spec.range`).
///        3. Si hay hit con entidad que tiene HealthComponent,
///           aplica `Health::applyDamage(spec.damage)`.
///        4. Spawn particle burst + sonido en el punto de impacto.
///
///        Spawn de muzzle sound + (opcional) particle muzzle en el
///        origin del shooter.
///
///        Decrementa `currentAmmo`. Resetea `fireTimer` a `1/fireRate`.
///
///        Loguea info al canal `engine` con tag + spec name + hits/total.
FireResult fire(Scene& scene,
                  Entity shooter,
                  const FireParams& params,
                  PhysicsWorld& physics,
                  AudioDevice& audio,
                  AssetManager& assets);

/// @brief Arranca el reload — setea `reloadTimer = spec.reloadTimeSec`.
///        Bloquea fire mientras `reloadTimer > 0`. Idempotente: re-call
///        durante el reload no extiende.
/// @return true si el reload arranco (o ya estaba en curso), false si
///         no hay arma equipada o ya esta full.
bool reload(Scene& scene, Entity shooter, AssetManager& assets);

/// @brief Tick del sistema. Decae `fireTimer` + `reloadTimer` de todas
///        las entidades con WeaponComponent. Cuando `reloadTimer` cruza
///        0, recarga el mag a `spec.magazineSize`. Llamar 1 vez por
///        frame en Play mode.
///
///        Tambien cleanup de particle bursts one-shot: entidades con
///        `ParticleBurstComponent` cuyo ttl expiro se destruyen.
void tickSystem(Scene& scene, f32 dt, AssetManager& assets);

/// @brief Equipa un WeaponSpec en una entidad. Si la entidad no tiene
///        WeaponComponent, lo emplaza. Si el path es vacio, equivale a
///        unequip (`weaponAssetId = 0`).
///        Resetea `currentAmmo` al mag size del nuevo spec (si != -1
///        en el componente, respeta lo que tenia — pensado para load
///        de scene).
/// @return true si la operacion fue exitosa.
bool equipWeapon(Scene& scene, Entity shooter,
                  const std::string& weaponPath,
                  AssetManager& assets);

/// @brief True si la entidad puede disparar ahora mismo (tiene arma,
///        cooldown OK, no esta reloading, tiene munición).
bool canFire(Scene& scene, Entity shooter, AssetManager& assets);

/// @brief Munición actual del arma equipada. Devuelve 0 si no hay arma
///        o si todavia no se inicializo (currentAmmo == -1).
int ammoLeft(Scene& scene, Entity shooter, AssetManager& assets);

} // namespace Weapon
} // namespace Mood
