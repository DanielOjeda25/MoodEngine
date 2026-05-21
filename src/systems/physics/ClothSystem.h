#pragma once

// F2H75 Bloque D: sistema de telas (cloth). Materializa lazy el soft body
// desde el `ClothComponent` (dirty -> createCloth), y cada frame lee la pose
// de las particulas de Jolt para producir el buffer de render interleaved
// (`ClothComponent::renderVertices`) que el `OpenGLClothRenderer` dibuja.
//
// Patron espejo del VehicleSystem: `tick` corre en Play (post physics step);
// `previewRest` corre en Editor (sin Play) para mostrar la tela plana en su
// pose de reposo sin necesidad de simular.

namespace Mood {

class Scene;
class PhysicsWorld;

namespace ClothSystem {

/// @brief Play mode. Materializa la tela si `dirty` (createCloth) y luego
///        lee las posiciones del soft body -> recalcula normales -> escribe
///        el buffer interleaved en `ClothComponent::renderVertices`. Llamar
///        DESPUES del physics step.
void tick(Scene& scene, PhysicsWorld& physicsWorld);

/// @brief Editor mode (sin Play). Produce el buffer de render desde la pose
///        de reposo analitica (grilla plana transformada por el Transform),
///        sin tocar la fisica — asi la tela se ve en el viewport del editor.
void previewRest(Scene& scene);

} // namespace ClothSystem
} // namespace Mood
