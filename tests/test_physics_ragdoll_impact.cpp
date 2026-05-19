// F2H68 Bloque D: tests del infra de auto-ragdoll por impacto.
// Cubre:
//   - registerBodyEntity / unregisterBodyEntity / entityOfBody (roundtrip).
//   - destroyBody auto-limpia el mapeo body->entity.
//   - ContactListener encola eventos cuando 2 bodies Dynamic chocan a una
//     velocidad relativa >= impactSpeedThreshold.
//   - Velocidad bajo threshold NO encola.
//   - setRagdollImpactFactor escala la magnitud del impulse encolado.
//
// NO se testea el flow completo Scene+RagdollComponent aca — eso requiere
// mock de Animator + Skeleton + Mesh, demasiado para un test unitario. El
// drain logic del RagdollSystem queda validado por el sample manual en
// `vehicle_demo.moodmap` (NPC con ragdoll Mixamo + auto a 10 m/s).

#include <doctest/doctest.h>

#include "engine/physics/world/PhysicsWorld.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>

using namespace Mood;

namespace {

constexpr f32 k_dt = 0.016f;

// Crea un body Dynamic box (mass=10kg por default) en `pos` con velocidad
// linear inicial `vel`. Helper compartido por varios tests.
u32 spawnDynamicBox(PhysicsWorld& pw, const glm::vec3& pos,
                     const glm::vec3& vel, f32 mass = 10.0f) {
    const u32 id = pw.createBody(pos, CollisionShape::Box,
                                   glm::vec3(0.5f, 0.5f, 0.5f),
                                   BodyType::Dynamic, mass, 0.5f);
    if (id != 0) pw.setBodyLinearVelocity(id, vel);
    return id;
}

} // anonymous

TEST_CASE("F2H68: registerBodyEntity + entityOfBody roundtrip") {
    PhysicsWorld pw;
    const u32 body = pw.createBody(glm::vec3(0.0f), CollisionShape::Box,
                                     glm::vec3(0.5f), BodyType::Dynamic, 10.0f);
    REQUIRE(body != 0u);
    CHECK(pw.entityOfBody(body) == 0u);  // no registrado todavia

    pw.registerBodyEntity(body, 42u);
    CHECK(pw.entityOfBody(body) == 42u);

    // Re-register sobrescribe.
    pw.registerBodyEntity(body, 99u);
    CHECK(pw.entityOfBody(body) == 99u);

    pw.unregisterBodyEntity(body);
    CHECK(pw.entityOfBody(body) == 0u);

    pw.destroyBody(body);
}

TEST_CASE("F2H68: destroyBody auto-limpia el mapeo body->entity") {
    PhysicsWorld pw;
    const u32 body = pw.createBody(glm::vec3(0.0f), CollisionShape::Box,
                                     glm::vec3(0.5f), BodyType::Dynamic, 10.0f);
    REQUIRE(body != 0u);
    pw.registerBodyEntity(body, 7u);
    REQUIRE(pw.entityOfBody(body) == 7u);

    pw.destroyBody(body);
    CHECK(pw.entityOfBody(body) == 0u);
}

TEST_CASE("F2H68: ContactListener encola evento al chocar bodies Dynamic") {
    PhysicsWorld pw;
    // Threshold default = 4 m/s. Body A se mueve hacia +X a 10 m/s; body B
    // esta detenido en el camino. Despues de unos steps Jolt detecta el
    // contacto.
    const u32 a = spawnDynamicBox(pw, glm::vec3(0.0f, 5.0f, 0.0f),
                                    glm::vec3(10.0f, 0.0f, 0.0f));
    const u32 b = spawnDynamicBox(pw, glm::vec3(1.5f, 5.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 0.0f));
    REQUIRE(a != 0u);
    REQUIRE(b != 0u);
    pw.registerBodyEntity(a, 100u);
    pw.registerBodyEntity(b, 200u);

    // Simular ~10 frames para que el contacto suceda.
    for (int i = 0; i < 10; ++i) pw.step(k_dt);

    auto events = pw.drainImpactEvents();
    REQUIRE(events.size() >= 1u);
    // Algun evento debe corresponder al body B (la "victima" parada que
    // recibe el golpe). El test acepta que tambien aparezca el body A.
    bool sawB = false;
    for (const auto& ev : events) {
        if (ev.victimBodyId == b) {
            sawB = true;
            CHECK(ev.impactSpeed >= 4.0f);  // arriba del threshold default
            CHECK(glm::length(ev.impulseWorld) > 0.0f);
        }
    }
    CHECK(sawB);

    // El drain es destructivo: una segunda llamada inmediatamente despues
    // (sin otro step) debe devolver vacio.
    auto events2 = pw.drainImpactEvents();
    CHECK(events2.empty());
}

TEST_CASE("F2H68: ContactListener NO encola si velocidad < threshold") {
    PhysicsWorld pw;
    pw.setRagdollImpactSpeedThreshold(4.0f);  // explicito
    // Velocidad 1 m/s — muy lenta para impacto ragdoll-able.
    const u32 a = spawnDynamicBox(pw, glm::vec3(0.0f, 5.0f, 0.0f),
                                    glm::vec3(1.0f, 0.0f, 0.0f));
    const u32 b = spawnDynamicBox(pw, glm::vec3(1.5f, 5.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 0.0f));
    REQUIRE(a != 0u);
    REQUIRE(b != 0u);

    for (int i = 0; i < 30; ++i) pw.step(k_dt);

    auto events = pw.drainImpactEvents();
    // A 1 m/s, aunque haya contacto, no debe pasar el threshold. Por
    // defensivo aceptamos 0 — si emerge algun ruido > 4 m/s falla el test.
    CHECK(events.empty());
}

TEST_CASE("F2H68: setRagdollImpactFactor escala la magnitud del impulse") {
    // Test runtime: mismo impacto con dos factores distintos -> magnitudes
    // distintas y proporcionales.
    auto magnitudeWithFactor = [](f32 factor) -> f32 {
        PhysicsWorld pw;
        pw.setRagdollImpactFactor(factor);
        const u32 a = spawnDynamicBox(pw, glm::vec3(0.0f, 5.0f, 0.0f),
                                        glm::vec3(10.0f, 0.0f, 0.0f));
        const u32 b = spawnDynamicBox(pw, glm::vec3(1.5f, 5.0f, 0.0f),
                                        glm::vec3(0.0f, 0.0f, 0.0f));
        for (int i = 0; i < 10; ++i) pw.step(k_dt);
        auto events = pw.drainImpactEvents();
        f32 maxMag = 0.0f;
        for (const auto& ev : events) {
            if (ev.victimBodyId == b) {
                maxMag = std::max(maxMag, glm::length(ev.impulseWorld));
            }
        }
        return maxMag;
    };
    const f32 m1 = magnitudeWithFactor(0.3f);
    const f32 m2 = magnitudeWithFactor(0.6f);
    REQUIRE(m1 > 0.0f);
    REQUIRE(m2 > 0.0f);
    // m2 debe ser ~2x m1 (factor escalo lineal). Damos margen 10% para
    // pequenias diferencias por step variability.
    const f32 ratio = m2 / m1;
    CHECK(ratio > 1.8f);
    CHECK(ratio < 2.2f);
}

TEST_CASE("F2H68: ragdollBodyIds devuelve vector vacio para id invalido") {
    PhysicsWorld pw;
    auto ids = pw.ragdollBodyIds(0u);
    CHECK(ids.empty());
    ids = pw.ragdollBodyIds(9999u);  // id no creado
    CHECK(ids.empty());
}
