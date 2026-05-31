// F4H5 — Tests del ProjectileSystem + WeaponSpec.projectile.
//
// Cubre:
// - WeaponSpec.projectile roundtrip + clamps de sanidad.
// - ProjectileSystem::applySplashDamage falloff lineal (centro=100%, borde=0%).
// - ignoreOwner skipea entity del shooter (rocket-jump opt-out).
// - tickSystem mueve proyectiles + lifetime expire → explode.
// - sin PhysicsWorld (tests headless) proyectiles vuelan hasta expirar.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/projectile/ProjectileSystem.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <nlohmann/json.hpp>

using namespace Mood;

namespace {

Entity makeHealthEntity(Scene& scene, glm::vec3 pos, f32 hp = 100.0f) {
    Entity e = scene.createEntity("victim");
    auto& tf = e.addComponent<TransformComponent>();
    tf.position = pos;
    auto& h = e.addComponent<HealthComponent>();
    h.current = hp;
    h.max     = hp;
    return e;
}

} // namespace

// =============================================================
// WeaponSpec.projectile schema
// =============================================================

TEST_CASE("F4H5 WeaponSpec.projectile defaults razonables") {
    Weapon::Spec s;
    CHECK(s.projectile.speed        == doctest::Approx(20.0f));
    CHECK(s.projectile.gravity      == doctest::Approx(0.0f));
    CHECK(s.projectile.bounceCount  == 0);
    CHECK(s.projectile.bounceFactor == doctest::Approx(0.6f));
    CHECK(s.projectile.lifetimeSec  == doctest::Approx(5.0f));
    CHECK(s.projectile.directDamage == doctest::Approx(30.0f));
    CHECK(s.projectile.splashRadius == doctest::Approx(2.0f));
    CHECK(s.projectile.splashDamage == doctest::Approx(30.0f));
}

TEST_CASE("F4H5 WeaponSpec.projectile JSON roundtrip") {
    Weapon::Spec s;
    s.displayName = "Rocket";
    s.category    = "projectile";
    s.projectile.speed        = 25.0f;
    s.projectile.gravity      = 0.0f;
    s.projectile.bounceCount  = 0;
    s.projectile.lifetimeSec  = 4.0f;
    s.projectile.directDamage = 80.0f;
    s.projectile.splashRadius = 4.5f;
    s.projectile.splashDamage = 60.0f;

    const auto j = s.toJson();
    REQUIRE(j.contains("projectile"));
    CHECK(j["projectile"]["speed"]        == doctest::Approx(25.0f));
    CHECK(j["projectile"]["splashRadius"] == doctest::Approx(4.5f));

    Weapon::Spec parsed = Weapon::Spec::fromJson(j);
    CHECK(parsed.category                  == "projectile");
    CHECK(parsed.projectile.speed          == doctest::Approx(25.0f));
    CHECK(parsed.projectile.splashRadius   == doctest::Approx(4.5f));
    CHECK(parsed.projectile.splashDamage   == doctest::Approx(60.0f));
}

TEST_CASE("F4H5 WeaponSpec.projectile sin bloque -> defaults") {
    nlohmann::json j;
    j["_version"]    = Weapon::Spec::k_schemaVersion;
    j["displayName"] = "Shotgun";
    j["category"]    = "hitscan";
    // sin "projectile" en el JSON
    Weapon::Spec parsed = Weapon::Spec::fromJson(j);
    CHECK(parsed.projectile.speed        == doctest::Approx(20.0f));
    CHECK(parsed.projectile.splashRadius == doctest::Approx(2.0f));
}

TEST_CASE("F4H5 WeaponSpec.projectile clamps") {
    nlohmann::json j;
    j["_version"] = Weapon::Spec::k_schemaVersion;
    j["projectile"] = {
        {"speed",        -10.0f},  // negativo → 0
        {"gravity",      -2.0f},   // negativo → 0
        {"bounceCount",  -5},      // negativo → 0
        {"bounceFactor", 2.5f},    // >1 → 1
        {"lifetimeSec",  0.0f},    // muy chico → 0.05
        {"directDamage", -50.0f},  // negativo → 0
        {"splashRadius", -3.0f},   // negativo → 0
        {"splashDamage", -10.0f},  // negativo → 0
    };
    Weapon::Spec s = Weapon::Spec::fromJson(j);
    CHECK(s.projectile.speed        == doctest::Approx(0.0f));
    CHECK(s.projectile.gravity      == doctest::Approx(0.0f));
    CHECK(s.projectile.bounceCount  == 0);
    CHECK(s.projectile.bounceFactor == doctest::Approx(1.0f));
    CHECK(s.projectile.lifetimeSec  == doctest::Approx(0.05f));
    CHECK(s.projectile.directDamage == doctest::Approx(0.0f));
    CHECK(s.projectile.splashRadius == doctest::Approx(0.0f));
    CHECK(s.projectile.splashDamage == doctest::Approx(0.0f));
}

