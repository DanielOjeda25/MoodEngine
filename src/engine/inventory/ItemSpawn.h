#pragma once

// ItemSpawn (F2H52 Bloque H5): helper para crear pickup-entities en el
// mundo desde codigo C++. Comparte logica entre:
//   - `inventory.spawn_pickup(...)` (Lua binding del Bloque E)
//   - HUD widget right-click "Tirar" (Bloque H5)
//   - Drag-drop del Item Browser al viewport (editor — Bloque D)
//
// La entity resultante tiene Transform + MeshRenderer + Trigger 0.5x0.5x0.5
// + ItemPickupComponent, igual que si el dev hubiese arrastrado el item al
// viewport desde el Item Browser. Mesh:
//   - `model_path` del item -> loadMesh + createMaterialsForMesh.
//   - sino `icon_path` -> cubo + textura icon como albedo.
//   - sino cubo + materiales default (placeholder magenta).
//
// Engine-grade: el helper NO conoce semantica game-specific. El dev
// decide cuando spawnear (al matar enemigo, al abrir cofre, al "Tirar"
// desde el HUD, etc.).

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

class Scene;
class AssetManager;

namespace Inventory {

/// @brief Crea una entity-pickup en el mundo en la posicion dada con
///        el item indicado. Mismo layout que el drag-drop del editor.
///        Falla (retorna false con warn al log) si:
///        - `scene` o `assets` son nullptr,
///        - el item no existe en el AssetManager.
/// @param itemPath   Path logico del item (ej. "items/iron_sword.mooditem").
/// @param worldPos   Posicion world-space donde aparecera el pickup.
/// @param quantity   Cantidad que se agrega al inventario al levantarlo.
/// @return true si la entity se creo correctamente.
bool spawnPickupInWorld(Scene* scene,
                         AssetManager* assets,
                         const std::string& itemPath,
                         const glm::vec3& worldPos,
                         int quantity = 1);

} // namespace Inventory
} // namespace Mood
