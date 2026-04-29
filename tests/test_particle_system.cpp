// Tests del ParticleSystem (Hito 29 Bloque 1). Cubren la simulacion CPU
// pura: tasa de emision, lifetime, gravedad, reciclaje de slots,
// pool full, RNG deterministico.

#include <doctest/doctest.h>

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/ParticleSystem.h"

#include <glm/vec3.hpp>

using namespace Mood;

namespace {

// Helper: cuenta particulas vivas a mano (sin confiar en aliveCount).
u32 countAlive(const ParticleEmitterComponent& em) {
    u32 c = 0;
    for (u8 a : em.alive) if (a != 0) ++c;
    return c;
}

} // namespace

TEST_CASE("ParticleSystem: tasa de emision spawnea cantidad esperada") {
    Scene scene;
    Entity e = scene.createEntity("Emisor");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 10.0f;
    em.maxParticles = 32;
    em.lifetimeMin = em.lifetimeMax = 5.0f;

    ParticleSystem sys;
    // 0.5s a 10/s = 5 particulas. Hacemos un solo update con dt=0.5
    // para evitar acumulacion de error en sub-frames.
    sys.update(scene, 0.5f);
    CHECK(countAlive(em) == 5u);
    CHECK(em.aliveCount == 5u);
}

TEST_CASE("ParticleSystem: lifetime expira la particula") {
    Scene scene;
    Entity e = scene.createEntity("ShortLived");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 4.0f;     // 1 particula en 0.25s
    em.maxParticles = 8;
    em.lifetimeMin = em.lifetimeMax = 1.0f;

    ParticleSystem sys;
    sys.update(scene, 0.25f);          // spawnea 1
    REQUIRE(countAlive(em) == 1u);
    em.emitting = false;               // pausamos para no contaminar
    sys.update(scene, 1.0f);           // expira la particula (age=1.0 == lifetime)
    CHECK(countAlive(em) == 0u);
    CHECK(em.aliveCount == 0u);
}

TEST_CASE("ParticleSystem: gravityFactor=1 aplica -9.81 m/s^2 en Y") {
    Scene scene;
    Entity e = scene.createEntity("Gravedad");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate     = 0.0f;            // no emitir auto, controlamos manualmente
    em.maxParticles = 4;
    em.lifetimeMin = em.lifetimeMax = 10.0f;
    em.gravityFactor = 1.0f;
    em.velocityMin = em.velocityMax = glm::vec3(0.0f, 5.0f, 0.0f);

    ParticleSystem sys;
    sys.update(scene, 0.0001f);  // ensurePoolSized + emit attempt (rate=0 => 0)

    // Inyectamos una particula manualmente al slot 0.
    em.alive[0]      = 1;
    em.positions[0]  = glm::vec3(0.0f);
    em.velocities[0] = glm::vec3(0.0f, 5.0f, 0.0f);
    em.ages[0]       = 0.0f;
    em.lifetimes[0]  = 10.0f;
    em.aliveCount    = 1;

    // dt=1: vy pasa de 5 a 5 + (-9.81*1) = -4.81. pos.y = 0 + (-4.81)*1 = -4.81.
    // (Nota: el sistema actualiza vel ANTES de pos, asi que pos usa la nueva vel.)
    sys.update(scene, 1.0f);
    CHECK(em.velocities[0].y == doctest::Approx(-4.81f).epsilon(0.01));
    CHECK(em.positions[0].y == doctest::Approx(-4.81f).epsilon(0.01));
}

TEST_CASE("ParticleSystem: pool full descarta sin crashear") {
    Scene scene;
    Entity e = scene.createEntity("PoolFull");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 100.0f;
    em.maxParticles = 4;          // intencionalmente chica
    em.lifetimeMin = em.lifetimeMax = 5.0f;

    ParticleSystem sys;
    sys.update(scene, 1.0f);      // intentaria spawnear 100 — solo entran 4
    CHECK(countAlive(em) == 4u);
    CHECK(em.aliveCount == 4u);
}

TEST_CASE("ParticleSystem: emitting=false pausa spawn pero sigue avanzando vivas") {
    Scene scene;
    Entity e = scene.createEntity("Pausable");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 10.0f;
    em.maxParticles = 16;
    em.lifetimeMin = em.lifetimeMax = 10.0f;

    ParticleSystem sys;
    sys.update(scene, 0.5f);     // spawnea 5
    REQUIRE(countAlive(em) == 5u);
    em.emitting = false;
    sys.update(scene, 1.0f);     // no spawn, las 5 siguen
    CHECK(countAlive(em) == 5u);
}

TEST_CASE("ParticleSystem: spawn position = TransformComponent.position") {
    Scene scene;
    Entity e = scene.createEntity("Localizado");
    e.getComponent<TransformComponent>().position = glm::vec3(5.0f, 6.0f, 7.0f);
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate     = 1.0f;
    em.maxParticles = 4;
    em.lifetimeMin = em.lifetimeMax = 10.0f;
    em.velocityMin = em.velocityMax = glm::vec3(0.0f); // sin velocidad

    ParticleSystem sys;
    sys.update(scene, 1.0f);    // spawnea 1
    REQUIRE(em.aliveCount >= 1u);
    // Buscamos la primera viva.
    for (u32 i = 0; i < em.alive.size(); ++i) {
        if (em.alive[i] == 0) continue;
        // pos = spawnPos + vel*dt = (5,6,7) + 0 = (5,6,7). Pero el update
        // YA aplico vel*dt (que es 0 aqui) y gravedad (0 aqui).
        CHECK(em.positions[i].x == doctest::Approx(5.0f));
        CHECK(em.positions[i].y == doctest::Approx(6.0f));
        CHECK(em.positions[i].z == doctest::Approx(7.0f));
        break;
    }
}

TEST_CASE("ParticleSystem: pool size matchea maxParticles tras primer update") {
    Scene scene;
    Entity e = scene.createEntity("Resize");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 0.0f;
    em.maxParticles = 17;
    em.lifetimeMin = em.lifetimeMax = 1.0f;

    CHECK(em.positions.empty());
    ParticleSystem sys;
    sys.update(scene, 0.0001f);
    CHECK(em.positions.size() == 17u);
    CHECK(em.velocities.size() == 17u);
    CHECK(em.alive.size() == 17u);
}

TEST_CASE("ParticleSystem: dt<=0 es no-op") {
    Scene scene;
    Entity e = scene.createEntity("NoOp");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 100.0f;
    em.maxParticles = 16;

    ParticleSystem sys;
    sys.update(scene, 0.0f);
    sys.update(scene, -1.0f);
    CHECK(em.aliveCount == 0u);
    // Tampoco se inicializa la pool — la lazy-alloc espera dt>0.
    CHECK(em.positions.empty());
}

TEST_CASE("ParticleSystem: emisor recicla slots de particulas muertas") {
    Scene scene;
    Entity e = scene.createEntity("Reciclador");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 5.0f;
    em.maxParticles = 4;        // se llena rapido
    em.lifetimeMin = em.lifetimeMax = 0.5f;

    ParticleSystem sys;
    // 1s @ 5/s = 5 spawn requests. Pool=4 -> primera tanda llena. Luego
    // dt=1.0s mata las 4 (lifetime=0.5s). El frame siguiente vuelve a
    // emitir hasta llenar.
    sys.update(scene, 0.8f);
    const u32 cnt1 = countAlive(em);
    CHECK(cnt1 == 4u);
    sys.update(scene, 1.0f);   // expira las primeras + spawnea hasta lim
    CHECK(countAlive(em) == 4u);
}
