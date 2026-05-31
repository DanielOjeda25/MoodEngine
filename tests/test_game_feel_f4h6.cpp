// F4H6 — Tests del game feel pass.
//
// Cubre: triggerCameraShake (state lifecycle), triggerPainReaction
// (anti-spam + roll determinismo), applySplashDamage returns count,
// FpsCamera offsets transient (shake + pain pitch/roll), crosshair gap.

#include <doctest/doctest.h>

#include "engine/game/state/GameState.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/projectile/ProjectileSystem.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/FpsCamera.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

// =============================================================
// triggerCameraShake
// =============================================================

TEST_CASE("F4H6 triggerCameraShake setea amp + timers") {
    GameState::reset();
    GameState::triggerCameraShake(0.05f, 0.3f);
    const auto& h = GameState::hud();
    CHECK(h.shake_amp   == doctest::Approx(0.05f));
    CHECK(h.shake_t     == doctest::Approx(0.3f));
    CHECK(h.shake_max_t == doctest::Approx(0.3f));
}

TEST_CASE("F4H6 triggerCameraShake anti-spam: shake activo + nuevo mas debil no pisa") {
    GameState::reset();
    GameState::triggerCameraShake(0.15f, 0.4f);
    GameState::triggerCameraShake(0.02f, 0.08f);
    const auto& h = GameState::hud();
    CHECK(h.shake_amp == doctest::Approx(0.15f));
    CHECK(h.shake_t   == doctest::Approx(0.4f));
}

TEST_CASE("F4H6 triggerCameraShake: nuevo mas fuerte reemplaza") {
    GameState::reset();
    GameState::triggerCameraShake(0.02f, 0.08f);
    GameState::triggerCameraShake(0.15f, 0.4f);
    const auto& h = GameState::hud();
    CHECK(h.shake_amp == doctest::Approx(0.15f));
}

// =============================================================
// triggerPainReaction
// =============================================================

TEST_CASE("F4H6 triggerPainReaction setea pitch + roll + timer") {
    GameState::reset();
    GameState::triggerPainReaction();
    const auto& h = GameState::hud();
    CHECK(h.pain_pitch_amp     == doctest::Approx(2.0f));
    CHECK(h.pain_pitch_t       == doctest::Approx(0.25f));
    CHECK(h.pain_pitch_max_t   == doctest::Approx(0.25f));
    // Roll random en [-1, 1].
    CHECK(h.pain_roll_offset >= -1.0f);
    CHECK(h.pain_roll_offset <=  1.0f);
}

TEST_CASE("F4H6 triggerPainReaction anti-spam: timer > 0.1s no re-trigger") {
    GameState::reset();
    GameState::triggerPainReaction();
    const f32 firstRoll = GameState::hud().pain_roll_offset;
    GameState::triggerPainReaction();  // no-op (timer todavia 0.25s)
    CHECK(GameState::hud().pain_roll_offset == doctest::Approx(firstRoll));
}

// =============================================================
// FpsCamera offsets transient (shake position + pain pitch/roll)
// =============================================================

TEST_CASE("F4H6 FpsCamera setShakeOffset desplaza el view") {
    FpsCamera cam(glm::vec3(0.0f, 1.6f, 0.0f), -90.0f, 0.0f);
    const glm::mat4 mBase = cam.viewMatrix();
    cam.setShakeOffset(glm::vec3(0.1f, 0.0f, 0.0f));
    const glm::mat4 mShaken = cam.viewMatrix();
    // Las matrices difieren = el offset se aplica.
    CHECK(mBase[3].x != doctest::Approx(mShaken[3].x));
}

TEST_CASE("F4H6 FpsCamera setShakeOffset(0) restaura el view") {
    FpsCamera cam;
    const glm::mat4 mBase = cam.viewMatrix();
    cam.setShakeOffset(glm::vec3(0.5f));
    cam.setShakeOffset(glm::vec3(0.0f));
    const glm::mat4 mRestored = cam.viewMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            CHECK(mBase[c][r] == doctest::Approx(mRestored[c][r]));
}