TEST_CASE("F4H5 WeaponSpec.projectile speed cap a 200 m/s") {
    nlohmann::json j;
    j["_version"]   = Weapon::Spec::k_schemaVersion;
    j["projectile"] = {{"speed", 500.0f}};
    Weapon::Spec s = Weapon::Spec::fromJson(j);
    CHECK(s.projectile.speed == doctest::Approx(200.0f));
}

// =============================================================
// applySplashDamage falloff lineal
// =============================================================

TEST_CASE("F4H5 splash damage en el centro = baseDamage (falloff = 1.0)") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(0.0f), 100.0f);

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), /*radius=*/5.0f,
                                    /*baseDamage=*/60.0f,
                                    /*ignoreOwner=*/0xFFFFFFFFu);
    const auto& h = v.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(40.0f));  // 100 - 60
}

TEST_CASE("F4H5 splash damage en el borde = 0 (falloff = 0)") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(5.0f, 0.0f, 0.0f), 100.0f);

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 60.0f,
                                    0xFFFFFFFFu);
    const auto& h = v.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(100.0f));  // sin damage
}

TEST_CASE("F4H5 splash damage a media distancia = 50%") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(2.5f, 0.0f, 0.0f), 100.0f);

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 60.0f,
                                    0xFFFFFFFFu);
    const auto& h = v.getComponent<HealthComponent>();
    // 60 * (1 - 2.5/5) = 60 * 0.5 = 30
    CHECK(h.current == doctest::Approx(70.0f));
}

TEST_CASE("F4H5 splash damage fuera del radio = no efecto") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(10.0f, 0.0f, 0.0f), 100.0f);

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 60.0f,
                                    0xFFFFFFFFu);
    const auto& h = v.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(100.0f));
}

TEST_CASE("F4H5 splash damage ignoreOwner skipea al shooter") {
    Scene scene;
    Entity shooter = makeHealthEntity(scene, glm::vec3(0.0f), 100.0f);

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 60.0f,
                                    static_cast<u32>(shooter.handle()));
    const auto& h = shooter.getComponent<HealthComponent>();
    CHECK(h.current == doctest::Approx(100.0f));  // no se daño a si mismo
}

TEST_CASE("F4H5 splash damage radius=0 -> no-op") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(0.0f), 100.0f);
    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 0.0f, 60.0f,
                                    0xFFFFFFFFu);
    CHECK(v.getComponent<HealthComponent>().current == doctest::Approx(100.0f));
}

TEST_CASE("F4H5 splash damage baseDamage=0 -> no-op") {
    Scene scene;
    Entity v = makeHealthEntity(scene, glm::vec3(0.0f), 100.0f);
    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 0.0f,
                                    0xFFFFFFFFu);
    CHECK(v.getComponent<HealthComponent>().current == doctest::Approx(100.0f));
}

TEST_CASE("F4H5 splash damage afecta multiples entities en el radio") {
    Scene scene;
    Entity a = makeHealthEntity(scene, glm::vec3(0.0f, 0.0f, 0.0f),  100.0f);
    Entity b = makeHealthEntity(scene, glm::vec3(2.0f, 0.0f, 0.0f),  100.0f);
    Entity c = makeHealthEntity(scene, glm::vec3(10.0f, 0.0f, 0.0f), 100.0f); // afuera

    Projectile::applySplashDamage(scene, glm::vec3(0.0f), 5.0f, 60.0f,
                                    0xFFFFFFFFu);
    CHECK(a.getComponent<HealthComponent>().current == doctest::Approx(40.0f));
    // b: dist=2, falloff = 1 - 2/5 = 0.6 → 60 * 0.6 = 36
    CHECK(b.getComponent<HealthComponent>().current == doctest::Approx(64.0f));
    CHECK(c.getComponent<HealthComponent>().current == doctest::Approx(100.0f));
}
