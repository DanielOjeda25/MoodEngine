#pragma once

// Helpers compartidos para serializar/deserializar `SavedEntity` <-> JSON.
// Extraidos del `SceneSerializer` (Hito 10/11/12) para que el
// `PrefabSerializer` (Hito 14) los reuse sin duplicar la logica de mapeo
// componente -> JSON.
//
// Schema de cada entidad (consistente entre `.moodmap` y `.moodprefab`):
//   {
//     "tag": "...",
//     "transform": {"position": [..], "rotationEuler": [..], "scale": [..]},
//     "mesh_renderer": { ... }   // opcional
//     "light": { ... }           // opcional
//     "rigid_body": { ... }      // opcional
//     "prefab_path": "..."       // opcional, link al prefab del que se
//                                //   instancio (Hito 14 Bloque 6).
//   }

#include "engine/serialization/SceneSerializer.h" // SavedEntity y subtipos

#include <nlohmann/json.hpp>

namespace Mood {

class AssetManager;
class Entity;

/// @brief Serializa una entidad de la Scene a JSON. Lee Tag, Transform y
///        cualquier subset de {MeshRenderer, Light, RigidBody}. Si la
///        entidad tiene un `prefabPath` registrado, tambien lo escribe.
///        El caller decide cuando la incluye en `.moodmap` (filtro de tile)
///        o en `.moodprefab` (root + children).
nlohmann::json serializeEntityToJson(Entity entity, const AssetManager& assets);

/// @brief Parsea un objeto JSON con el schema de arriba a `SavedEntity`.
///        Campos faltantes caen a defaults; nunca lanza por campos
///        opcionales ausentes.
SavedEntity parseEntityFromJson(const nlohmann::json& j);

} // namespace Mood
