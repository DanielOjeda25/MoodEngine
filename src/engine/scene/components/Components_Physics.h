#pragma once

// F2H81 (auditoría): split de Components.h por categoría. Componentes de
// física / constraints / vehículos / cloth. Incluir via `Components.h`.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

/// @brief Rigid body (Hito 12). Presencia física en Jolt: gravedad y
///        colisiones. `bodyId` lo llena `PhysicsSystem` cuando crea el
///        body en el `PhysicsWorld`; hasta entonces vale 0.
///        Cambiar shape/mass en runtime requiere recrear el body (hoy solo
///        se permite fuera de Play Mode; el Inspector lo va a grayar).
struct RigidBodyComponent {
    enum class Type  : u8 { Static = 0, Kinematic = 1, Dynamic = 2 };
    enum class Shape : u8 { Box = 0, Sphere = 1, Capsule = 2 };

    Type  type  = Type::Dynamic;
    Shape shape = Shape::Box;
    glm::vec3 halfExtents{0.5f}; // Box: (x,y,z). Sphere: (r,_,_). Capsule: (halfH, r, _).
    f32 mass = 1.0f;              // kg, solo Dynamic
    // Hito 34 A: friction per-body. Default 0.5 (heredado del Hito 31 D —
    // realista para madera-sobre-madera). Aplica al body Static y Dynamic;
    // en Static no afecta al body en si pero si al contacto contra otros.
    // Editar desde el Inspector se aplica al re-materializar (proximo
    // entrar a Play Mode); no hay setter en runtime por ahora.
    f32 friction = 0.5f;

    // F2H68: si true, el body se materializa como SENSOR (Jolt
    // `mIsSensor=true`). El body detecta contactos (dispara
    // ContactListener) pero no empuja ni bloquea. Equivalente a Unity
    // `Collider.isTrigger` / Unreal `CollisionResponseChannel::Overlap`.
    // Uso: NPC hitbox para auto-ragdoll por embestida — el chassis cruza
    // sin rebote y el listener dispara la transicion a Ragdolling.
    bool isSensor = false;

    u32 bodyId = 0;               // llenado por PhysicsSystem (0 = no creado)

    // F2H40: cache del ultimo halfExtents sincronizado al body Jolt.
    // Si `halfExtents != lastSyncedHalfExtents`, `updateRigidBodies`
    // llama `setBodyHalfExtents` para resincronizar. Cubre 2 vectores:
    //   1. Para Box bodies, auto-actualiza `halfExtents = t.scale*0.5`
    //      cuando el dev escala el Transform (Inspector / gizmo). Sin
    //      esto, el visual se ensancha pero la colision queda con el
    //      tamaño original — el player atraviesa o cae al vacio.
    //   2. Para cualquier shape, sincroniza si el dev edita
    //      `halfExtents` directo en el Inspector (Sphere radius,
    //      Capsule height, etc.).
    // NO se serializa (estado runtime). Default 0 fuerza primer sync
    // al materializar el body.
    glm::vec3 lastSyncedHalfExtents{0.0f};

    // Hito 41 fix-up #2: pending velocidades del Save/Load. Si
    // `applyLoadedSave` corre ANTES de que el body se materialice
    // (bodyId == 0), las vels del snapshot se quedan stash aca.
    // `updateRigidBodies` las aplica al body recien creado y limpia
    // el flag. NO se serializan en `.moodmap` ni `.moodsave` (estado
    // transitorio entre load y siguiente updateRigidBodies).
    bool hasPendingVel = false;
    glm::vec3 pendingLinearVel{0.0f};
    glm::vec3 pendingAngularVel{0.0f};

    RigidBodyComponent() = default;
    RigidBodyComponent(Type t, Shape s, glm::vec3 he, f32 m = 1.0f)
        : type(t), shape(s), halfExtents(he), mass(m) {}
};

/// @brief F2H65: sentinel "sin target asignado" para JointComponent.
///        UINT32_MAX (== static_cast<u32>(entt::null)) en vez de 0 porque
///        la primera entidad creada en una Scene fresca tiene raw handle
///        0 — un sentinel == 0 colisionaria con un target legitimo a esa
///        entidad.
constexpr u32 kJointNoTarget = static_cast<u32>(~static_cast<u32>(0));

