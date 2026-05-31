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

/// @brief Equipa un WeaponSpec en el SLOT ACTIVO de la entidad. Si la
///        entidad no tiene WeaponComponent, lo emplaza. Si el path es
///        vacio, equivale a unequip del slot activo (`weaponAssetId=0`).
///        Resetea `currentAmmo` del slot al mag size del nuevo spec
///        (si != -1 en el componente, respeta lo que tenia — pensado
///        para load de scene). Para equipar en otro slot usar
///        `equipWeaponInSlot`.
/// @return true si la operacion fue exitosa.
bool equipWeapon(Scene& scene, Entity shooter,
                  const std::string& weaponPath,
                  AssetManager& assets);

/// @brief F4H3 — Equipa un WeaponSpec en un slot especifico (no cambia
///        `activeSlot`). Util para inicializar el arsenal del player al
///        spawnear ("dame escopeta en slot 0, pistola en slot 1") o
///        para pickups que ocupan un slot fijo.
///        Path vacio = vacia el slot. No toca `fireTimer/reloadTimer`
///        (otro slot activo puede estar mid-cooldown).
/// @param slotIdx Indice del slot [0, WeaponComponent::k_maxSlots).
/// @return true si la operacion fue exitosa, false si `slotIdx` fuera
///         de rango.
bool equipWeaponInSlot(Scene& scene, Entity shooter, u32 slotIdx,
                        const std::string& weaponPath,
                        AssetManager& assets);

/// @brief F4H3 — Cambia al slot `slotIdx`. No-op si `slotIdx` esta fuera
///        de rango o si ya es el slot activo. Si el slot destino esta
///        vacio (`weaponAssetId==0`), el swap igual procede — el dev
///        decidio activar un slot vacio (HUD muestra "(sin arma)").
///        Actualiza `lastActiveSlot` antes de cambiar. Resetea
///        `fireTimer` + `reloadTimer` a 0 (interrumpe reload en curso).
/// @return true si el swap se ejecuto, false si no-op.
bool swapToSlot(Scene& scene, Entity shooter, u32 slotIdx);

/// @brief F4H3 — Cambia al siguiente slot NO VACIO en orden circular
///        (activeSlot+1, +2, ... wraparound). Si todos los demas slots
///        estan vacios, no-op. Util para scroll wheel up.
/// @return true si el swap se ejecuto, false si no hay otro slot armado.
bool swapNext(Scene& scene, Entity shooter);

/// @brief F4H3 — Cambia al slot NO VACIO anterior en orden circular
///        (activeSlot-1, -2, ... wraparound). Util para scroll wheel down.
/// @return true si el swap se ejecuto, false si no hay otro slot armado.
bool swapPrev(Scene& scene, Entity shooter);

/// @brief F4H3 — Toggle entre `activeSlot` y `lastActiveSlot`. Util para
///        la tecla Q/Tab (HL/Apex style "previous weapon"). No-op si
///        `lastActiveSlot == activeSlot` (no hay arma anterior).
/// @return true si el swap se ejecuto.
bool swapLast(Scene& scene, Entity shooter);

/// @brief F4H3 — Sincroniza el viewmodel a la camara del player y swap
///        del mesh segun el slot activo. Busca entity con tag
///        `"__viewmodel"` (engine-generic — el nombre vive en codigo,
///        no en docs UI) + `ViewmodelComponent`. Si no existe, no-op.
///
///        Comportamiento por frame:
///        1. Resuelve player (tag "player") + WeaponComponent.
///        2. Si `syncMeshOnSwap=true` y el slot activo cambio, swap el
///           MeshRenderer del viewmodel al `spec.viewmodelMesh`. Si el
///           spec no tiene viewmodelMesh, usa `missingMeshId()` (cubo
///           primitivo) como placeholder.
///        3. Setea Transform: position = cameraPos + right*x + up*y +
///           forward*z, rotation = camera-aligned + extraRotEulerDeg.
///
///        Llamar ANTES del SceneRenderer en Play mode, pasando el state
///        actual de la camara del player.
void tickViewmodel(Scene& scene,
                    const glm::vec3& cameraPos,
                    const glm::vec3& cameraForward,
                    const glm::vec3& cameraUp,
                    AssetManager& assets);

/// @brief True si la entidad puede disparar ahora mismo (tiene arma,
///        cooldown OK, no esta reloading, tiene munición).
bool canFire(Scene& scene, Entity shooter, AssetManager& assets);

/// @brief Munición actual del arma equipada. Devuelve 0 si no hay arma
///        o si todavia no se inicializo (currentAmmo == -1).
int ammoLeft(Scene& scene, Entity shooter, AssetManager& assets);

} // namespace Weapon
} // namespace Mood
