#include "engine/physics/vehicle/VehicleConfig.h"

#include <cmath>

namespace Mood::vehicle {

VehicleConfig makeDefaultSA() {
    VehicleConfig cfg;

    // Chasis: sedan SA tipico 1.8m alto x 4m largo x 1.8m ancho =>
    // half-extents (0.9, 0.5, 2.0). Z = forward.
    cfg.chassisHalfExtents = glm::vec3(0.9f, 0.5f, 2.0f);
    cfg.chassisMass = 1500.0f;
    // CoM bajo el centro geometrico => mas estable, no se vuelca.
    cfg.centerOfMassLocal = glm::vec3(0.0f, -0.20f, 0.0f);

    // Wheels: 4 esquinas del chasis. Convencion local:
    //   X+ = derecha del auto, Y+ = arriba, Z+ = delante.
    // Las ruedas se montan en `attachLocal` (suspension anchor). La altura
    // del anchor queda un poco arriba del piso del chasis para que el
    // ride height en reposo sea negativo (el auto cuelga).
    //
    // Spread: 0.85 lateral (un poco mas chico que el half-extent X para
    // que la rueda no se hunda en el chasis), 1.5 longitudinal.
    const f32 sx = 0.85f;
    const f32 sz = 1.50f;
    const f32 sy = -0.30f;  // anchor por debajo del centro del chasis

    cfg.wheels[WheelFL].attachLocal = glm::vec3(-sx, sy,  sz);
    cfg.wheels[WheelFR].attachLocal = glm::vec3( sx, sy,  sz);
    cfg.wheels[WheelRL].attachLocal = glm::vec3(-sx, sy, -sz);
    cfg.wheels[WheelRR].attachLocal = glm::vec3( sx, sy, -sz);

    for (WheelConfig& w : cfg.wheels) {
        w.radius = 0.35f;
        w.width  = 0.20f;
        w.suspensionMaxLength = 0.50f;
        w.suspensionMinLength = 0.10f;
        w.suspensionFrequency = 1.5f;
        w.suspensionDamping = 0.5f;
        w.longitudinalFriction = 1.6f;
        w.lateralFriction = 1.4f;
        w.driven = true;  // 4WD por default (estable para arcade SA)
        w.steered = false;
        w.handbraked = false;
    }
    // Solo las delanteras giran con el volante.
    cfg.wheels[WheelFL].steered = true;
    cfg.wheels[WheelFR].steered = true;
    // Solo las traseras tienen handbrake (derrapes controlados).
    cfg.wheels[WheelRL].handbraked = true;
    cfg.wheels[WheelRR].handbraked = true;

    // Engine SA-like: torque amplio, RPM responsivo, 5 gears + 1 reverse.
    cfg.engine.maxTorque = 500.0f;
    cfg.engine.maxTorqueRPM = 4000.0f;
    cfg.engine.maxRPM = 6000.0f;
    cfg.engine.minRPM = 1000.0f;
    cfg.engine.inertia = 0.5f;
    cfg.engine.angularDamping = 0.2f;
    cfg.engine.gearRatios = {2.66f, 1.78f, 1.30f, 1.00f, 0.74f};
    cfg.engine.reverseGearRatios = {2.90f};
    cfg.engine.finalDriveRatio = 3.42f;
    cfg.engine.brakeTorque = 1500.0f;
    cfg.engine.handbrakeTorque = 4000.0f;

    // Steering responsivo (35deg + lerp 4.0/s).
    cfg.maxSteerAngleDeg = 35.0f;
    cfg.steerLerpSpeed = 4.0f;

    return cfg;
}

namespace {
bool isFinitePositive(f32 v) {
    return std::isfinite(v) && v > 0.0f;
}
bool isFiniteNonNeg(f32 v) {
    return std::isfinite(v) && v >= 0.0f;
}
} // anonymous

bool isValid(const VehicleConfig& cfg) {
    if (!isFinitePositive(cfg.chassisMass)) return false;
    if (!std::isfinite(cfg.chassisHalfExtents.x) ||
        !std::isfinite(cfg.chassisHalfExtents.y) ||
        !std::isfinite(cfg.chassisHalfExtents.z)) return false;
    if (cfg.chassisHalfExtents.x <= 0.0f ||
        cfg.chassisHalfExtents.y <= 0.0f ||
        cfg.chassisHalfExtents.z <= 0.0f) return false;
    if (!std::isfinite(cfg.centerOfMassLocal.x) ||
        !std::isfinite(cfg.centerOfMassLocal.y) ||
        !std::isfinite(cfg.centerOfMassLocal.z)) return false;
    if (cfg.wheels.size() != WheelCount) return false;
    for (const WheelConfig& w : cfg.wheels) {
        if (!isFinitePositive(w.radius)) return false;
        if (!isFinitePositive(w.width)) return false;
        if (!isFinitePositive(w.suspensionMaxLength)) return false;
        if (!isFiniteNonNeg(w.suspensionMinLength)) return false;
        if (w.suspensionMinLength >= w.suspensionMaxLength) return false;
        if (!isFinitePositive(w.suspensionFrequency)) return false;
        if (!isFiniteNonNeg(w.suspensionDamping)) return false;
        if (!isFiniteNonNeg(w.longitudinalFriction)) return false;
        if (!isFiniteNonNeg(w.lateralFriction)) return false;
    }
    if (!isFinitePositive(cfg.engine.maxTorque)) return false;
    if (!isFinitePositive(cfg.engine.maxTorqueRPM)) return false;
    if (!isFinitePositive(cfg.engine.maxRPM)) return false;
    if (cfg.engine.maxTorqueRPM >= cfg.engine.maxRPM) return false;
    if (!isFiniteNonNeg(cfg.engine.minRPM)) return false;
    if (cfg.engine.minRPM >= cfg.engine.maxTorqueRPM) return false;
    if (!isFinitePositive(cfg.engine.inertia)) return false;
    if (!isFiniteNonNeg(cfg.engine.angularDamping)) return false;
    if (cfg.engine.gearRatios.empty()) return false;
    for (f32 r : cfg.engine.gearRatios) {
        if (!isFinitePositive(r)) return false;
    }
    for (f32 r : cfg.engine.reverseGearRatios) {
        if (!isFinitePositive(r)) return false;
    }
    if (!isFinitePositive(cfg.engine.finalDriveRatio)) return false;
    if (!isFiniteNonNeg(cfg.engine.brakeTorque)) return false;
    if (!isFiniteNonNeg(cfg.engine.handbrakeTorque)) return false;
    if (!isFinitePositive(cfg.maxSteerAngleDeg)) return false;
    if (cfg.maxSteerAngleDeg > 90.0f) return false;
    if (!isFinitePositive(cfg.steerLerpSpeed)) return false;
    return true;
}

} // namespace Mood::vehicle
