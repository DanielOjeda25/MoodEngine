#pragma once

// F2H67 Bloque A: descripcion declarativa del tuning de un vehiculo.
// Header puro (sin Jolt) -- usable headless en tests + serializable a JSON
// como asset .moodvehicle.
//
// Convencion del orden de ruedas (fija):
//   [0] FrontLeft (FL)
//   [1] FrontRight (FR)
//   [2] RearLeft (RL)
//   [3] RearRight (RR)
//
// El PhysicsWorld (Bloque C) consume este struct al `createVehicle` para
// armar las settings reales de `JPH::VehicleConstraint` + `WheeledVehicle-
// Controller`.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <array>
#include <vector>

namespace Mood::vehicle {

/// @brief Indices fijos para acceder a las 4 ruedas en `VehicleConfig::wheels`.
///        Si emerge demanda para motos (2 wheels) o tanques, abrir hito propio.
enum WheelIndex : int {
    WheelFL = 0,  ///< Front Left
    WheelFR = 1,  ///< Front Right
    WheelRL = 2,  ///< Rear Left
    WheelRR = 3,  ///< Rear Right
    WheelCount = 4,
};

/// @brief Config de una rueda individual.
///
///        `attachLocal` es el punto donde la suspension se "ancla" al
///        chasis, en LOCAL space del chasis (Y up). La rueda en reposo
///        cuelga `suspensionMaxLength` por debajo. El visual mesh debe
///        renderizarse en el punto que retorna `readVehicleState` (la
///        composicion la hace el wrapper).
struct WheelConfig {
    /// Punto donde la suspension se monta al chasis (local space).
    glm::vec3 attachLocal{0.0f};
    /// Radio de la rueda en metros. SA default: 0.35 m.
    f32 radius = 0.35f;
    /// Ancho de la rueda en metros. SA default: 0.20 m.
    f32 width  = 0.20f;

    /// Longitud maxima de la suspension cuando esta totalmente extendida
    /// (sin carga). Mas largo = mas absorcion de baches.
    f32 suspensionMaxLength = 0.50f;
    /// Longitud minima cuando totalmente comprimida.
    f32 suspensionMinLength = 0.10f;
    /// Frecuencia natural del resorte (Hz). 1.5 = blando estilo SA.
    f32 suspensionFrequency = 1.5f;
    /// Damping ratio (0 = sin freno, 1 = critico). 0.5 = blando absorbente.
    f32 suspensionDamping = 0.5f;

    /// Friccion longitudinal (aceleracion / frenado). Alta = no patina al
    /// acelerar. SA: 1.6 (genera tirones fuertes en aceleracion).
    f32 longitudinalFriction = 1.6f;
    /// Friccion lateral (curvas). Alta = pega al asfalto. SA: 1.4 (no
    /// derrapa salvo handbrake).
    f32 lateralFriction = 1.4f;

    /// Si esta rueda recibe torque del motor (4WD = todas; FWD = solo FL+FR).
    bool driven = true;
    /// Si esta rueda gira con el volante (steered = solo delanteras tipicamente).
    bool steered = false;
    /// Si el handbrake aplica torque de frenado a esta rueda (traseras tipico).
    bool handbraked = false;
};

/// @brief Config del motor + transmision + frenos.
struct EngineConfig {
    /// Torque maximo del motor en Nm. SA-like sedan: 500 Nm.
    f32 maxTorque = 500.0f;
    /// RPM en torque maximo (aprox la mitad del rango).
    f32 maxTorqueRPM = 4000.0f;
    /// RPM redline.
    f32 maxRPM = 6000.0f;
    /// RPM de idle (motor "encendido" en reposo).
    f32 minRPM = 1000.0f;

    /// Inercia angular del motor (kg*m^2). Mas alto = mas demora en
    /// cambiar RPM. SA: 0.5 (responsivo).
    f32 inertia = 0.5f;
    /// Damping angular del motor (sin gas, decae a idle).
    f32 angularDamping = 0.2f;

    /// Ratios de marcha adelante. SA: 5 gears tipico sedan.
    /// Indice 0 = 1ra (corta, mucho torque), indice N-1 = ultima (larga).
    std::vector<f32> gearRatios{2.66f, 1.78f, 1.30f, 1.00f, 0.74f};
    /// Ratios de marcha atras (negativos NO, se interpreta como reverse
    /// por el sentido del transmission). SA tipico: 1 sola marcha atras.
    std::vector<f32> reverseGearRatios{2.90f};
    /// Final drive ratio (entre transmission y differentials).
    f32 finalDriveRatio = 3.42f;

    /// Torque de freno regular (en cada wheel que tenga freno habilitado).
    /// Aplicado proporcional al input `brake` [0,1].
    f32 brakeTorque = 1500.0f;
    /// Torque de handbrake (solo ruedas con `handbraked = true`). Alto =
    /// derrapes controlados.
    f32 handbrakeTorque = 4000.0f;
};

/// @brief Config completa del vehiculo. Materializable en `.moodvehicle`
///        (JSON) o construida en codigo con `makeDefaultSA()`.
struct VehicleConfig {
    /// Half-extents del chassis box (collision shape). SA sedan tipico:
    /// 2.0 m largo x 0.5 m alto x 0.9 m ancho => half = (0.9, 0.5, 2.0)
    /// (axis convencion: +Z forward, +Y up, +X right).
    glm::vec3 chassisHalfExtents{0.9f, 0.5f, 2.0f};
    /// Masa total del chasis en kg. SA sedan: 1500 kg.
    f32 chassisMass = 1500.0f;
    /// Offset del centro de masa respecto al centro geometrico del chasis
    /// (LOCAL space). Y negativo = bajo el centro => mas estable, no flips.
    glm::vec3 centerOfMassLocal{0.0f, -0.20f, 0.0f};

    /// 4 ruedas en orden fijo (FL, FR, RL, RR).
    std::array<WheelConfig, WheelCount> wheels{};

    /// Tuning del motor.
    EngineConfig engine{};

    /// Maximo angulo de direccion en grados. SA: 35 (responsivo).
    f32 maxSteerAngleDeg = 35.0f;
    /// Velocidad de lerp del steering input al angulo real (1/s).
    /// SA: 4.0 (cambia rapido pero no instantaneo).
    f32 steerLerpSpeed = 4.0f;
};

/// @brief Construye una `VehicleConfig` con valores estilo GTA San Andreas:
///        sedan medio, 4WD, alta traccion, brakes fuertes, dirección
///        responsiva, suspension blanda, no se vuelca facil. Usable
///        directamente para el sample "Banshee_SA" del Bloque G.
///
///        Wheel layout: 4 ruedas en las 4 esquinas del chasis,
///        delanteras steered, traseras handbraked, todas driven (4WD).
VehicleConfig makeDefaultSA();

/// @brief Suma de los `gearRatios` y demas validaciones sanas. Devuelve
///        true si el config no tiene NaN/inf, `chassisMass > 0`, exactamente
///        4 wheels, gears no vacios, y los ratios son estrictamente
///        positivos. Util para tests + para gate de loader del asset.
bool isValid(const VehicleConfig& cfg);

} // namespace Mood::vehicle
