// F4H1 — Tests del sistema de salud/daño (cimiento del combate Fase 4).
//
// Cubre: defaults, applyDamage normal/clamp/idempotente, heal, transición
// a dead única, tickSystem decae el flash, tickSystem auto-add RigidBody
// Dynamic al morir.

#include <doctest/doctest.h>

#include "engine/gameplay/Health.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

using namespace Mood;

namespace {

Entity makeDummyWithHealth(Scene& scene, f32 max = 100.0f) {
    Entity e = scene.createEntity("dummy_test");
    auto& h = e.addComponent<HealthComponent>();
    h.current = max;
    h.max = max;
    return e;
}

} // namespace

TEST_CASE("F4H1 HealthComponent defaults: 100/100, alive") {
    Scene scene;
    Entity e = scene.createEntity("d");
    auto& h = e.addComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(100.0f));
    CHECK(h.max == doctest::Approx(100.0f));
    CHECK_FALSE(h.dead);
    CHECK(h.hitFlashTimer == doctest::Approx(0.0f));
}

TEST_CASE("F4H1 applyDamage resta vida y triggea flash") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene);

    Health::applyDamage(scene, e, 25.0f);
    const auto& h = e.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(75.0f));
    CHECK_FALSE(h.dead);
    CHECK(h.hitFlashTimer == doctest::Approx(0.08f));  // k_hitFlashSec
}

TEST_CASE("F4H1 applyDamage clamp a 0 + dead=true al cruzar") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene, 50.0f);

    Health::applyDamage(scene, e, 200.0f);  // sobre-daño
    const auto& h = e.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(0.0f));
    CHECK(h.dead);
    CHECK(h.hitFlashTimer == doctest::Approx(0.25f));  // k_deathFlashSec mayor
}

TEST_CASE("F4H1 applyDamage idempotente sobre dead (no resucita ni mata 2x)") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene, 10.0f);

    Health::applyDamage(scene, e, 20.0f);
    CHECK(e.getComponent<HealthComponent>().dead);
    const f32 flashAfterDeath = e.getComponent<HealthComponent>().hitFlashTimer;

    // Segundo damage: no-op silencioso (no resetea flash, no muta current).
    Health::applyDamage(scene, e, 100.0f);
    const auto& h = e.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(0.0f));
    CHECK(h.dead);
    CHECK(h.hitFlashTimer == doctest::Approx(flashAfterDeath));
}

TEST_CASE("F4H1 applyDamage no-op sobre entity sin HealthComponent") {
    Scene scene;
    Entity e = scene.createEntity("sin_health");
    // No debe crashear ni agregar HealthComponent implicito.
    Health::applyDamage(scene, e, 50.0f);
    CHECK_FALSE(e.hasComponent<HealthComponent>());
}

TEST_CASE("F4H1 applyDamage con amount<=0 es no-op") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene);
    Health::applyDamage(scene, e, 0.0f);
    Health::applyDamage(scene, e, -10.0f);
    CHECK(e.getComponent<HealthComponent>().current == doctest::Approx(100.0f));
    CHECK(e.getComponent<HealthComponent>().hitFlashTimer == doctest::Approx(0.0f));
}

TEST_CASE("F4H1 heal suma vida con clamp a max") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene);
    auto& h = e.getComponent<HealthComponent>();
    h.current = 30.0f;

    Health::heal(scene, e, 50.0f);
    CHECK(h.current == doctest::Approx(80.0f));

    Health::heal(scene, e, 9999.0f);
    CHECK(h.current == doctest::Approx(100.0f));  // clamp a max
}

TEST_CASE("F4H1 heal no-op sobre dead (morir es definitivo en F4H1)") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene, 10.0f);
    Health::applyDamage(scene, e, 20.0f);
    REQUIRE(e.getComponent<HealthComponent>().dead);

    Health::heal(scene, e, 100.0f);
    const auto& h = e.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(0.0f));  // no revivió
    CHECK(h.dead);
}

TEST_CASE("F4H1 tickSystem decae hitFlashTimer") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene);
    Health::applyDamage(scene, e, 10.0f);
    REQUIRE(e.getComponent<HealthComponent>().hitFlashTimer == doctest::Approx(0.08f));

    Health::tickSystem(scene, 0.05f);
    CHECK(e.getComponent<HealthComponent>().hitFlashTimer
          == doctest::Approx(0.03f));

    Health::tickSystem(scene, 1.0f);  // sobre-decay
    CHECK(e.getComponent<HealthComponent>().hitFlashTimer
          == doctest::Approx(0.0f));
}

TEST_CASE("F4H1 tickSystem auto-add RigidBody Dynamic al morir") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene, 10.0f);
    REQUIRE_FALSE(e.hasComponent<RigidBodyComponent>());

    Health::applyDamage(scene, e, 20.0f);  // mata
    REQUIRE(e.getComponent<HealthComponent>().dead);
    // Pre-tick: aún no tiene RigidBody.
    CHECK_FALSE(e.hasComponent<RigidBodyComponent>());

    Health::tickSystem(scene, 0.016f);
    // Post-tick: D2 — auto-add Dynamic para que cae con física.
    REQUIRE(e.hasComponent<RigidBodyComponent>());
    const auto& rb = e.getComponent<RigidBodyComponent>();
    CHECK(rb.type == RigidBodyComponent::Type::Dynamic);
    CHECK(rb.shape == RigidBodyComponent::Shape::Box);
    CHECK(rb.mass == doctest::Approx(1.0f));
}

TEST_CASE("F4H1 tickSystem NO duplica RigidBody si ya existe") {
    Scene scene;
    Entity e = makeDummyWithHealth(scene, 10.0f);
    e.addComponent<RigidBodyComponent>(
        RigidBodyComponent::Type::Static,
        RigidBodyComponent::Shape::Sphere,
        glm::vec3(0.3f), 0.0f);
    Health::applyDamage(scene, e, 20.0f);
    Health::tickSystem(scene, 0.016f);
    // Mantiene el Static original — F4H1 no convierte; agendizable.
    const auto& rb = e.getComponent<RigidBodyComponent>();
    CHECK(rb.type == RigidBodyComponent::Type::Static);
    CHECK(rb.shape == RigidBodyComponent::Shape::Sphere);
}
