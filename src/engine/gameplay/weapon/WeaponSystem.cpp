#include "engine/gameplay/weapon/WeaponSystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/audio/clips/AudioClip.h"
#include "engine/audio/device/AudioDevice.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace Mood::Weapon {

namespace {

// xorshift64 — RNG determinista per-frame. Sembrado con el body id +
// pellet index para que dos disparos consecutivos no den el mismo
// patron. NO usar `rand()` global (no determinista en multi-thread).
inline u64 xorshift64(u64& state) {
    if (state == 0) state = 0x9E3779B97F4A7C15ull;
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

inline f32 randFloat01(u64& state) {
    return static_cast<f32>(xorshift64(state) & 0xFFFFFFu)
           / static_cast<f32>(0x1000000u);
}

// F4H3: accessor mutable al slot activo con clamp defensivo. Cualquier
// path que cargue mapas viejos o serializacion corrupta puede dejar
// `activeSlot` fuera de rango — clampeamos a 0 en lugar de UB.
inline WeaponSlot& activeSlotOf(WeaponComponent& wc) {
    if (wc.activeSlot >= WeaponComponent::k_maxSlots) wc.activeSlot = 0;
    return wc.slots[wc.activeSlot];
}
inline const WeaponSlot& activeSlotOf(const WeaponComponent& wc) {
    const u32 idx = (wc.activeSlot < WeaponComponent::k_maxSlots)
                    ? wc.activeSlot : 0u;
    return wc.slots[idx];
}

// Construye una direccion perturbada dentro de un cono de semi-angulo
// `maxAngleDeg` alrededor de `forward`. Distribucion uniforme sobre el
// disco proyectado del cono (no uniforme en angulo: tipico shotgun feel
// — mas pellets cerca del centro).
glm::vec3 perturbConeDirection(const glm::vec3& forward, f32 maxAngleDeg,
                                  u64& rngState) {
    if (maxAngleDeg <= 0.0f) return forward;

    // Base orthonormal del cono. Cualquier vector no paralelo a forward
    // sirve — uso world-up; si forward es casi vertical, switch a +X.
    glm::vec3 up = (std::abs(forward.y) < 0.999f)
        ? glm::vec3(0.0f, 1.0f, 0.0f)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    up              = glm::normalize(glm::cross(right, forward));

    const f32 maxAngleRad = glm::radians(maxAngleDeg);
    const f32 cosMax = std::cos(maxAngleRad);

    // Muestra direccion uniforme en el cono solido (Archimedes hat-box).
    const f32 z = cosMax + (1.0f - cosMax) * randFloat01(rngState);
    const f32 phi = 2.0f * glm::pi<f32>() * randFloat01(rngState);
    const f32 r = std::sqrt(std::max(0.0f, 1.0f - z * z));

    return glm::normalize(forward * z
                          + right   * (r * std::cos(phi))
                          + up      * (r * std::sin(phi)));
}

// Spawn de un particle burst one-shot. Crea una entity efimera con
// ParticleEmitterComponent + ParticleBurstComponent. WeaponSystem::
// tickSystem la destruye cuando el ttl expira. NO requiere un .moodvfx
// real — usa un puff procedural rojo-naranja que sirve como impacto
// generico (el dev puede sobreescribir por arma via spec.impactVfx en
// F4H2.1 cuando agreguemos VFX assets).
void spawnImpactBurst(Scene& scene, const glm::vec3& worldPos,
                       const glm::vec3& normal) {
    Entity e = scene.createEntity("__weapon_impact_burst");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = worldPos;
    tf.rotationEuler = glm::vec3(0.0f);
    tf.scale = glm::vec3(1.0f);

    ParticleEmitterComponent emitter{};
    emitter.emissionShape = ParticleEmitterComponent::EmissionShape::Point;
    emitter.emitRate = 0.0f; // burst manual via accumulator
    emitter.lifetimeMin = 0.10f;
    emitter.lifetimeMax = 0.30f;
    // Velocidades hemisferio en la direccion de la normal (chispas
    // alejandose del punto de impact). Ancho ~0.6 m/s lateral, 1.5 m/s
    // en la normal. Magnitudes a ojo — F4H2.1 expone via .moodvfx.
    const glm::vec3 n = glm::normalize(normal);
    emitter.velocityMin = n * 0.5f - glm::vec3(0.6f, 0.6f, 0.6f);
    emitter.velocityMax = n * 2.5f + glm::vec3(0.6f, 0.6f, 0.6f);
    emitter.sizeStart = 0.10f;
    emitter.sizeEnd   = 0.02f;
    emitter.colorStart = glm::vec4(1.0f, 0.85f, 0.3f, 1.0f); // chispa amarilla
    emitter.colorEnd   = glm::vec4(1.0f, 0.20f, 0.0f, 0.0f); // rojo transparente
    emitter.additive = true;
    emitter.gravityFactor = 0.6f;
    emitter.maxParticles = 32;
    // Pre-emit: el accumulator empieza con 20 particulas pendientes; el
    // ParticleSystem las spawnea en el siguiente update.
    emitter.emitAccumulator = 20.0f;
    e.addComponent<ParticleEmitterComponent>(emitter);

    // TTL = lifetimeMax + buffer para asegurar que las particulas mueran
    // antes de destruir la entidad.
    ParticleBurstComponent burst{};
    burst.ttl = emitter.lifetimeMax + 0.1f;
    e.addComponent<ParticleBurstComponent>(burst);
}

// F4H3: resuelve el Spec del slot activo. Devuelve nullptr si no hay
// WeaponComponent, si el slot activo esta vacio, o si el id es invalido.
const Spec* resolveSpec(Scene& /*scene*/, Entity shooter,
                          const AssetManager& assets) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return nullptr;
    const auto& wc = shooter.getComponent<WeaponComponent>();
    const auto& slot = activeSlotOf(wc);
    if (slot.weaponAssetId == 0) return nullptr;
    return assets.getWeapon(slot.weaponAssetId);
}

// Tag debug para logs.
std::string tagOf(Entity e) {
    if (e && e.hasComponent<TagComponent>())
        return e.getComponent<TagComponent>().name;
    return std::string("<sin-tag>");
}

} // namespace

