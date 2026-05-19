#pragma once

// F2H66 Bloque D: sistema que coordina los `RagdollComponent` con el
// `PhysicsWorld`. Detecta transiciones Animated->Ragdolling, construye el
// ragdoll desde el skeleton del mesh (auto-Mixamo) y luego cada frame lee
// las poses fisicas y las escribe en `SkeletonComponent.skinningMatrices`
// — el mesh se renderiza siguiendo el ragdoll en vez de la animacion.
//
// Convencion HL2 / Source: una vez Ragdolling, no se vuelve a Animated.
// El AnimatorComponent queda pausado (`playing=false`) tras la transicion.
//
// Hookeado en `EditorScene::tickPhysics` (1 sola invocacion). El sistema
// vive aislado para mantener `EditorScene.cpp` bajo soft cap.

namespace Mood {

class AssetManager;
class PhysicsWorld;
class Scene;

namespace RagdollSystem {

/// @brief Tick del sistema: maneja transicion + sync ragdoll->skeleton.
///        Llamar DESPUES de `PhysicsWorld::step()` para que la pose leida
///        ya este post-step.
///
/// @param scene         Escena con entidades. Procesa todas las que tengan
///                      `RagdollComponent` + `AnimatorComponent` +
///                      `SkeletonComponent` + `MeshRendererComponent`.
/// @param physicsWorld  Backend Jolt. Crea / destruye / consulta ragdolls.
/// @param assets        Para resolver `MeshAssetId` -> `MeshAsset*`
///                      (skeleton del mesh).
void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets);

} // namespace RagdollSystem
} // namespace Mood
