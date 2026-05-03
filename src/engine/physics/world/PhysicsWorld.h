#pragma once

// Wrapper RAII sobre Jolt Physics (Hito 12). Encapsula todo el boilerplate
// de inicializacion (Factory, alocadores, layer interfaces, JobSystem) y
// expone una API compacta para crear/destruir bodies y stepear la sim.
//
// Convencion de layers:
//   - ObjectLayer::Moving  -> bodies dinamicos (player, props, proyectiles)
//   - ObjectLayer::Static  -> geometria del mapa (tiles, suelo, paredes)
//   Matriz: Moving colisiona con Moving + Static. Static no se testea
//   contra Static (optimizacion estandar de engines).
//
// La API usa tipos de Jolt directo (JPH::BodyID, JPH::RVec3, etc.) porque
// ocultarlos detras de fachada propia en este hito duplicaria código sin
// beneficio — el consumidor es `PhysicsSystem` del motor, no gameplay code.

#include "core/Types.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>  // Hito 41 C: setBodyPositionRot quat

#include <memory>

// Forward decls para no arrastrar headers de Jolt en este .h.
namespace JPH {
class PhysicsSystem;
class TempAllocator;
class JobSystem;
class BodyInterface;
class Shape;
class BodyID;
}

namespace Mood {

/// @brief Layers de la simulacion. 8-bit por Jolt — alcanzan.
///
/// Hito 40 C (decision permanente): mantenemos solo Static + Moving.
/// Para filtros de gameplay tipo "ignore enemies" o "solo pickups", el
/// patron es:
///   1. `TagComponent` para identificar el rol (ej. "enemy_grunt").
///   2. `layerMask` del raycast (Hito 39 C) para Static vs Moving binario.
///   3. Lua post-filtra el `bodyId` del hit consultando la entidad
///      asociada (futuro: helper `engine.entity_from_body(id)`).
/// Extender este enum cambia la matrix de colision Jolt y NO emerge
/// como necesario en los demos previstos. Si en Fase 2 aparece, agregar
/// aqui + actualizar `ObjectLayerPairFilterMood` + serializar layer
/// per-body.
enum class ObjectLayer : u8 {
    Static = 0, // world geometry (tiles, immovable props)
    Moving = 1, // dynamic bodies + character
    Count  = 2,
};

/// @brief Tipo de rigid body. Mapea a `JPH::EMotionType`.
enum class BodyType : u8 {
    Static    = 0, // no se mueve; parte del mapa
    Kinematic = 1, // se mueve por codigo (no responde a fuerzas)
    Dynamic   = 2, // simulado por Jolt (gravedad, colisiones)
};

/// @brief Shape primitivo soportado. MeshFromAsset queda para hitos
///        posteriores (convex decomposition o raw mesh collider).
enum class CollisionShape : u8 {
    Box     = 0,
    Sphere  = 1,
    Capsule = 2,
};

class PhysicsWorld {
public:
    /// @brief Inicializa Jolt (Factory + allocator global + layer interfaces
    ///        + JobSystem + PhysicsSystem). Loguea al canal `physics`. Lanza
    ///        si falla la inicializacion.
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /// @brief Avanza la simulacion `dt` segundos. `collisionSteps` es el
    ///        numero de sub-steps (default 1 para 60fps; 2 para mayor
    ///        estabilidad a bajo framerate).
    void step(f32 dt, int collisionSteps = 1);