FireResult fire(Scene& scene,
                  Entity shooter,
                  const FireParams& params,
                  PhysicsWorld& physics,
                  AudioDevice& audio,
                  AssetManager& assets) {
    FireResult out{};

    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return out;
    auto& wc = shooter.getComponent<WeaponComponent>();
    auto& slot = activeSlotOf(wc);

    const Spec* spec = resolveSpec(scene, shooter, assets);
    if (spec == nullptr) return out;

    // Solo hitscan en F4H2/F4H3. Otras categorias son no-op forward-compat.
    if (spec->category != "hitscan") {
        Log::engine()->warn(
            "[weapon] '{}': category '{}' no soportada (solo hitscan)",
            tagOf(shooter), spec->category);
        return out;
    }

    // Auto-inicializar ammo si nunca se hizo (currentAmmo == -1).
    if (slot.currentAmmo < 0) {
        slot.currentAmmo = static_cast<int>(spec->magazineSize);
    }

    // Validaciones de estado.
    if (wc.reloadTimer > 0.0f) return out;
    if (wc.fireTimer > 0.0f)   return out;
    if (slot.currentAmmo <= 0) return out;

    // Direccion forward (no asumir normalizado en el caller).
    const glm::vec3 forward = (glm::length(params.direction) > 0.0001f)
        ? glm::normalize(params.direction)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    // Ignored body: si el Spec lo pide y el caller proveyo uno.
    const u32 ignoredId = spec->ignoreOwner ? params.ignoredBodyId : 0u;

    // RNG sembrado con el id de la entidad shooter + el ammo actual.
    // Da reproducibilidad por estado del arma sin necesidad de wall clock.
    u64 rng = (static_cast<u64>(static_cast<u32>(shooter.handle())) << 17)
              ^ static_cast<u64>(slot.currentAmmo)
              ^ 0xA02BDBF7BB3C0A7Bull;

    bool firstHitRecorded = false;

    for (u32 i = 0; i < spec->pellets; ++i) {
        const glm::vec3 dir = perturbConeDirection(forward, spec->spreadDeg, rng);
        const auto hit = physics.raycast(params.origin, dir, spec->range,
                                            ignoredId);
        ++out.pelletsFired;
        if (!hit.hit) continue;
        ++out.pelletsHit;

        // Resolver entidad del body impactado.
        const u32 entityHandle = physics.entityOfBody(hit.bodyId);
        Entity victim = (entityHandle != 0)
            ? Entity{static_cast<entt::entity>(entityHandle), &scene}
            : Entity{};

        // Aplica danno si la victima tiene HealthComponent.
        if (victim && victim.hasComponent<HealthComponent>()) {
            Health::applyDamage(scene, victim, spec->damage, dir);
            if (!firstHitRecorded) {
                out.damagedTarget = entityHandle;
                out.firstHitPoint = hit.point;
                firstHitRecorded = true;
            }
        }

        // Feedback al impacto (D2: sonido + particula siempre).
        if (!spec->impactSound.empty()) {
            const auto clipId = assets.loadAudio(spec->impactSound);
            if (AudioClip* clip = assets.getAudio(clipId)) {
                audio.play(*clip, 1.0f, false, true, hit.point);
            }
        }
        spawnImpactBurst(scene, hit.point, hit.normal);

        if (!firstHitRecorded) {
            out.firstHitPoint = hit.point;
            firstHitRecorded = true;
        }
    }

    // Sonido de disparo (muzzle), una sola vez por shot. 3D positional
    // en origin del shooter — para 1ra persona el listener esta en el
    // mismo punto asi que se escucha "en la cara".
    if (!spec->fireSound.empty()) {
        const auto clipId = assets.loadAudio(spec->fireSound);
        if (AudioClip* clip = assets.getAudio(clipId)) {
            audio.play(*clip, 1.0f, false, true, params.origin);
        }
    }

    // Consumir munición + arrancar cooldown.
    --slot.currentAmmo;
    wc.fireTimer = 1.0f / spec->fireRatePerSec;
    out.fired = true;

    Log::engine()->info(
        "[weapon] '{}' disparo '{}' ({} pellet{}, {} hit{}, ammo {}/{} slot {})",
        tagOf(shooter), spec->displayName.empty() ? std::string("(unnamed)")
                                                     : spec->displayName,
        out.pelletsFired, out.pelletsFired == 1 ? "" : "s",
        out.pelletsHit,   out.pelletsHit == 1 ? "" : "s",
        slot.currentAmmo, spec->magazineSize, wc.activeSlot);

    return out;
}

