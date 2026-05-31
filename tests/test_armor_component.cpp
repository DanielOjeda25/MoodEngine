// F4H4 — Tests de ArmorComponent + integracion con Health::applyDamage.
//
// Cubre: defaults, applyDamage consume armor primero (segun absorbRatio),
// armor=0 todo va al HP, sin ArmorComponent flujo legacy F4H1 intacto,
// damage que excede la armor mata al player tras agotarla, heal/armor no
// interactuan (heal de health no toca armor).

#include <doctest/doctest.h>

#include "engine/gameplay/Health.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

namespace {

Entity makePlayerWithHpAndArmor(Scene& scene, f32 hp, f32 armor,
                                 f32 absorbRatio = 0.66f) {
    Entity e = scene.createEntity("player_armor_test");
    auto& h = e.addComponent<HealthComponent>();
    h.current = hp;
    h.max     = hp;
    auto& a = e.addComponent<ArmorComponent>();
    a.current     = armor;
    a.max         = armor < 100.0f ? 100.0f : armor;
    a.absorbRatio = absorbRatio;
    return e;
}

} // namespace

TEST_CASE("F4H4 ArmorComponent defaults: 0/100, absorbRatio=0.66") {
    Scene scene;
    Entity e = scene.createEntity("d");
    auto& a = e.addComponent<ArmorComponent>();
    CHECK(a.current     == doctest::Approx(0.0f));
    CHECK(a.max         == doctest::Approx(100.0f));
    CHECK(a.absorbRatio == doctest::Approx(0.66f));
}

TEST_CASE("F4H4 applyDamage: sin ArmorComponent flujo F4H1 intacto") {
    Scene scene;
    Entity e = scene.createEntity("dummy");
    auto& h = e.addComponent<HealthComponent>();
    h.current = 100.0f;
    h.max     = 100.0f;

    Health::applyDamage(scene, e, 25.0f);
    CHECK(h.current == doctest::Approx(75.0f));
}

TEST_CASE("F4H4 applyDamage: armor=0 todo el damage va al HP") {
    Scene scene;
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 0.0f);

    Health::applyDamage(scene, e, 30.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(h.current == doctest::Approx(70.0f));
    CHECK(a.current == doctest::Approx(0.0f));
}

TEST_CASE("F4H4 applyDamage: armor consume el 66% del dmg, resto al HP") {
    Scene scene;
    // hp=100 / armor=100 / absorbRatio=0.66
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 100.0f, 0.66f);

    // 30 dmg → armor absorbe 30 * 0.66 = 19.8 → HP recibe 10.2
    Health::applyDamage(scene, e, 30.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(a.current == doctest::Approx(100.0f - 19.8f));
    CHECK(h.current == doctest::Approx(100.0f - 10.2f));
}

TEST_CASE("F4H4 applyDamage: armor=1 con dmg grande → armor se agota, sobra al HP") {
    Scene scene;
    // armor=1, absorbRatio=0.66
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 1.0f, 0.66f);

    // 30 dmg → armor wanted=19.8 pero solo hay 1 → absorbed=1
    //          → HP recibe 30 - 1 = 29
    Health::applyDamage(scene, e, 30.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(a.current == doctest::Approx(0.0f));
    CHECK(h.current == doctest::Approx(100.0f - 29.0f));
}

TEST_CASE("F4H4 applyDamage: absorbRatio=1.0 armor come 100% hasta agotarse") {
    Scene scene;
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 50.0f, 1.0f);

    // 40 dmg → armor wanted=40, hay 50 → absorbed=40 → HP intacto
    Health::applyDamage(scene, e, 40.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(a.current == doctest::Approx(10.0f));
    CHECK(h.current == doctest::Approx(100.0f));
}

TEST_CASE("F4H4 applyDamage: absorbRatio=0.0 armor inerte, todo va al HP") {
    Scene scene;
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 100.0f, 0.0f);

    Health::applyDamage(scene, e, 25.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(a.current == doctest::Approx(100.0f));   // intacto
    CHECK(h.current == doctest::Approx(75.0f));
}

TEST_CASE("F4H4 applyDamage: dmg masivo agota armor + mata al player") {
    Scene scene;
    Entity e = makePlayerWithHpAndArmor(scene, 50.0f, 20.0f, 0.66f);

    // 200 dmg → armor wanted=132 pero solo hay 20 → absorbed=20
    //         → HP recibe 200 - 20 = 180 → muere
    Health::applyDamage(scene, e, 200.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(a.current == doctest::Approx(0.0f));
    CHECK(h.current == doctest::Approx(0.0f));
    CHECK(h.dead);
}

TEST_CASE("F4H4 ArmorComponent: absorbRatio clamped al usar (no muta el campo)") {
    // El clamp es defensivo dentro de applyDamage; el campo se respeta
    // tal cual lo dejo el editor/serializer.
    Scene scene;
    Entity e = makePlayerWithHpAndArmor(scene, 100.0f, 100.0f, 2.5f /* invalido */);

    // Clamp interno a 1.0 → 30 dmg * 1.0 = 30 al armor, 0 al HP
    Health::applyDamage(scene, e, 30.0f);
    auto& h = e.getComponent<HealthComponent>();
    auto& a = e.getComponent<ArmorComponent>();
    CHECK(h.current == doctest::Approx(100.0f));
    CHECK(a.current == doctest::Approx(70.0f));
    CHECK(a.absorbRatio == doctest::Approx(2.5f)); // campo NO mutado
}
