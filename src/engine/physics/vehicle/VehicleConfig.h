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
#include <string>
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

    /// F2H82 Bloque B (centrado en runtime): nombre del sub-mesh de esta rueda
    /// dentro del .glb del vehiculo. VACIO = el VehicleSystem usa el nombre
    /// canonico `wheel_FL/FR/RL/RR` (ruedas ya centradas por
    /// tools/glb/split_wheels.py, p.ej. el DeLorean). Cuando el importador
    /// detecta un auto SIN procesar, guarda aca el nombre real del nodo
    /// (`RUEDRA_DEL_IZQ`, `f_t_l`, ...) tal cual viene en el modelo.
    std::string meshSubName;

    /// F2H82 Bloque B: centroide del sub-mesh de la rueda en MODEL space (el
    /// mismo frame que los vertices del MeshAsset cargado). El render path
    /// dibuja la rueda con `worldMatrix * translate(-meshHubOffset)` para que
    /// rote en su hub sin tener que re-exportar/centrar el .glb. (0,0,0) = la
    /// malla ya esta centrada en el hub (caso DeLorean).
    glm::vec3 meshHubOffset{0.0f};
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
///        (JSON) o construida en codigo con `makeFallbackGenericSedan()`.
struct VehicleConfig {
    /// F2H70.3 Bloque F: nombre legible del vehiculo (de `metadata.name` en
    /// el .moodvehicle, ej. "DeLorean DMC-12"). Usado como tag del entity al
    /// spawnear via drop, y por el browser. Vacio = el callsite usa un
    /// fallback (p.ej. el filename stem).
    std::string displayName;

    /// F2H70.3 Bloque F: mesh visual del vehiculo (path logico relativo a
    /// assets/, ej. "vehicles/delorean/delorean.glb"). Hace al `.moodvehicle`
    /// self-contained — soltar uno en el viewport spawnea un entity completo
    /// (Transform + MeshRenderer(meshPath) + VehicleComponent). Vacio = el
    /// callsite debe asignar el mesh aparte (p.ej. drop sobre entity existente).
    std::string meshPath;

    /// Half-extents del chassis box (collision shape). SA sedan tipico:
    /// 2.0 m largo x 0.5 m alto x 0.9 m ancho => half = (0.9, 0.5, 2.0)
    /// (axis convencion: +Z forward, +Y up, +X right).
    glm::vec3 chassisHalfExtents{0.9f, 0.5f, 2.0f};
    /// F2H82 Bloque B (caja al centro del cuerpo): offset del CENTRO de la caja
    /// fisica respecto al ORIGEN del entity/modelo. (0,0,0) = caja centrada en
    /// el origen (caso DeLorean: origen del modelo == centro del cuerpo).
    /// Para autos importados cuyo origen NO esta en el centro del cuerpo
    /// (ej. origen en la base/atras), el importador guarda aca el centroide
    /// del chasis en MODEL space. Sin esto, la caja queda en el piso y el
    /// cuerpo flota encima. La interpreta `PhysicsWorld::createVehicle`
    /// envolviendo el `BoxShape` en un `RotatedTranslatedShape`.
    glm::vec3 chassisBoxOffset{0.0f, 0.0f, 0.0f};
    /// Masa total del chasis en kg. SA sedan: 1500 kg.
    f32 chassisMass = 1500.0f;
    /// Offset del centro de masa respecto al ORIGEN del entity/modelo
    /// (LOCAL space). Y negativo = bajo el centro => mas estable, no flips.
    /// Para autos cuyo origen NO esta en el centro del cuerpo, el importador
    /// emite aca `chassisBoxOffset + (0, -0.2*halfY, 0)` para que el CoM
    /// quede un poquito debajo del centro del cuerpo (no debajo del piso).
    glm::vec3 centerOfMassLocal{0.0f, -0.20f, 0.0f};

    /// F2H70.2: damping lineal del chasis (resistencia al movimiento sin
    /// input). Aplicado a `JPH::BodyCreationSettings::mLinearDamping`. Sin
    /// esto, el auto rueda infinito al soltar el acelerador (momentum sin
    /// freno aerodinamico/friccional).
    /// Default 0.3 = arcade-ish responsivo: el auto frena en ~5s sin gas
    /// pero NO se siente "pesado" al acelerar. Tuneado runtime con dev
    /// reporte "se siente mas pesado al mover" en 0.5 → bajado a 0.3.
    /// 0.05 (Jolt default low) = sim-floppy (rueda infinito).
    f32 chassisLinearDamping = 0.3f;
    /// F2H70.2: damping angular del chasis. Resistencia al giro libre (sin
    /// steering input). Sin esto, un golpe lateral hace al chasis rotar
    /// indefinidamente. Default 0.3 = arcade estable sin freno al girar.
    /// Mismo path al `mAngularDamping` de Jolt.
    f32 chassisAngularDamping = 0.3f;

