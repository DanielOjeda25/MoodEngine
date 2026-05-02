#include "systems/ParticleSystem.h"

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Mood {

namespace {

constexpr f32 k_gravity = -9.81f;

// xorshift64* — RNG deterministico por emisor (rngState). Sin <random>
// porque queremos repetibilidad cross-platform y zero-overhead.
inline u64 xorshift64(u64& s) {
    if (s == 0) s = 0xC0FFEEu;
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 0x2545F4914F6CDD1Dull;
}

// Random float en [0, 1).
inline f32 randUnit(u64& s) {
    constexpr f32 inv = 1.0f / 18446744073709551616.0f; // 2^64
    return static_cast<f32>(xorshift64(s)) * inv;
}

// Lerp componente a componente.
inline glm::vec3 randVec3(u64& s, const glm::vec3& a, const glm::vec3& b) {
    return glm::vec3(
        a.x + randUnit(s) * (b.x - a.x),
        a.y + randUnit(s) * (b.y - a.y),
        a.z + randUnit(s) * (b.z - a.z));
}

// Asegura que las SoA tengan tamano `cap`. Las recien-creadas quedan
// todas en `alive=0`. Idempotente si ya estan al tamano.
void ensurePoolSized(ParticleEmitterComponent& em, u32 cap) {
    if (em.positions.size() == cap) return;
    em.positions.assign(cap, glm::vec3(0.0f));
    em.velocities.assign(cap, glm::vec3(0.0f));
    em.ages.assign(cap, 0.0f);
    em.lifetimes.assign(cap, 0.0f);
    em.alive.assign(cap, 0u);
    em.aliveCount = 0;
}

// Hito 37 C: muestrea un offset del centro segun el shape del emisor.
// Devuelve (0,0,0) para Point. Box: cubo +/- size en cada eje. Sphere:
// rejection sampling sobre una esfera unitaria escalada por size. Disc:
// punto random en plano XZ con radio <= size.
glm::vec3 sampleEmissionOffset(const ParticleEmitterComponent& em, u64& s) {
    using ES = ParticleEmitterComponent::EmissionShape;
    switch (em.emissionShape) {
        case ES::Point: return glm::vec3(0.0f);
        case ES::Box: {
            const f32 sz = em.emissionShapeSize;
            return glm::vec3(
                (randUnit(s) * 2.0f - 1.0f) * sz,
                (randUnit(s) * 2.0f - 1.0f) * sz,
                (randUnit(s) * 2.0f - 1.0f) * sz);
        }
        case ES::Sphere: {
            // Rejection sampling: muestrea cubo [-1,1]^3, rechaza si fuera
            // de la esfera. Tipicamente 1-2 iteraciones por particula
            // (volumen esfera/cubo = pi/6 ≈ 0.52).
            for (int it = 0; it < 8; ++it) {
                const glm::vec3 p(
                    randUnit(s) * 2.0f - 1.0f,
                    randUnit(s) * 2.0f - 1.0f,
                    randUnit(s) * 2.0f - 1.0f);
                if (glm::dot(p, p) <= 1.0f) {
                    return p * em.emissionShapeSize;
                }
            }
            // Fallback raro (8 rejects seguidos): devolver un punto en
            // el ecuador.
            return glm::vec3(em.emissionShapeSize, 0.0f, 0.0f);
        }
        case ES::Disc: {
            // Rejection sampling en cuadrado XZ.
            for (int it = 0; it < 8; ++it) {
                const f32 x = randUnit(s) * 2.0f - 1.0f;
                const f32 z = randUnit(s) * 2.0f - 1.0f;
                if (x * x + z * z <= 1.0f) {
                    return glm::vec3(x, 0.0f, z) * em.emissionShapeSize;
                }
            }
            return glm::vec3(em.emissionShapeSize, 0.0f, 0.0f);
        }
        case ES::Cone: {
            // Hito 39 A. Cono con axis +Y, altura == size, base radio
            // == size en Y=0 (tip en Y=size). Para distribucion uniforme
            // sobre el volumen: muestrear h ~ size * cbrt(rand) (ese
            // sesgo compensa el menor area cerca del tip), y radio
            // permitido en esa altura es `size * (1 - h/size)`. Disc
            // sampling estandar adentro de ese radio.
            const f32 size = em.emissionShapeSize;
            const f32 u01 = randUnit(s);
            const f32 hNorm = std::cbrt(u01);          // [0..1] hacia el tip
            const f32 h = hNorm * size;
            const f32 maxR = size * (1.0f - hNorm);
            // Disc sampling para el (x, z) en plano horizontal.
            for (int it = 0; it < 8; ++it) {
                const f32 x = randUnit(s) * 2.0f - 1.0f;
                const f32 z = randUnit(s) * 2.0f - 1.0f;
                if (x * x + z * z <= 1.0f) {
                    return glm::vec3(x * maxR, h, z * maxR);
                }
            }
            return glm::vec3(maxR, h, 0.0f);
        }
    }
    return glm::vec3(0.0f);
}

// Spawna una particula en el primer slot libre. Devuelve true si ubico
// hueco (false si la pool esta llena, descarta la solicitud).
// `originPos` es el centro del shape (world o local segun localSpace);
// el offset por shape se calcula y suma adentro.
bool spawnOne(ParticleEmitterComponent& em, const glm::vec3& originPos) {
    const u32 cap = static_cast<u32>(em.alive.size());
    for (u32 i = 0; i < cap; ++i) {
        if (em.alive[i] != 0) continue;
        em.positions[i]  = originPos + sampleEmissionOffset(em, em.rngState);
        em.velocities[i] = randVec3(em.rngState, em.velocityMin, em.velocityMax);
        em.ages[i]       = 0.0f;
        em.lifetimes[i]  = em.lifetimeMin
            + randUnit(em.rngState) * (em.lifetimeMax - em.lifetimeMin);
        em.alive[i]      = 1;
        ++em.aliveCount;
        return true;
    }
    return false;
}

} // namespace

void ParticleSystem::update(Scene& scene, f32 dt) {
    if (dt <= 0.0f) return;
    scene.forEach<TransformComponent, ParticleEmitterComponent>(
        [dt](Entity, TransformComponent& tf, ParticleEmitterComponent& em) {
        ensurePoolSized(em, em.maxParticles);

        // 1) Avanzar particulas vivas + matar las que cumplen lifetime.
        const u32 cap = static_cast<u32>(em.alive.size());
        const f32 dvy = k_gravity * em.gravityFactor * dt;
        for (u32 i = 0; i < cap; ++i) {
            if (em.alive[i] == 0) continue;
            em.ages[i] += dt;
            if (em.ages[i] >= em.lifetimes[i]) {
                em.alive[i] = 0;
                if (em.aliveCount > 0) --em.aliveCount;
                continue;
            }
            em.velocities[i].y += dvy;
            em.positions[i]    += em.velocities[i] * dt;
        }

        // 2) Spawnear nuevas si el emisor esta activo.
        if (em.emitting && em.emitRate > 0.0f && cap > 0) {
            em.emitAccumulator += em.emitRate * dt;
            // Cap fraccional: integer frames. El residuo queda para el
            // proximo frame.
            const i32 toSpawn = static_cast<i32>(std::floor(em.emitAccumulator));
            em.emitAccumulator -= static_cast<f32>(toSpawn);
            // Hito 31 F: localSpace=true => spawn en (0,0,0) local; el
            // renderer multiplica por el worldMatrix del emisor. Asi las
            // particulas siguen al emisor cuando este se mueve.
            const glm::vec3 spawnOrigin = em.localSpace
                ? glm::vec3(0.0f)
                : tf.position;
            for (i32 k = 0; k < toSpawn; ++k) {
                if (!spawnOne(em, spawnOrigin)) {
                    // Pool llena — descartamos sin warning (es esperado
                    // cuando rate*lifetime > maxParticles).
                    em.emitAccumulator = 0.0f;
                    break;
                }
            }
        }
    });
}

} // namespace Mood
