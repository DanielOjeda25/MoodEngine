#pragma once

// F2H82 Bloque C: presets de "feel" físico por tipo de vehículo. El importador
// NO deriva masa/motor/freno de la malla (eso no se puede medir geométricamente
// y sale mal); en cambio el dev elige un tipo (deportivo/sedán/camioneta/
// blindado) que precarga números razonables, y después hace ajuste fino.
//
// Sigue la filosofía data-driven de Source/Rockstar: la geometría sale del
// modelo (Bloque A, VehicleMeshAnalyzer) y el tuning sale de una tabla de
// presets editable. Header puro (sin assimp ni json) → testeable headless.

#include "core/Types.h"

#include <string>

namespace Mood::vehicle {

/// Tipo/clase de vehículo. Determina el preset físico base.
enum class VehicleClass : int {
    Deportivo = 0,   ///< Liviano, mucho torque, suspensión dura, RWD, ágil.
    Sedan     = 1,   ///< Equilibrado, uso general.
    Camioneta = 2,   ///< Pesado, alto, suspensión blanda, AWD, torque alto.
    Blindado  = 3,   ///< Muy pesado, mucho torque para mover la masa, AWD.
    Count     = 4,
};

/// Tren motriz: qué eje(s) reciben torque.
enum class Drivetrain : int { FWD = 0, RWD = 1, AWD = 2 };

/// Números de "feel" físico de un preset. Son los campos que el modal del
/// importador deja editar (ajuste fino) antes de escribir el .moodvehicle.
/// La GEOMETRÍA (extents, track, wheelbase, radios, posiciones de rueda) NO
/// está acá: sale del análisis de la malla.
struct VehiclePhysicsPreset {
    f32 massKg = 1500.0f;

    // Motor.
    f32 peakTorqueNm  = 300.0f;
    f32 peakTorqueRpm = 4000.0f;
    f32 redlineRpm    = 6000.0f;
    f32 idleRpm       = 1000.0f;
    f32 finalDrive    = 3.42f;
    std::string transmission = "5-speed-manual";  // ver presetTransmission()

    // Agarre (fricción de los neumáticos).
    f32 frictionLong = 1.6f;
    f32 frictionLat  = 1.4f;

    // Suspensión.
    f32 suspFrequencyHz = 1.5f;
    f32 suspDamping     = 0.5f;
    f32 suspMaxLenMm    = 300.0f;
    f32 suspMinLenMm    = 100.0f;

    // Frenos (estilo Source: el dev declara deceleración objetivo, no Nm).
    f32 decelTargetMps2 = 8.0f;
    f32 handbrakeRatio  = 0.5f;

    // Dirección.
    f32 maxSteerDeg = 35.0f;
    f32 steerLerp   = 4.0f;

    // Tren motriz + amortiguación del chasis.
    Drivetrain drivetrain     = Drivetrain::AWD;
    f32        chassisLinearDamping  = 0.3f;
    f32        chassisAngularDamping = 0.3f;
};

/// Devuelve el preset físico base para una clase. PURO.
VehiclePhysicsPreset presetFor(VehicleClass cls);

/// Nombre legible de la clase (para UI/logs). PURO.
const char* vehicleClassName(VehicleClass cls);

} // namespace Mood::vehicle