bool reload(Scene& scene, Entity shooter, AssetManager& assets) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return false;
    auto& wc = shooter.getComponent<WeaponComponent>();
    auto& slot = activeSlotOf(wc);

    const Spec* spec = resolveSpec(scene, shooter, assets);
    if (spec == nullptr) return false;

    // Ya esta full -> no-op.
    if (slot.currentAmmo == static_cast<int>(spec->magazineSize)) return false;
    // Ya esta reloading -> no extender.
    if (wc.reloadTimer > 0.0f) return true;

    wc.reloadTimer = spec->reloadTimeSec;
    Log::engine()->info("[weapon] '{}' reload start ({:.2f}s, slot {})",
                         tagOf(shooter), spec->reloadTimeSec, wc.activeSlot);
    return true;
}

void tickSystem(Scene& scene, f32 dt, AssetManager& assets) {
    auto& reg = scene.registry();

    // --- Weapon timers ---
    reg.view<WeaponComponent>().each([&](entt::entity h, WeaponComponent& wc) {
        auto& slot = activeSlotOf(wc);

        if (wc.fireTimer > 0.0f) {
            wc.fireTimer = std::max(0.0f, wc.fireTimer - dt);
        }
        if (wc.reloadTimer > 0.0f) {
            const f32 before = wc.reloadTimer;
            wc.reloadTimer = std::max(0.0f, wc.reloadTimer - dt);
            // Reload completo: rellenar mag del slot activo.
            if (before > 0.0f && wc.reloadTimer == 0.0f) {
                if (slot.weaponAssetId != 0) {
                    if (const Spec* spec = assets.getWeapon(slot.weaponAssetId)) {
                        slot.currentAmmo = static_cast<int>(spec->magazineSize);
                        const std::string tag =
                            reg.all_of<TagComponent>(h)
                                ? reg.get<TagComponent>(h).name
                                : std::string("<sin-tag>");
                        Log::engine()->info(
                            "[weapon] '{}' reload completo ({}/{} slot {})",
                            tag, slot.currentAmmo, spec->magazineSize,
                            wc.activeSlot);
                    }
                }
            }
        }
        // Auto-clear del flag de input — el bridge lo re-setea cada
        // frame si el boton sigue sostenido.
        wc.firing = false;
    });

    // --- Particle burst cleanup ---
    std::vector<entt::entity> toDestroy;
    reg.view<ParticleBurstComponent>().each(
        [&](entt::entity h, ParticleBurstComponent& b) {
            b.ttl -= dt;
            if (b.ttl <= 0.0f) toDestroy.push_back(h);
        });
    for (entt::entity h : toDestroy) {
        scene.destroyEntity(Entity{h, &scene});
    }
}

