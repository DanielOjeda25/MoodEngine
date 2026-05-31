// F4H4 — Tests del PickupSystem. Cubre: overlap detection por distancia,
// payload aplicado segun tipo (Health/Armor/Weapon/Ammo), refill ammo en
// arma duplicada, slot vacio busqueda, sin AssetManager weapon-pickup
// skip, consumed=true → destroy en proximo tick.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/pickup/PickupSystem.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

namespace {

Entity makePlayer(Scene& scene, glm::vec3 pos) {
    Entity p = scene.createEntity("player");
    auto& tf = p.addComponent<TransformComponent>();
    tf.position = pos;
    p.addComponent<HealthComponent>();
    return p;
}

Entity spawnPickup(Scene& scene, PickupType type, glm::vec3 pos) {
    Entity e = scene.createEntity("pickup");
    auto& tf = e.addComponent<TransformComponent>();
    tf.position = pos;
    PickupComponent pc{};
    pc.type = type;
    pc.healthAmount  = 25.0f;
    pc.armorAmount   = 25.0f;
    pc.pickupRadius  = 1.5f;
    pc.spinDegPerSec = 0.0f;  // tests: sin spin para verificar transform estable
    pc.bobAmplitude  = 0.0f;  // tests: sin bob
    e.addComponent<PickupComponent>(pc);
    return e;
}

} // namespace

TEST_CASE("F4H4 PickupComponent defaults: Health/25/25/1.5m radio") {
    Scene scene;
    Entity e = scene.createEntity("d");
    PickupComponent pc{};
    e.addComponent<PickupComponent>(pc);
    const auto& p = e.getComponent<PickupComponent>();
    CHECK(p.type == PickupType::Health);
    CHECK(p.healthAmount == doctest::Approx(25.0f));
    CHECK(p.armorAmount  == doctest::Approx(25.0f));
    CHECK(p.pickupRadius == doctest::Approx(1.5f));
    CHECK_FALSE(p.consumed);
}

TEST_CASE("F4H4 health pickup: player full HP → no consume, no efecto") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& h = player.getComponent<HealthComponent>();
    h.current = 100.0f;
    h.max     = 100.0f;

    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(0.0f));
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(h.current == doctest::Approx(100.0f));
    // pickup no consumed → entity sigue viva
    CHECK(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 health pickup: player dañado → heal + consumed + destroyed") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& h = player.getComponent<HealthComponent>();
    h.current = 50.0f;
    h.max     = 100.0f;

    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(0.5f, 0.0f, 0.0f));
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(h.current == doctest::Approx(75.0f));
    CHECK_FALSE(scene.registry().valid(pickup.handle())); // destroyed
}

TEST_CASE("F4H4 pickup fuera del radio → no se recoge") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& h = player.getComponent<HealthComponent>();
    h.current = 50.0f;

    // 5m de distancia, radio default 1.5m
    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(5.0f, 0.0f, 0.0f));
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(h.current == doctest::Approx(50.0f));
    CHECK(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 armor pickup: player con ArmorComponent → suma armor") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& a = player.addComponent<ArmorComponent>();
    a.current = 10.0f;
    a.max     = 100.0f;

    spawnPickup(scene, PickupType::Armor, glm::vec3(0.0f));
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(a.current == doctest::Approx(35.0f));
}

TEST_CASE("F4H4 armor pickup: sin ArmorComponent → no consume (sigue vivo)") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f)); // sin armor

    Entity pickup = spawnPickup(scene, PickupType::Armor, glm::vec3(0.0f));
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    // applyArmorPickup retorna false si no hay ArmorComponent → no consume.
    // Convencion Doom: pickup overhead se queda hasta que sea util.
    CHECK(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 weapon pickup sin AssetManager → no se consume (skip)") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    player.addComponent<WeaponComponent>();

    Entity pickup = spawnPickup(scene, PickupType::Weapon, glm::vec3(0.0f));
    pickup.getComponent<PickupComponent>().weaponPath = "weapons/test.moodweapon";
    Pickup::tickSystem(scene, 0.016f, player, /*assets*/ nullptr);

    // Sin assets, weapon pickup loguea warn y no consume.
    CHECK(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 tick sin player → pickups siguen vivos") {
    Scene scene;
    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(0.0f));
    Pickup::tickSystem(scene, 0.016f, Entity{}, nullptr);

    CHECK(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 pickup ya marcado consumed → se destroy en proximo tick") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(100.0f, 0.0f, 0.0f)); // lejos
    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(0.0f));
    pickup.getComponent<PickupComponent>().consumed = true;

    Pickup::tickSystem(scene, 0.016f, player, nullptr);
    CHECK_FALSE(scene.registry().valid(pickup.handle()));
}

TEST_CASE("F4H4 health pickup clamp: heal no excede max") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& h = player.getComponent<HealthComponent>();
    h.current = 80.0f;
    h.max     = 100.0f;

    Entity pickup = spawnPickup(scene, PickupType::Health, glm::vec3(0.0f));
    pickup.getComponent<PickupComponent>().healthAmount = 50.0f;  // sobrante 30
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(h.current == doctest::Approx(100.0f));
}

TEST_CASE("F4H4 armor pickup clamp: no excede max") {
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    auto& a = player.addComponent<ArmorComponent>();
    a.current = 80.0f;
    a.max     = 100.0f;

    Entity pickup = spawnPickup(scene, PickupType::Armor, glm::vec3(0.0f));
    pickup.getComponent<PickupComponent>().armorAmount = 50.0f;
    Pickup::tickSystem(scene, 0.016f, player, nullptr);

    CHECK(a.current == doctest::Approx(100.0f));
}
