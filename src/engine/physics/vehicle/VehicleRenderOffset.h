#pragma once

// Helper de render-offset Y para una entity vehicle. Extraido de
// `systems/physics/VehicleSystem.h` en break-B1 para que el SceneLoader
// (engine/) no necesite incluir systems/. Pura utilidad de mesh-analysis
// + spring-rest: no toca fisica ni runtime, asi que vive bien en
// engine/physics/.

#include "core/Types.h"

namespace Mood {

class AssetManager;
class Entity;

namespace vehicle {

/// @brief F2H70 Bloque A: offset Y de auto-spawn-height para una entity
///        vehicle. Calcula `-aabbMin.y` del MeshAsset asociado (via
///        MeshRendererComponent) para que el bottom del modelo quede en
///        `TC.position.y` independiente de la convencion de origin del
///        modelo (base, centro vertical, etc.). Sumado al spring rest
///        compression (F2H70.2 B) para compensar el settle al spawn.
///        Devuelve 0 si la entity no tiene MeshRendererComponent / mesh
///        invalido / sin VehicleComponent.
f32 chassisRenderYOffset(Entity e, AssetManager& assets);

} // namespace vehicle
} // namespace Mood