TEST_CASE("F4H6 FpsCamera setPainOffset modifica el forward (pitch)") {
    FpsCamera cam(glm::vec3(0.0f), -90.0f, 0.0f);
    const glm::vec3 fwdBase = cam.forward();
    cam.setPainOffset(/*pitch=*/2.0f, /*roll=*/0.0f);
    const glm::vec3 fwdPained = cam.forward();
    // Pitch +2 → forward apunta mas arriba (y mayor).
    CHECK(fwdPained.y > fwdBase.y);
}

TEST_CASE("F4H6 FpsCamera setPainOffset(0,0) restaura forward") {
    FpsCamera cam(glm::vec3(0.0f), -90.0f, 0.0f);
    const glm::vec3 fwdBase = cam.forward();
    cam.setPainOffset(3.0f, 1.5f);
    cam.setPainOffset(0.0f, 0.0f);
    const glm::vec3 fwdRestored = cam.forward();
    CHECK(fwdBase.x == doctest::Approx(fwdRestored.x));
    CHECK(fwdBase.y == doctest::Approx(fwdRestored.y));
    CHECK(fwdBase.z == doctest::Approx(fwdRestored.z));
}

TEST_CASE("F4H6 FpsCamera defaults: offsets en 0 (sin shake/pain)") {
    FpsCamera cam(glm::vec3(0.0f), -90.0f, 0.0f);
    const glm::vec3 fwdRaw = cam.forward();
    // Sin offsets, forward debe ser el clasico -Z (yaw=-90).
    CHECK(fwdRaw.z < -0.99f);
    CHECK(std::abs(fwdRaw.x) < 0.01f);
}

// =============================================================
// applySplashDamage retorna count de targets
// =============================================================

TEST_CASE("F4H6 applySplashDamage retorna count de entities dañadas") {
    Scene scene;
    Entity a = scene.createEntity("a");
    a.addComponent<TransformComponent>().position = glm::vec3(0.0f);
    auto& ha = a.addComponent<HealthComponent>();
    ha.current = 100.0f; ha.max = 100.0f;

    Entity b = scene.createEntity("b");
    b.addComponent<TransformComponent>().position = glm::vec3(2.0f, 0.0f, 0.0f);
    auto& hb = b.addComponent<HealthComponent>();
    hb.current = 100.0f; hb.max = 100.0f;

    Entity c = scene.createEntity("c");
    c.addComponent<TransformComponent>().position = glm::vec3(20.0f, 0.0f, 0.0f); // afuera
    c.addComponent<HealthComponent>();

    const int n = Projectile::applySplashDamage(scene, glm::vec3(0.0f),
                                                  /*radius=*/5.0f,
                                                  /*baseDamage=*/60.0f,
                                                  /*ignoreOwner=*/0xFFFFFFFFu);
    CHECK(n == 2);
}

TEST_CASE("F4H6 applySplashDamage radius=0 retorna 0") {
    Scene scene;
    Entity a = scene.createEntity("a");
    a.addComponent<TransformComponent>().position = glm::vec3(0.0f);
    a.addComponent<HealthComponent>();
    const int n = Projectile::applySplashDamage(scene, glm::vec3(0.0f), 0.0f,
                                                  60.0f, 0xFFFFFFFFu);
    CHECK(n == 0);
}

// =============================================================
// Crosshair spread (HudState field exists + se setea)
// =============================================================

TEST_CASE("F4H6 HudState.crosshair_spread_deg default 0") {
    GameState::reset();
    CHECK(GameState::hud().crosshair_spread_deg == doctest::Approx(0.0f));
}

TEST_CASE("F4H6 HudState reset limpia todos los timers F4H6") {
    auto& h = GameState::hud();
    h.shake_amp = 1.0f; h.shake_t = 0.5f; h.shake_max_t = 0.5f;
    h.pain_pitch_amp = 3.0f; h.pain_pitch_t = 0.2f;
    h.crosshair_spread_deg = 5.0f;
    GameState::reset();
    CHECK(h.shake_t == doctest::Approx(0.0f));
    CHECK(h.pain_pitch_t == doctest::Approx(0.0f));
    // crosshair_spread_deg NO se resetea por reset (es estado de live sync,
    // no transient). El bridge lo setea a 0 si no hay arma.
}
