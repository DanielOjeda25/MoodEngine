#include "engine/gameplay/projectile/ProjectileSystem.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/audio/device/AudioDevice.h"
#include "engine/audio/clips/AudioClip.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <vector>

namespace Mood {
namespace Projectile {

namespace {

// Spawnea un impact burst chiquito en el punto de explosion. Reuso del
// patron de WeaponSystem::spawnImpactBurst pero con tinte naranja-rojo
// + mas particulas (explosion vs spark de hitscan).
void spawnExplosionBurst(Scene& scene, const glm::vec3& worldPos, f32 radius) {
    Entity e = scene.createEntity("__projectile_explosion");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = worldPos;
    tf.scale = glm::vec3(std::max(0.5f, radius * 0.25f));

    ParticleEmitterComponent emitter{};
    emitter.emissionShape = ParticleEmitterComponent::EmissionShape::Point;
    emitter.emitRate = 0.0f;
    emitter.lifetimeMin = 0.15f;
    emitter.lifetimeMax = 0.45f;
    // Velocidades omnidireccionales (esferica).
    emitter.velocityMin = glm::vec3(-3.0f);
    emitter.velocityMax = glm::vec3(3.0f);
    emitter.sizeStart = std::max(0.2f, radius * 0.15f);
    emitter.sizeEnd   = 0.05f;
    emitter.colorStart = glm::vec4(1.0f, 0.7f, 0.2f, 1.0f);   // naranja vivo
    emitter.colorEnd   = glm::vec4(0.3f, 0.05f, 0.0f, 0.0f);   // marron oscuro fade
    emitter.additive = true;
    emitter.gravityFactor = 0.3f;
    emitter.maxParticles = 64;
    emitter.emitAccumulator = 48.0f;
    e.addComponent<ParticleEmitterComponent>(emitter);

    ParticleBurstComponent burst{};
    burst.ttl = emitter.lifetimeMax + 0.1f;
    e.addComponent<ParticleBurstComponent>(burst);
}

// Aplica explosion al final del frame: spawn burst + audio + splash damage
// + marca el projectile exploded=true. NO destruye el entity (eso lo hace
// el cleanup del tick).
void explode(Scene& scene, Entity proj, const glm::vec3& center,
              const Weapon::Spec& spec, AudioDevice* audio,
              AssetManager& assets) {
    auto& p = proj.getComponent<ProjectileComponent>();
    if (p.exploded) return;
    p.exploded = true;

    const u32 ignoreOwner = spec.ignoreOwner ? p.owner : 0u;

    applySplashDamage(scene, center, spec.projectile.splashRadius,
                       spec.projectile.splashDamage, ignoreOwner);
    spawnExplosionBurst(scene, center, spec.projectile.splashRadius);

    if (audio && !spec.impactSound.empty()) {
        const auto clipId = assets.loadAudio(spec.impactSound);
        if (AudioClip* clip = assets.getAudio(clipId)) {
            audio->play(*clip, 1.0f, false, true, center);
        }
    }

    Log::engine()->info(
        "[projectile] explosion at ({:.1f},{:.1f},{:.1f}) splash={}m dmg={}",
        center.x, center.y, center.z,
        spec.projectile.splashRadius, spec.projectile.splashDamage);
}

} // namespace

void applySplashDamage(Scene& scene, const glm::vec3& center, f32 radius,
                        f32 baseDamage, u32 ignoreOwnerRaw) {
    if (radius <= 0.0f || baseDamage <= 0.0f) return;
    auto& reg = scene.registry();
    const entt::entity ignoreOwner = static_cast<entt::entity>(ignoreOwnerRaw);

    reg.view<HealthComponent, TransformComponent>().each(
        [&](entt::entity ent, HealthComponent& hc, const TransformComponent& tf) {
            (void)hc;
            if (ent == ignoreOwner) return;
            const glm::vec3 d = tf.position - center;
            const f32 dist = glm::length(d);
            if (dist > radius) return;
            const f32 falloff = std::max(0.0f, 1.0f - dist / radius);
            const f32 dmg = baseDamage * falloff;
            if (dmg <= 0.0f) return;
            Entity victim{ent, &scene};
            Health::applyDamage(scene, victim, dmg, glm::normalize(
                glm::vec3(d.x, std::max(0.1f, d.y), d.z)));
        });
}

void tickSystem(Scene& scene, f32 dt, PhysicsWorld* physics,
                 AudioDevice* audio, AssetManager& assets) {
    auto& reg = scene.registry();

    // Pass 1: avanzar + detectar colision + explode.
    std::vector<entt::entity> toDestroy;

    reg.view<ProjectileComponent, TransformComponent>().each(
        [&](entt::entity ent, ProjectileComponent& p, TransformComponent& tf) {
            Entity proj{ent, &scene};
            const auto* spec = assets.getWeapon(p.weaponAssetId);
            if (!spec) {
                // Sin spec — desconocido, marca para destroy.
                toDestroy.push_back(ent);
                return;
            }

            if (p.exploded) {
                toDestroy.push_back(ent);
                return;
            }

            // Aging + lifetime.
            p.age += dt;
            if (p.age >= p.lifetimeSec) {
                explode(scene, proj, tf.position, *spec, audio, assets);
                toDestroy.push_back(ent);
                return;
            }

            // Aplicar gravedad si corresponde (granada).
            if (spec->projectile.gravity > 0.0f) {
                p.velocity.y -= spec->projectile.gravity * dt;
            }

            // Movimiento por integracion explicita.
            p.prevPos = tf.position;
            const glm::vec3 delta = p.velocity * dt;
            const glm::vec3 newPos = tf.position + delta;
            const f32 distance = glm::length(delta);

            // Raycast prevPos -> newPos para detectar colision continua.
            // Sin physics (tests headless), saltea — el proyectil sigue
            // volando hasta lifetime expire.
            bool hitSomething = false;
            glm::vec3 hitPoint{0.0f};
            glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
            u32 hitBodyEntity = 0;

            if (physics && distance > 1e-4f) {
                const glm::vec3 dir = delta / distance;
                const u32 ignoredBody = p.owner;
                const auto hit = physics->raycast(p.prevPos, dir, distance,
                                                    ignoredBody);
                if (hit.hit) {
                    hitSomething = true;
                    hitPoint  = hit.point;
                    hitNormal = hit.normal;
                    hitBodyEntity = physics->entityOfBody(hit.bodyId);
                }
            }

            if (!hitSomething) {
                tf.position = newPos;
                return;
            }

            // Hit! Si granada con bouncesLeft > 0 y NO impacto entity con
            // Health, rebota. Si pego a entity con Health (enemy), explota
            // siempre (no rebota).
            const bool hitHealthEntity = hitBodyEntity != 0
                && reg.valid(static_cast<entt::entity>(hitBodyEntity))
                && reg.all_of<HealthComponent>(
                       static_cast<entt::entity>(hitBodyEntity));

            if (p.bouncesLeft > 0 && !hitHealthEntity) {
                // Rebote: reflejar velocity sobre la normal, decrementar bounce.
                tf.position = hitPoint + hitNormal * 0.05f;
                const glm::vec3 n = glm::normalize(hitNormal);
                const glm::vec3 reflected =
                    p.velocity - 2.0f * glm::dot(p.velocity, n) * n;
                p.velocity = reflected * spec->projectile.bounceFactor;
                p.bouncesLeft -= 1;

                // Direct damage al hit entity si tiene Health (granada al pasar
                // rozando con un enemy podria pegarle aunque no explote).
                // Pero como hitHealthEntity es false aca, este branch no aplica.
                return;
            }

            // No rebota → explosion. Direct damage al hit entity si Health.
            if (hitHealthEntity) {
                Entity victim{static_cast<entt::entity>(hitBodyEntity), &scene};
                Health::applyDamage(scene, victim, spec->projectile.directDamage,
                                     glm::normalize(p.velocity));
            }
            explode(scene, proj, hitPoint, *spec, audio, assets);
            toDestroy.push_back(ent);
        });

    // Cleanup.
    for (entt::entity ent : toDestroy) {
        if (reg.valid(ent)) reg.destroy(ent);
    }
}

} // namespace Projectile
} // namespace Mood
