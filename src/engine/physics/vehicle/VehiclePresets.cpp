// F2H82 Bloque C: tabla de presets físicos por tipo de vehículo.
// Números pensados para feel arcade-realista (consistente con el DeLorean y el
// fallback sedan de VehicleConfig.cpp), no simulación pura.

#include "engine/physics/vehicle/VehiclePresets.h"

namespace Mood::vehicle {

VehiclePhysicsPreset presetFor(VehicleClass cls) {
    VehiclePhysicsPreset p;
    switch (cls) {
    case VehicleClass::Deportivo:
        // Liviano y nervioso: alto torque/peso, suspensión dura, RWD, agarre
        // alto, dirección rápida. 0-100 ~5-6s.
        p.massKg          = 1300.0f;
        p.peakTorqueNm    = 420.0f;
        p.peakTorqueRpm   = 4500.0f;
        p.redlineRpm      = 7000.0f;
        p.idleRpm         = 1000.0f;
        p.finalDrive      = 3.70f;
        p.transmission    = "6-speed-manual";
        p.frictionLong    = 1.8f;
        p.frictionLat     = 1.6f;
        p.suspFrequencyHz = 2.2f;   // duro
        p.suspDamping     = 0.6f;
        p.suspMaxLenMm    = 220.0f;
        p.suspMinLenMm    = 80.0f;
        p.decelTargetMps2 = 9.5f;
        p.handbrakeRatio  = 0.6f;
        p.maxSteerDeg     = 38.0f;
        p.steerLerp       = 5.0f;
        p.drivetrain      = Drivetrain::RWD;
        p.chassisLinearDamping  = 0.2f;
        p.chassisAngularDamping = 0.3f;
        break;

    case VehicleClass::Sedan:
        // Equilibrado, uso general (similar al fallback genérico).
        p.massKg          = 1500.0f;
        p.peakTorqueNm    = 300.0f;
        p.peakTorqueRpm   = 4000.0f;
        p.redlineRpm      = 6000.0f;
        p.idleRpm         = 1000.0f;
        p.finalDrive      = 3.42f;
        p.transmission    = "5-speed-manual";
        p.frictionLong    = 1.6f;
        p.frictionLat     = 1.4f;
        p.suspFrequencyHz = 1.5f;
        p.suspDamping     = 0.5f;
        p.suspMaxLenMm    = 300.0f;
        p.suspMinLenMm    = 100.0f;
        p.decelTargetMps2 = 8.0f;
        p.handbrakeRatio  = 0.5f;
        p.maxSteerDeg     = 35.0f;
        p.steerLerp       = 4.0f;
        p.drivetrain      = Drivetrain::FWD;
        p.chassisLinearDamping  = 0.3f;
        p.chassisAngularDamping = 0.3f;
        break;

    case VehicleClass::Camioneta:
        // Pesada y alta: suspensión blanda y larga, AWD, torque alto, agarre
        // moderado, dirección más lenta. Tiende a balancearse.
        p.massKg          = 2300.0f;
        p.peakTorqueNm    = 480.0f;
        p.peakTorqueRpm   = 3200.0f;   // torque bajo de RPM (Diesel-like)
        p.redlineRpm      = 5000.0f;
        p.idleRpm         = 800.0f;
        p.finalDrive      = 3.90f;
        p.transmission    = "4-speed-auto";
        p.frictionLong    = 1.5f;
        p.frictionLat     = 1.2f;
        p.suspFrequencyHz = 1.2f;   // blando
        p.suspDamping     = 0.45f;
        p.suspMaxLenMm    = 380.0f;
        p.suspMinLenMm    = 120.0f;
        p.decelTargetMps2 = 7.0f;
        p.handbrakeRatio  = 0.4f;
        p.maxSteerDeg     = 32.0f;
        p.steerLerp       = 3.5f;
        p.drivetrain      = Drivetrain::AWD;
        p.chassisLinearDamping  = 0.35f;
        p.chassisAngularDamping = 0.4f;
        break;

    case VehicleClass::Blindado:
        // Muy pesado: necesita mucho torque para moverse, suspensión firme
        // por la carga, AWD, agarre relativo bajo (mucha inercia), dirección
        // lenta. Cuesta acelerar y frenar.
        p.massKg          = 5000.0f;
        p.peakTorqueNm    = 900.0f;
        p.peakTorqueRpm   = 3000.0f;
        p.redlineRpm      = 4800.0f;
        p.idleRpm         = 800.0f;
        p.finalDrive      = 4.30f;
        p.transmission    = "4-speed-auto";
        p.frictionLong    = 1.4f;
        p.frictionLat     = 1.1f;
        p.suspFrequencyHz = 1.6f;   // firme por la masa
        p.suspDamping     = 0.55f;
        p.suspMaxLenMm    = 300.0f;
        p.suspMinLenMm    = 110.0f;
        p.decelTargetMps2 = 6.0f;
        p.handbrakeRatio  = 0.3f;
        p.maxSteerDeg     = 30.0f;
        p.steerLerp       = 3.0f;
        p.drivetrain      = Drivetrain::AWD;
        p.chassisLinearDamping  = 0.4f;
        p.chassisAngularDamping = 0.5f;
        break;

    case VehicleClass::Count:
    default:
        break;  // devuelve los defaults del struct (sedan-ish)
    }
    return p;
}

const char* vehicleClassName(VehicleClass cls) {
    switch (cls) {
    case VehicleClass::Deportivo: return "Deportivo";
    case VehicleClass::Sedan:     return "Sedan";
    case VehicleClass::Camioneta: return "Camioneta";
    case VehicleClass::Blindado:  return "Blindado";
    default:                      return "Desconocido";
    }
}

} // namespace Mood::vehicle
