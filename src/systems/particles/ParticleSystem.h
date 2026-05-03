#pragma once

// ParticleSystem (Hito 29 Bloque 1): avanza la simulacion CPU de cada
// `ParticleEmitterComponent` de la `Scene`.
//
// Por entidad con emisor:
//   1) Lazy alloc de la pool si `positions.empty()` (se hace una sola vez).
//   2) Para cada particula viva: age += dt; pos += vel*dt; vel.y +=
//      -9.81 * gravityFactor * dt. Si age >= lifetime, marca alive=0.
//   3) Si emitter.emitting: emitAccumulator += emitRate * dt; spawnea
//      floor(emitAccumulator) particulas reciclando slots libres.
//
// Nota sobre worldOrigin: las particulas se spawnean en la posicion world
// del `TransformComponent` al momento del spawn — no quedan attached al
// emisor (un humo que sigue una entidad en movimiento se ve mas natural
// asi). Si en el futuro se quiere "particulas attached", agregar un flag
// `localSpace` al componente.

#include "core/Types.h"

namespace Mood {

class Scene;

class ParticleSystem {
public:
    /// @brief Avanza simulacion `dt` segundos sobre todos los emisores
    ///        de la escena. Llamar una vez por frame (Editor + Player).
    void update(Scene& scene, f32 dt);
};

} // namespace Mood
