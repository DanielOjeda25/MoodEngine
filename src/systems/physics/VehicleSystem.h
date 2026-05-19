#pragma once

// F2H67 Bloque D: sistema que coordina los `VehicleComponent` con el
// `PhysicsWorld`. Materializa el chassis + constraint al `dirty=true`,
// pushea el input del componente cada frame y sincroniza la pose del
// chassis + las 4 child-entities wheel post-step.
//
// Hookeado en `EditorScene::tickPhysics` (1 invocacion DESPUES de
// `PhysicsWorld::step()`). Vive aislado para mantener `EditorScene.cpp`
// bajo soft cap, mismo patron que RagdollSystem (F2H66 D).
//
// Convencion del .moodmap para las wheels (v1):
//   - Chassis es UNA entity con `VehicleComponent`.
//   - Las 4 ruedas se declaran como entities aparte con tags fijos:
//       "Wheel_FL", "Wheel_FR", "Wheel_RL", "Wheel_RR".
//   - El primer tick del system las busca por tag y cachea sus handles
//     en `VehicleComponent.wheelEntities[4]`. Si alguna falta, el system
//     skipea el sync visual de esa wheel pero el physics sigue corriendo.
//   - Si la entity chassis tiene `parent_id`, las wheels se buscan en
//     scope global -- los tags Wheel_* deben ser unicos en el mapa.

namespace Mood {

class AssetManager;
class PhysicsWorld;
class Scene;

namespace VehicleSystem {

/// @brief Tick del sistema. Llamar DESPUES de `PhysicsWorld::step()` para
///        que `readVehicleState` devuelva pose post-step.
///
/// @param scene         Escena con entidades.
/// @param physicsWorld  Backend Jolt (createVehicle / setInput / read).
/// @param assets        Para futuras lookups de `.moodvehicle` (Bloque G).
///                      En Bloque D v1 se ignora si `configPath` esta
///                      vacio -- usamos `makeDefaultSA()` como fallback.
void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets);

} // namespace VehicleSystem
} // namespace Mood