    /// @brief Crea un rigid body y lo agrega al mundo. Devuelve la BodyID
    ///        (numerica, estable durante la vida del body).
    /// @param position        Posicion en world coords al crear.
    /// @param shape           Primitivo de colision.
    /// @param halfExtents     Para Box: half-extents (x, y, z). Para Sphere: (radius, _, _).
    ///                        Para Capsule: (halfHeight, radius, _).
    /// @param type            Static / Kinematic / Dynamic.
    /// @param mass            Solo para Dynamic. Static/Kinematic lo ignoran.
    /// @param friction        Hito 34 A. Coef. friction Jolt [0, +inf]. Defaults a
    ///                        0.5 (madera-sobre-madera). Aplica a Static + Dynamic.
    /// @param rotationQuat    Hito 41 fix-up #2: rotacion inicial del body
    ///                        como quat (X, Y, Z, W). Default identidad.
    ///                        Critico para Load Game: la materializacion
    ///                        post-load del body Dynamic preserva la
    ///                        rotation del Transform (antes el body
    ///                        arrancaba "derecho" aunque el save lo tuviera
    ///                        rotado).
    u32 createBody(const glm::vec3& position,
                   CollisionShape shape,
                   const glm::vec3& halfExtents,
                   BodyType type,
                   f32 mass = 1.0f,
                   f32 friction = 0.5f,
                   const glm::vec4& rotationQuat = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    /// @brief Destruye y remueve un body. Llamar cuando la entidad se borra
    ///        del Scene. Idempotente (id invalido = no-op).
    void destroyBody(u32 bodyId);

    /// @brief Lee la posicion actual del body. Devuelve (0,0,0) si id invalido.
    glm::vec3 bodyPosition(u32 bodyId) const;

    /// @brief Setea la posicion del body (teleport). Util para Kinematic o
    ///        para setear pose inicial de Dynamic antes de activarlo.
    void setBodyPosition(u32 bodyId, const glm::vec3& position);

    /// @brief Hito 39 D: cambia el coef de friction de un body en runtime.
    ///        Aplica al `BodyInterface` directo y reactiva el body para
    ///        que las contactos vigentes recalculen friccion. Idempotente
    ///        para bodyId invalidos.
    void setBodyFriction(u32 bodyId, f32 friction);

    /// @brief Hito 40 D: cambia la masa de un Dynamic body en runtime.
    ///        Re-computa las propiedades de inercia desde el shape
    ///        actual (asume distribucion uniforme). No-op para Static /
    ///        Kinematic. Idempotente para bodyId invalidos.
    void setBodyMass(u32 bodyId, f32 mass);

    /// @brief Hito 40 D: cambia los halfExtents (shape) de un body en
    ///        runtime. Implementa via SetShape de Jolt — barato cuando
    ///        las extents son chicas; al recrear, posicion + velocidad
    ///        se preservan, contacts se invalidan. No-op si bodyId
    ///        invalido o shape no es Box/Sphere/Capsule (los soportados
    ///        por `createBody`).
    void setBodyHalfExtents(u32 bodyId, CollisionShape shape,
                              const glm::vec3& halfExtents);

    /// @brief Aplica una fuerza al body este frame (Dynamic). En Newton.
    void addForce(u32 bodyId, const glm::vec3& force);

    /// @brief Hito 41 C: getter/setter de velocidades para snapshot
    ///        Save/Load. Sin lock — Jolt expone API thread-safe.
    glm::vec3 bodyLinearVelocity(u32 bodyId) const;
    glm::vec3 bodyAngularVelocity(u32 bodyId) const;
    void setBodyLinearVelocity(u32 bodyId, const glm::vec3& v);
    void setBodyAngularVelocity(u32 bodyId, const glm::vec3& w);

    /// @brief Hito 41 A: getter/setter combinado de pose. Position
    ///        + rotation como quaternion (X, Y, Z, W). Save/Load lo
    ///        usa para restaurar bodies dynamic con su orientation
    ///        post-fisica.
    glm::vec3 bodyPositionRot(u32 bodyId, glm::vec4& outQuatXYZW) const;
    void setBodyPositionRot(u32 bodyId, const glm::vec3& pos,
                              const glm::vec4& quatXYZW);

    /// @brief Aplica un impulso instantaneo (no acumula por dt).
    void addImpulse(u32 bodyId, const glm::vec3& impulse);

    /// @brief Cantidad de bodies activos. Util para tests / debug.
    u32 bodyCount() const;

    // --- Hito 30: Character Controller (CharacterVirtual) ---

    /// @brief Crea un character controller (capsule kinematic-style) en
    ///        `initialPos`. La capsule se compone de un cilindro central
    ///        de altura `2 * cylinderHalfHeight` mas dos hemiesferas de
    ///        `radius`. Altura total visual ≈ 2*(cylinderHalfHeight + radius).
    ///        Devuelve un handle estable; 0 si fallo.
    u32 createCharacter(const glm::vec3& initialPos,
                         f32 cylinderHalfHeight,
                         f32 radius);

    /// @brief Destruye y limpia el character. Idempotente (handle invalido no-op).
    void destroyCharacter(u32 charId);

    /// @brief Setea la velocidad lineal deseada (m/s) que el character
    ///        intenta aplicar en el proximo `step(dt)`. El propio character
    ///        resuelve slide contra la geometria estatica + dynamic.
    void setCharacterMovement(u32 charId, const glm::vec3& desiredVelocity);

    /// @brief Posicion world-space del centro de la capsule (no de la base).
    glm::vec3 characterPosition(u32 charId) const;

    /// @brief Teleport (resetea la velocidad interna).
    void setCharacterPosition(u32 charId, const glm::vec3& position);

    /// @brief True si el character esta sobre suelo (slope < max_slope).
    ///        Util para gating de salto y reset de gravedad acumulada.
    bool isCharacterOnGround(u32 charId) const;

    /// @brief Cambia la shape del character (capsule). Util para crouch:
    ///        del default (cylinderHalfHeight 0.5, radius 0.4) a
    ///        crouching (e.g. 0.1, 0.4 -> total ~1.0m). Devuelve `true`
    ///        si Jolt acepto el cambio sin penetracion (no hay techo
    ///        bloqueando). Si falla (techo bajo), el character mantiene
    ///        su shape anterior y el caller debe seguir crouched hasta
    ///        que se libere espacio.
    bool setCharacterShape(u32 charId,
                            f32 cylinderHalfHeight,
                            f32 radius);

    /// @brief Cantidad de characters activos.
    u32 characterCount() const;

    // --- Hito 33: Raycast (NarrowPhase de Jolt) ---

    /// Resultado de un `raycast`. `hit==false` indica que el rayo no
    /// intersectó nada en `[0, maxDistance]`.
    struct RaycastHit {
        bool      hit       = false;
        glm::vec3 point     {0.0f};   // world-space del impacto
        glm::vec3 normal    {0.0f};   // world-space, apunta hacia origin
        f32       distance  = 0.0f;   // [0, maxDistance]
        u32       bodyId    = 0;      // BodyID del cuerpo impactado (0 si miss)
    };

    /// @brief Lanza un rayo desde `origin` en direccion `direction` (no
    ///        necesariamente normalizada — internamente lo escalamos por
    ///        `maxDistance`). Devuelve el primer impacto contra cualquier
    ///        body (Static, Dynamic, Kinematic). Characters virtuales NO
    ///        son detectables por raycast (Jolt los considera "ghost" para
    ///        narrow phase queries — mismo comportamiento que el motor del
    ///        Hito 30).
    /// @param maxDistance En metros. Mantener acotado (ej. 100m) por
    ///        rendimiento — Jolt cobra O(log N) por bvh pero igual paga el
    ///        narrow phase test contra cada body candidato.
    /// @param ignoredBodyId Hito 34 B. Si != 0, ese body no se considera
    ///        para el cast (ni cuenta como hit ni ocluye al body que esta
    ///        detras). Util para "ignore self" en armas FPS desde un
    ///        body que no quiere autodetectarse.
    /// @param layerMask Hito 39 C. Bitfield aplicado al `ObjectLayer` del
    ///        body candidato: bit 0 = Static, bit 1 = Moving. Default
    ///        `0xFFFFFFFFu` = todos. Ej. `1u` solo paredes/piso, `2u`
    ///        solo Dynamic+Kinematic, `3u` ambos (== default).
    RaycastHit raycast(const glm::vec3& origin,
                       const glm::vec3& direction,
                       f32 maxDistance,
                       u32 ignoredBodyId = 0,
                       u32 layerMask = 0xFFFFFFFFu) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mood
