// F2H67 Bloque A: tests del VehicleConfig puro. Valida los defaults
// estilo GTA San Andreas y los chequeos de `isValid`.

#include <doctest/doctest.h>

#include "engine/physics/vehicle/VehicleConfig.h"

using namespace Mood;
using namespace Mood::vehicle;

TEST_CASE("VehicleConfig F2H67 A: makeDefaultSA -> valido + 4 ruedas") {
    const VehicleConfig cfg = makeDefaultSA();
    CHECK(isValid(cfg));
    CHECK(cfg.wheels.size() == WheelCount);
}

TEST_CASE("VehicleConfig F2H67 A: defaults SA tienen mass + CoM bajo") {
    const VehicleConfig cfg = makeDefaultSA();
    CHECK(cfg.chassisMass == doctest::Approx(1500.0f));
    // CoM negativo en Y => estable, no se vuelca.
    CHECK(cfg.centerOfMassLocal.y < 0.0f);
    // Chasis sedan tipico: half-extent Z (largo) mayor que X (ancho) y Y (alto).
    CHECK(cfg.chassisHalfExtents.z > cfg.chassisHalfExtents.x);
    CHECK(cfg.chassisHalfExtents.x > cfg.chassisHalfExtents.y);
}

TEST_CASE("VehicleConfig F2H67 A: wheels en 4 esquinas con simetria") {
    const VehicleConfig cfg = makeDefaultSA();
    // Front wheels mas adelante (Z > 0) que rear wheels (Z < 0).
    CHECK(cfg.wheels[WheelFL].attachLocal.z > 0.0f);
    CHECK(cfg.wheels[WheelFR].attachLocal.z > 0.0f);
    CHECK(cfg.wheels[WheelRL].attachLocal.z < 0.0f);
    CHECK(cfg.wheels[WheelRR].attachLocal.z < 0.0f);
    // Left wheels en X<0, right wheels en X>0.
    CHECK(cfg.wheels[WheelFL].attachLocal.x < 0.0f);
    CHECK(cfg.wheels[WheelFR].attachLocal.x > 0.0f);
    CHECK(cfg.wheels[WheelRL].attachLocal.x < 0.0f);
    CHECK(cfg.wheels[WheelRR].attachLocal.x > 0.0f);
    // Simetria izq/der (X coordinate mirror).
    CHECK(cfg.wheels[WheelFL].attachLocal.x ==
          doctest::Approx(-cfg.wheels[WheelFR].attachLocal.x));
    CHECK(cfg.wheels[WheelRL].attachLocal.x ==
          doctest::Approx(-cfg.wheels[WheelRR].attachLocal.x));
}

TEST_CASE("VehicleConfig F2H67 A: solo delanteras steered, traseras handbraked") {
    const VehicleConfig cfg = makeDefaultSA();
    CHECK(cfg.wheels[WheelFL].steered);
    CHECK(cfg.wheels[WheelFR].steered);
    CHECK_FALSE(cfg.wheels[WheelRL].steered);
    CHECK_FALSE(cfg.wheels[WheelRR].steered);

    CHECK_FALSE(cfg.wheels[WheelFL].handbraked);
    CHECK_FALSE(cfg.wheels[WheelFR].handbraked);
    CHECK(cfg.wheels[WheelRL].handbraked);
    CHECK(cfg.wheels[WheelRR].handbraked);

    // 4WD por default (todas driven).
    for (const auto& w : cfg.wheels) CHECK(w.driven);
}

TEST_CASE("VehicleConfig F2H67 A: friction alta SA-style") {
    const VehicleConfig cfg = makeDefaultSA();
    for (const auto& w : cfg.wheels) {
        // SA: alta traccion, no derrapa salvo handbrake.
        CHECK(w.lateralFriction >= 1.0f);
        CHECK(w.longitudinalFriction >= 1.0f);
    }
}

TEST_CASE("VehicleConfig F2H67 A: gears estrictamente decrecientes adelante") {
    const VehicleConfig cfg = makeDefaultSA();
    REQUIRE(cfg.engine.gearRatios.size() >= 2);
    for (usize i = 1; i < cfg.engine.gearRatios.size(); ++i) {
        CHECK(cfg.engine.gearRatios[i] < cfg.engine.gearRatios[i - 1]);
    }
    CHECK(cfg.engine.reverseGearRatios.size() >= 1);
}

TEST_CASE("VehicleConfig F2H67 A: handbrake torque > brake torque") {
    const VehicleConfig cfg = makeDefaultSA();
    // Handbrake debe ser mas fuerte que el brake regular para derrapes
    // controlados estilo SA.
    CHECK(cfg.engine.handbrakeTorque > cfg.engine.brakeTorque);
}

TEST_CASE("VehicleConfig F2H67 A: isValid rechaza mass <= 0") {
    VehicleConfig cfg = makeDefaultSA();
    cfg.chassisMass = 0.0f;
    CHECK_FALSE(isValid(cfg));
    cfg.chassisMass = -100.0f;
    CHECK_FALSE(isValid(cfg));
}

TEST_CASE("VehicleConfig F2H67 A: isValid rechaza wheel con radius <= 0") {
    VehicleConfig cfg = makeDefaultSA();
    cfg.wheels[WheelFL].radius = 0.0f;
    CHECK_FALSE(isValid(cfg));
}

TEST_CASE("VehicleConfig F2H67 A: isValid rechaza minRPM >= maxTorqueRPM") {
    VehicleConfig cfg = makeDefaultSA();
    cfg.engine.minRPM = cfg.engine.maxTorqueRPM;
    CHECK_FALSE(isValid(cfg));
}

TEST_CASE("VehicleConfig F2H67 A: isValid rechaza gearRatios vacio") {
    VehicleConfig cfg = makeDefaultSA();
    cfg.engine.gearRatios.clear();
    CHECK_FALSE(isValid(cfg));
}

TEST_CASE("VehicleConfig F2H67 A: isValid rechaza steerAngle > 90") {
    VehicleConfig cfg = makeDefaultSA();
    cfg.maxSteerAngleDeg = 100.0f;
    CHECK_FALSE(isValid(cfg));
}

TEST_CASE("VehicleConfig F2H67 A: steerLerpSpeed responsivo (>= 2.0)") {
    const VehicleConfig cfg = makeDefaultSA();
    // SA: dirección responsiva pero no instantánea.
    CHECK(cfg.steerLerpSpeed >= 2.0f);
    CHECK(cfg.steerLerpSpeed <= 10.0f);
}