/// @brief F2H65: constraint (joint) entre 2 bodies fisicos. Permite armar
///        puertas con bisagra (Hinge), cuerdas/cadenas (Distance), y
///        pivotes 3DoF (Point) estilo Source / Unity / Godot.
///
///        El componente vive en la entidad "A" (uno de los dos cuerpos del
///        joint). `targetEntity` es el raw entt::entity handle de la
///        entidad "B" -- el otro cuerpo. Ambas deben tener RigidBodyComponent
///        con bodyId valido para que el PhysicsSystem cree el constraint.
struct JointComponent {
    enum class Type : u8 {
        Hinge    = 0,   // 1 eje rotacion + limits (puertas, brazos)
        Distance = 1,   // distancia entre 2 puntos pivot (cuerdas)
        Point    = 2,   // 3 ejes rotacion libres, 0 translation (ball joint)
        Slider   = 3,   // 1 eje translation + limits (cajones, ascensores, pistones)
        Fixed    = 4,   // los 6 DOF locked; suelda 2 bodies (plataformas, props)
    };

    Type      type = Type::Hinge;

    /// @brief Raw entt::entity handle del body B. kJointNoTarget = sin
    ///        asignar (el PhysicsSystem skipea la creacion hasta que el
    ///        dev lo setee desde el Inspector o via drag-drop del Hierarchy).
    u32       targetEntity = kJointNoTarget;

    /// @brief Pivot del joint en local space de la entity A (esta).
    ///        Para Hinge: posicion de la bisagra.
    ///        Para Distance: punto de attach en A (el de B es origen de B).
    ///        Para Point: pivot compartido (mismo punto en world coords
    ///        que B(0,0,0)).
    glm::vec3 pivotLocal{0.0f};

    /// @brief Hinge: eje de rotacion en local space de A. Slider: eje de
    ///        translation (la direccion del riel). Default Y-up = bisagra
    ///        vertical estilo puerta / riel vertical estilo ascensor.
    glm::vec3 axisLocal{0.0f, 1.0f, 0.0f};

    /// @brief Solo Hinge: limits angulares en grados. Defaults a
    ///        [-180, 180] = libre rotacion. El dev los ajusta a [0, 90]
    ///        para puerta de un solo lado, etc.
    f32       limitMinDeg = -180.0f;
    f32       limitMaxDeg = 180.0f;

    /// @brief Solo Distance: rango de distancia entre los 2 pivots
    ///        (world coords). min==max -> rigido. min<max -> cuerda
    ///        que puede flexionar entre los limites.
    f32       minDistance = 0.0f;
    f32       maxDistance = 1.0f;

    /// @brief Solo Slider: rango de translation a lo largo de `axisLocal`
    ///        en metros, relativo a la pose inicial. min==max -> bloqueado
    ///        (no desliza). min<max -> desliza dentro del rango (ej. cajon
    ///        [0, 0.5] = abre 50cm). Estilo prismatic joint (Unity/Unreal).
    f32       sliderLimitMin = 0.0f;
    f32       sliderLimitMax = 1.0f;

    // --- Runtime (NO se persiste) ---

    /// @brief Handle del constraint en PhysicsWorld. 0 = no creado.
    u32  constraintId = 0;

    /// @brief Marcar true para forzar destroy + create del constraint en
    ///        el proximo tick del PhysicsSystem. Se setea desde el
    ///        Inspector cuando el dev cambia type/pivot/axis/limits, y se
    ///        limpia a false una vez aplicado. Tambien default true al
    ///        spawn para que el primer tick cree el constraint.
    bool dirty = true;
};

/// @brief F2H66: marker para ragdoll fisico. Cuando `state == Ragdolling`
///        el RagdollSystem auto-construye 14 bodies (capsules) + 13
///        constraints (Hinge/SwingTwist) a partir del skeleton Mixamo del
///        mesh asociado y los pone bajo control de Jolt. El Animator queda
///        pausado y las matrices de skinning se derivan de las poses
///        fisicas — el cadaver flopa realista, no se mueve por animacion.
///
///        Convencion HL2: una vez Ragdolling no se vuelve a Animated
///        (sin scope F2H66). Para "revivir" un NPC, destruir + recrear
///        la entity.
struct RagdollComponent {
    enum class State : u8 { Animated = 0, Ragdolling = 1 };
    State state = State::Animated;

