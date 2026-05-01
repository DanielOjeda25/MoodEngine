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
    u32 createBody(const glm::vec3& position,
                   CollisionShape shape,
                   const glm::vec3& halfExtents,
                   BodyType type,
                   f32 mass = 1.0f);

    /// @brief Destruye y remueve un body. Llamar cuando la entidad se borra
    ///        del Scene. Idempotente (id invalido = no-op).
    void destroyBody(u32 bodyId);

    /// @brief Lee la posicion actual del body. Devuelve (0,0,0) si id invalido.
    glm::vec3 bodyPosition(u32 bodyId) const;

    /// @brief Setea la posicion del body (teleport). Util para Kinematic o
    ///        para setear pose inicial de Dynamic antes de activarlo.
    void setBodyPosition(u32 bodyId, const glm::vec3& position);

    /// @brief Aplica una fuerza al body este frame (Dynamic). En Newton.
    void addForce(u32 bodyId, const glm::vec3& force);

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
    RaycastHit raycast(const glm::vec3& origin,
                       const glm::vec3& direction,
                       f32 maxDistance) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mood