bool equipWeapon(Scene& /*scene*/, Entity shooter,
                  const std::string& weaponPath,
                  AssetManager& assets) {
    if (!shooter) return false;

    WeaponComponent* wc = shooter.hasComponent<WeaponComponent>()
        ? &shooter.getComponent<WeaponComponent>()
        : &shooter.addComponent<WeaponComponent>();

    // F4H3: equipWeapon opera sobre el slot ACTIVO (back-compat con
    // F4H2 API). Para equipar en un slot especifico usar
    // `equipWeaponInSlot(scene, e, slot, path)`.
    WeaponSlot& slot = activeSlotOf(*wc);

    if (weaponPath.empty()) {
        slot.weaponAssetId = 0;
        slot.currentAmmo   = -1;
        wc->fireTimer      = 0.0f;
        wc->reloadTimer    = 0.0f;
        Log::engine()->info("[weapon] '{}' unequipped (slot {})",
                             tagOf(shooter), wc->activeSlot);
        return true;
    }

    const auto id = assets.loadWeapon(weaponPath);
    slot.weaponAssetId = id;
    wc->fireTimer      = 0.0f;
    wc->reloadTimer    = 0.0f;
    // Si currentAmmo nunca se inicializo o se viene de un weapon distinto,
    // resetear al magsize nuevo. Si el caller carga la scene con un valor
    // guardado, lo respeta (el SceneLoader pasa por path setea sin tocar
    // ammo despues).
    if (slot.currentAmmo < 0) {
        if (const Spec* spec = assets.getWeapon(id)) {
            slot.currentAmmo = static_cast<int>(spec->magazineSize);
        }
    }
    Log::engine()->info("[weapon] '{}' equipped '{}' (id {} slot {})",
                         tagOf(shooter), weaponPath, id, wc->activeSlot);
    return true;
}