    /// @brief Masa total del cadaver (kg). Se distribuye por volumen entre
    ///        los 14 bodies. Default 70 kg = persona promedio.
    f32 totalMass = 70.0f;

    /// @brief Radio de los capsules de extremidades (m). El torso usa ~1.8x.
    f32 limbRadius = 0.05f;

    /// @brief Si los bodies caen por gravedad. Default true. Apagable para
    ///        ragdolls zero-G / espacio.
    bool useGravity = true;

    /// @brief Impulse world-space aplicado al body torso al transicionar
    ///        a Ragdolling. Convencion HL2: el cadaver vuela en la
    ///        direccion del disparo. Si es (0,0,0) el ragdoll arranca
    ///        en reposo y solo cae por gravedad. Magnitud tipica: 5-50 N·s.
    glm::vec3 spawnImpulse{0.0f};

    // --- Runtime (NO se persiste) ---

    /// @brief Handle del ragdoll en PhysicsWorld. 0 = no creado.
    u32 ragdollId = 0;
};

/// @brief F2H67: marca una entity como chassis de un vehiculo conducible.
///        El `VehicleSystem` consume `configPath` para construir 1 body
///        chassis + 4 wheels en Jolt via `JPH::VehicleConstraint`.
struct VehicleComponent {
    /// @brief Path al asset `.moodvehicle` (relativo a `assets/`). Ej.
    ///        "vehicles/banshee_sa.moodvehicle". Si esta vacio o el asset
    ///        no carga, el VehicleSystem skipea materializacion + log warn.
    std::string configPath;

    /// @brief Input state - Lua lo escribe cada frame. Rango [0, 1] para
    ///        throttle/brake/handbrake, [-1, 1] para steer (negativo = izq).
    f32 inputThrottle  = 0.0f;
    f32 inputBrake     = 0.0f;
    f32 inputSteer     = 0.0f;
    f32 inputHandbrake = 0.0f;

    // --- Runtime (NO se persiste) ---

    /// @brief Handle del vehiculo en PhysicsWorld. 0 = no creado.
    u32 vehicleId = 0;

    /// @brief Handles entt (raw u32) de los 4 child-entities que renderizan
    ///        las ruedas. Orden fijo: FL, FR, RL, RR. 0 = aun no asignado.
    ///        El VehicleSystem las auto-spawnea si todas son 0 al primer
    ///        materialize.
    u32 wheelEntities[4] = {0, 0, 0, 0};

    /// @brief Marker de re-materializacion (al editar `configPath` o al
    ///        spawnear). Sigue el patron F2H65/F2H66 (`dirty=true` =>
    ///        destroy + recreate en el proximo tick).
    bool dirty = true;
};

/// @brief F2H82 Bloque B: marca una entity como rueda auto-spawneada por el
///        VehicleSystem. Funciona como flag de "entity interna del motor":
///        NO seleccionable por click, NO listada en la jerarquia, NO
///        serializada — la materializa el chasis dueño en cada load.
///        Reemplaza el check legacy `isWheelEntityTag(name)` que solo cubria
///        los 4 nombres canonicos `wheel_FL/FR/RL/RR` y dejaba escapar autos
///        importados con nombres reales (ej. `f_t_l`, `RUEDRA_*`).
struct VehicleWheelMarker {
    u32 chassisHandle = 0;  ///< handle entt del chasis dueño
    int wheelIndex = -1;    ///< 0=FL, 1=FR, 2=RL, 3=RR
};

/// @brief F2H67: marca el offset del asiento del conductor sobre el chassis
///        (local space). El sistema de mount/dismount del player teleporta
///        al player a `chassis.worldMatrix * seatOffset` al subirse y a
///        un punto al lado del auto al bajarse.
struct VehicleSeatComponent {
    glm::vec3 seatOffsetLocal{0.0f, 0.6f, 0.2f};
};

/// @brief F2H72: zona de fuerza fisica. Cada frame en Play, el
///        ForceFieldSystem aplica una fuerza a los RigidBody Dynamic cuyo
///        centro cae dentro de la zona. Sin colision solida (los objetos
///        atraviesan la zona libremente, igual que un TriggerComponent).
struct ForceFieldComponent {
    enum class Shape : u8 { Box = 0, Sphere = 1 };
    /// Directional = viento (fuerza constante en `direction`).
    /// Radial = explosion / atractor (fuerza a lo largo del vector
    ///          centro->body; `strength` > 0 empuja afuera, < 0 atrae).
    enum class Mode  : u8 { Directional = 0, Radial = 1 };

