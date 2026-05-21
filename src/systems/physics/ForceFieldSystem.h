#pragma once

// ForceFieldSystem (F2H72): aplica fuerzas fisicas a los RigidBody Dynamic
// que caen dentro de un ForceFieldComponent. Cada frame en Play, por cada
// zona, junta los bodies dynamic adentro y les agrega una fuerza:
//   - Directional: fuerza constante en `direction` (viento).
//   - Radial: fuerza a lo largo de centro->body (explosion si strength>0,
//     atractor si <0), con falloff lineal opcional.
//
// Reusa el patron de overlap del TriggerSystem (OBB para Box / distancia
// para Sphere). Sin colision solida — la zona no frena a los objetos, solo
// los empuja. Debe correr ANTES del step de fisica (las fuerzas que Jolt
// acumula se aplican en el proximo Update y se limpian despues).

#include "core/Types.h"

namespace Mood {

class PhysicsWorld;
class Scene;

class ForceFieldSystem {
public:
    /// @param scene   Scene con entidades ForceFieldComponent + los bodies.
    /// @param physics Para leer posiciones y aplicar `addForce`.
    /// @param dt      Delta time del frame (reservado para futuras zonas
    ///                dependientes del tiempo; v1 usa fuerza continua).
    void update(Scene& scene, PhysicsWorld& physics, f32 dt);
};

} // namespace Mood