bool equipWeaponInSlot(Scene& /*scene*/, Entity shooter, u32 slotIdx,
                        const std::string& weaponPath, AssetManager& assets) {
    if (!shooter) return false;
    if (slotIdx >= WeaponComponent::k_maxSlots) {
        Log::engine()->warn(
            "[weapon] equipWeaponInSlot: slot {} fuera de rango [0,{})",
            slotIdx, WeaponComponent::k_maxSlots);
        return false;
    }

    WeaponComponent* wc = shooter.hasComponent<WeaponComponent>()
        ? &shooter.getComponent<WeaponComponent>()
        : &shooter.addComponent<WeaponComponent>();

    WeaponSlot& slot = wc->slots[slotIdx];

    if (weaponPath.empty()) {
        slot.weaponAssetId = 0;
        slot.currentAmmo   = -1;
        // No tocar timers globales — otro slot activo puede estar mid-cooldown.
        Log::engine()->info("[weapon] '{}' slot {} unequipped",
                             tagOf(shooter), slotIdx);
        return true;
    }

    const auto id = assets.loadWeapon(weaponPath);
    slot.weaponAssetId = id;
    if (slot.currentAmmo < 0) {
        if (const Spec* spec = assets.getWeapon(id)) {
            slot.currentAmmo = static_cast<int>(spec->magazineSize);
        }
    }
    Log::engine()->info("[weapon] '{}' slot {} <- '{}' (id {})",
                         tagOf(shooter), slotIdx, weaponPath, id);
    return true;
}

// F4H3: helper interno para los 4 swap APIs. Aplica el cambio +
// resetea timers + actualiza lastActiveSlot + logea. Asume que
// `wc.activeSlot != newSlot` y `newSlot < k_maxSlots`.
namespace {

void applySwap(Entity shooter, WeaponComponent& wc, u32 newSlot) {
    wc.lastActiveSlot = wc.activeSlot;
    wc.activeSlot     = newSlot;
    wc.fireTimer      = 0.0f;
    wc.reloadTimer    = 0.0f;
    const std::string tag = (shooter && shooter.hasComponent<TagComponent>())
        ? shooter.getComponent<TagComponent>().name
        : std::string("<sin-tag>");
    Log::engine()->info("[weapon] '{}' swap slot {} -> {} (last {})",
                         tag, wc.lastActiveSlot, wc.activeSlot,
                         wc.lastActiveSlot);
}

} // namespace

bool swapToSlot(Scene& /*scene*/, Entity shooter, u32 slotIdx) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return false;
    if (slotIdx >= WeaponComponent::k_maxSlots) return false;
    auto& wc = shooter.getComponent<WeaponComponent>();
    if (wc.activeSlot == slotIdx) return false; // no-op
    applySwap(shooter, wc, slotIdx);
    return true;
}

bool swapNext(Scene& /*scene*/, Entity shooter) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return false;
    auto& wc = shooter.getComponent<WeaponComponent>();
    constexpr u32 N = WeaponComponent::k_maxSlots;
    // Busca el siguiente slot no-vacio en orden circular (skip current).
    for (u32 step = 1; step < N; ++step) {
        const u32 idx = (wc.activeSlot + step) % N;
        if (wc.slots[idx].weaponAssetId != 0) {
            applySwap(shooter, wc, idx);
            return true;
        }
    }
    return false; // sin otro slot armado
}

bool swapPrev(Scene& /*scene*/, Entity shooter) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return false;
    auto& wc = shooter.getComponent<WeaponComponent>();
    constexpr u32 N = WeaponComponent::k_maxSlots;
    for (u32 step = 1; step < N; ++step) {
        const u32 idx = (wc.activeSlot + N - step) % N;
        if (wc.slots[idx].weaponAssetId != 0) {
            applySwap(shooter, wc, idx);
            return true;
        }
    }
    return false;
}

bool swapLast(Scene& /*scene*/, Entity shooter) {
    if (!shooter || !shooter.hasComponent<WeaponComponent>()) return false;
    auto& wc = shooter.getComponent<WeaponComponent>();
    if (wc.lastActiveSlot == wc.activeSlot) return false; // sin arma anterior
    if (wc.lastActiveSlot >= WeaponComponent::k_maxSlots) return false;
    applySwap(shooter, wc, wc.lastActiveSlot);
    return true;
}