    Shape shape = Shape::Sphere;
    Mode  mode  = Mode::Radial;

    glm::vec3 halfExtents{2.0f, 2.0f, 2.0f}; // Box (metros directos)
    f32       radius = 3.0f;                  // Sphere

    /// Solo Directional: direccion del "viento" en world space (se
    /// normaliza). Ignorado en Radial.
    glm::vec3 direction{0.0f, 1.0f, 0.0f};

    /// Magnitud de la fuerza. Radial: > 0 empuja afuera (explosion),
    /// < 0 atrae al centro (gravedad/iman). Newtons, o m/s^2 si
    /// `ignoreMass == true`.
    f32 strength = 20.0f;

    /// Solo Radial: si true, escala la fuerza por (1 - dist/range) — max en
    /// el centro, 0 en el borde de la zona (falloff lineal estilo explosion).
    bool linearFalloff = true;

    /// Si true, la fuerza es independiente de la masa (se interpreta como
    /// aceleracion: el sistema la multiplica por la masa del body). Util
    /// para viento / zonas de gravedad donde todo cae/flota igual.
    bool ignoreMass = false;

    /// Master switch — un script puede apagar/prender la zona.
    bool enabled = true;
};

/// F2H75: tela (cloth) simulada como soft body. Grilla procedural NxM de
/// particulas unidas por resortes, anclada por un borde/esquinas; cuelga por
/// gravedad y ondea con las zonas de viento (`ForceFieldComponent`). Se
/// renderiza como mesh dinamico (vertices actualizados por frame desde Jolt).
struct ClothComponent {
    /// Que parte de la tela queda fija (espejo de `cloth::AnchorEdge`; se
    /// mantiene local para no acoplar al header de fisica, mismo criterio
    /// que `RagdollComponent::State`).
    enum class Anchor : u8 {
        None       = 0,  ///< Cae libre (trapo/paracaidas).
        TopEdge    = 1,  ///< Borde superior (cortina/bandera colgante).
        TopCorners = 2,  ///< Esquinas de arriba (guirnalda).
        LeftEdge   = 3,  ///< Borde izquierdo (bandera en asta).
    };

    // --- Parametros serializados (definen la tela) ---
    f32    width  = 2.0f;            ///< Ancho en metros (eje X local).
    f32    height = 2.0f;            ///< Alto en metros (eje Y local).
    int    resX = 16;               ///< Particulas a lo ancho (>=2, cap ~40).
    int    resY = 16;               ///< Particulas a lo alto (>=2, cap ~40).
    Anchor anchor = Anchor::TopEdge;
    f32    totalMass = 1.0f;         ///< Masa total repartida (kg).
    /// Rigidez [0,1]: 1 = tela firme (compliance 0), 0 = muy elastica. El
    /// ClothSystem la mapea a la compliance de los resortes de Jolt.
    f32    stiffness = 1.0f;
    /// Damping lineal del solver (Jolt default 0.1). Mas alto = la tela
    /// amortigua antes el movimiento (menos "latigazo").
    f32    damping = 0.1f;
    bool   useGravity = true;
    /// Color base de la tela (la luz direccional + ambiente lo modulan).
    glm::vec3 color{0.75f, 0.2f, 0.2f};  // rojo bandera por default

    // --- Runtime (NO se persiste) ---
    /// Handle del soft body en PhysicsWorld. 0 = no creado.
    u32  clothId = 0;
    /// Marker de re-materializacion (al editar params o spawnear). Sigue el
    /// patron F2H67 (`dirty=true` => destroy + recreate en el proximo tick).
    bool dirty = true;
    /// Buffer de render interleaved (pos.xyz, normal.xyz por vertice, con los
    /// triangulos ya expandidos en world space). Lo produce el ClothSystem
    /// cada frame desde la pose del soft body; lo consume el ClothRenderer
    /// (sube a un VBO dinamico + dibuja doble cara). 6 floats por vertice.
    std::vector<f32> renderVertices;
};

} // namespace Mood
