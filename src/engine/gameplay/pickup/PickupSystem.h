#pragma once

// F4H4 — Pickup system. Engine-generic: el sistema busca entities con
// PickupComponent + TransformComponent en la escena, las anima (spin + bob),
// detecta overlap con el player (tag "player") por distancia plana, y
// aplica el payload segun PickupType. Marca `consumed=true` al recoger; el
// proximo tick destruye la entity.

#include "core/Types.h"

namespace Mood {

class Scene;
class Entity;
class AssetManager;

namespace Pickup {

/// @brief Tick del sistema. Llamado cada frame en Play mode. Recorre
///        entities con PickupComponent, actualiza spin+bob del Transform,
///        chequea overlap con player y aplica payload. Cleanup de
///        `consumed=true` borra la entity al final del tick.
///
///        Si `playerEntity` es null o no tiene Transform, el tick sigue
///        animando los pickups pero no aplica payload (no hay con quien
///        hacer overlap). Logueo via canal `engine`.
///
///        `assets` es opcional — solo se usa para resolver `weaponPath` al
///        recoger un pickup de tipo Weapon. Si es nullptr, los pickups
///        Weapon loggean warn y NO se consumen.
void tickSystem(Scene& scene, f32 dt, Entity playerEntity, AssetManager* assets);

} // namespace Pickup
} // namespace Mood