void tickViewmodel(Scene& scene,
                    const glm::vec3& cameraPos,
                    const glm::vec3& cameraForward,
                    const glm::vec3& cameraUp,
                    AssetManager& assets) {
    // 1. Buscar entity viewmodel + entity player con WeaponComponent.
    Entity viewmodelEntity{};
    Entity playerEntity{};
    auto& reg = scene.registry();
    reg.view<TagComponent>().each([&](entt::entity h, TagComponent& tag) {
        if (tag.name == "__viewmodel") {
            viewmodelEntity = Entity{h, &scene};
        } else if (tag.name == "player") {
            playerEntity = Entity{h, &scene};
        }
    });
    if (!viewmodelEntity || !viewmodelEntity.hasComponent<ViewmodelComponent>()) {
        return;
    }
    if (!playerEntity || !playerEntity.hasComponent<WeaponComponent>()) {
        return;
    }

    auto& vm = viewmodelEntity.getComponent<ViewmodelComponent>();
    auto& wc = playerEntity.getComponent<WeaponComponent>();
    const WeaponSlot& activeSlot = wc.slots[
        (wc.activeSlot < WeaponComponent::k_maxSlots) ? wc.activeSlot : 0u];

    // 2. Sync mesh si el slot activo cambio (o es primer tick).
    if (vm.syncMeshOnSwap && activeSlot.weaponAssetId != vm.lastSeenWeaponId) {
        vm.lastSeenWeaponId = activeSlot.weaponAssetId;
        if (viewmodelEntity.hasComponent<MeshRendererComponent>()) {
            auto& mr = viewmodelEntity.getComponent<MeshRendererComponent>();
            MeshAssetId targetMesh = assets.missingMeshId(); // cubo fallback
            if (activeSlot.weaponAssetId != 0) {
                if (const Spec* spec = assets.getWeapon(activeSlot.weaponAssetId)) {
                    if (!spec->viewmodelMesh.empty()) {
                        targetMesh = assets.loadMesh(spec->viewmodelMesh);
                    }
                }
            }
            mr.mesh = targetMesh;
            // Materiales se quedan como esten (fallback default si missing).
            // F4H3.1 puede asignar `spec.viewmodelMaterial` aca.
        }
    }

    // 3. Sync transform: posicion = cameraPos + right*x + up*y + forward*z.
    //    rotacion = camara-aligned + extraRotEulerDeg.
    if (viewmodelEntity.hasComponent<TransformComponent>()) {
        auto& tf = viewmodelEntity.getComponent<TransformComponent>();
        const glm::vec3 forward = glm::normalize(cameraForward);
        const glm::vec3 right   = glm::normalize(glm::cross(forward,
                                                              glm::normalize(cameraUp)));
        const glm::vec3 up      = glm::normalize(glm::cross(right, forward));

        tf.position = cameraPos
                    + right   * vm.offsetCamSpace.x
                    + up      * vm.offsetCamSpace.y
                    + forward * vm.offsetCamSpace.z;
        // Yaw + pitch derivados de forward.
        const f32 yaw   = glm::degrees(std::atan2(forward.x, -forward.z));
        const f32 pitch = glm::degrees(std::asin(forward.y));
        tf.rotationEuler = glm::vec3(pitch, yaw, 0.0f) + vm.extraRotEulerDeg;
        tf.scale = vm.scale;
    }
}

bool canFire(Scene& scene, Entity shooter, AssetManager& assets) {
    const Spec* spec = resolveSpec(scene, shooter, assets);
    if (spec == nullptr) return false;
    const auto& wc = shooter.getComponent<WeaponComponent>();
    const auto& slot = activeSlotOf(wc);
    if (wc.reloadTimer > 0.0f) return false;
    if (wc.fireTimer > 0.0f)   return false;
    const int ammo = (slot.currentAmmo < 0)
        ? static_cast<int>(spec->magazineSize) : slot.currentAmmo;
    return ammo > 0;
}

int ammoLeft(Scene& scene, Entity shooter, AssetManager& assets) {
    const Spec* spec = resolveSpec(scene, shooter, assets);
    if (spec == nullptr) return 0;
    const auto& wc = shooter.getComponent<WeaponComponent>();
    const auto& slot = activeSlotOf(wc);
    if (slot.currentAmmo < 0) return static_cast<int>(spec->magazineSize);
    return slot.currentAmmo;
}

} // namespace Mood::Weapon