    /// F2H70.2 D5: yaw offset del MESH visual respecto al chasis fisico
    /// (grados, alrededor del eje Y local del auto). Sirve para reconciliar
    /// la convencion "+Z forward" del engine con GLBs autoreados en otra
    /// convencion (-Z, +X, etc.) SIN tener que bakear el asset o parchear
    /// cada moodmap con `rotationEuler`. La rotacion se aplica en LOCAL
    /// space (post-multiply al world matrix del chasis Jolt) ANTES de
    /// escribir al TransformComponent — afecta solo lo visual, NO la fisica.
    ///
    /// Casos tipicos:
    ///   0   = mesh mira +Z (industry standard glTF/Source — nada a hacer)
    ///   180 = mesh mira -Z (DeLorean Sketchfab y otros assets legacy)
    ///   90  = mesh mira -X (modelos rotados sideways al exportar)
    ///   -90 = mesh mira +X
    ///
    /// Drag-and-drop pipeline: arrastras un .glb nuevo, generas un
    /// .moodvehicle base; si al subirte los controles se sienten invertidos
    /// (W va para atras visualmente), editas este campo y listo. Persiste
    /// con el auto, no con el map -- vale en cualquier escena donde aparezca.
    f32 meshYawOffsetDeg = 0.0f;

    /// 4 ruedas en orden fijo (FL, FR, RL, RR).
    std::array<WheelConfig, WheelCount> wheels{};

    /// Tuning del motor.
    EngineConfig engine{};

    /// Maximo angulo de direccion en grados. SA: 35 (responsivo).
    f32 maxSteerAngleDeg = 35.0f;
    /// Velocidad de lerp del steering input al angulo real (1/s).
    /// SA: 4.0 (cambia rapido pero no instantaneo).
    f32 steerLerpSpeed = 4.0f;

    /// F2H82 polish: factor de escala visual del mesh. Aplica a
    /// `TransformComponent.scale` cuando se spawnea el vehiculo. Caso de uso:
    /// modelos exportados con vertices en mm o cm (tipico Sketchfab/Maya) cuyo
    /// AABB resulta minusculo (4 cm en vez de 4 m). El modal del importador
    /// ofrece este multiplicador (default 1.0 = mesh ya en metros). Los valores
    /// FISICOS (chassisHalfExtents, attachLocal, wheel.radius...) se guardan ya
    /// scaled (en metros reales). Solo el render del MESH necesita el
    /// multiplicador adicional porque sus vertices estan en unidades crudas.
    f32 meshImportScale = 1.0f;
};

/// @brief Construye una `VehicleConfig` generica de fallback: sedan medio,
///        4WD, alta traccion, brakes fuertes, dirección responsiva,
///        suspension blanda, no se vuelca facil. Usada SOLO cuando un
///        `.moodvehicle` no carga (path vacio o invalido) — los assets
///        reales viven en `assets/vehicles/<name>/<name>.moodvehicle` con
///        specs derivadas del modelo real (ver `delorean_dmc12.moodvehicle`).
///
///        Si `VehicleSystem` cae a este fallback, loguea warn — eso indica
///        que algo esta mal con el config del vehicle.
///
///        Wheel layout: 4 ruedas en las 4 esquinas del chasis,
///        delanteras steered, traseras handbraked, todas driven (4WD).
VehicleConfig makeFallbackGenericSedan();

/// @brief Suma de los `gearRatios` y demas validaciones sanas. Devuelve
///        true si el config no tiene NaN/inf, `chassisMass > 0`, exactamente
///        4 wheels, gears no vacios, y los ratios son estrictamente
///        positivos. Util para tests + para gate de loader del asset.
bool isValid(const VehicleConfig& cfg);

/// @brief F2H70.2 Bloque B — distancia que comprime la suspension de una
///        wheel bajo gravedad en equilibrio (mass-independent: depende solo
///        de la frecuencia natural del resorte).
///
///        Para un sistema masa-resorte, en equilibrio:
///          F_spring = F_gravedad => k * x = m * g
///        Con `k = (2π * f)² * m` (definicion de frecuencia natural):
///          x = g / (2π * f)²
///
///        Es decir, la `m` se cancela — el spring compression al settle
///        natural depende solo de `f`. Para f=1.8 Hz (DeLorean default):
///        x ≈ 0.077 m = 7.7 cm.
///
///        Usado por `VehicleSystem::chassisRenderYOffset` para elevar el
///        spawn del chassis por encima del piso de modo que las wheels
///        apoyen apenas la sim arranca (sino el chassis "cae" 7-10 cm
///        post-spawn por el settle, generando un brinco visual).
f32 wheelRestCompression(const WheelConfig& w);

} // namespace Mood::vehicle
