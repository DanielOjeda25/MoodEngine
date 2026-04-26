#pragma once

// Serializacion de prefabs (Hito 14). Un `.moodprefab` es un asset reusable
// con la "plantilla" de una entidad — sus componentes en el momento del
// guardado — instanciable multiple veces en distintas escenas.
//
// Schema v1:
//   {
//     "version": 1,
//     "name": "torch",                  // opcional, info display
//     "root": <SavedEntity JSON>,
//     "children": [<SavedEntity>...]    // depth-first preorder. Cada
//                                       // child tiene "parentIndex":
//                                       // u32 relativo al array (-1 = root).
//   }
//
// Hito 14 NO soporta jerarquia padre-hijo real (TransformComponent no tiene
// `parent`). El campo `children` queda en el schema para futuro pero los
// prefabs guardados desde el editor producen `children: []`.
//
// El serializer usa los helpers del `EntitySerializer.{h,cpp}` para mapear
// una entidad a/desde JSON, asi que los componentes soportados en prefabs
// son los mismos que en `.moodmap`: Mesh / Light / RigidBody (+ link path).

#include "core/Types.h"
#include "engine/serialization/SceneSerializer.h" // SavedEntity

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class AssetManager;
class Entity;

constexpr int k_MoodprefabFormatVersion = 1;

struct SavedPrefab {
    std::string name;             // opcional, derivado del nombre de archivo
    SavedEntity root;
    std::vector<SavedEntity> children; // sin uso en Hito 14; reservado
};

class PrefabSerializer {
public:
    /// @brief Serializa una entidad como `.moodprefab` en `path`. Persiste
    ///        TagComponent + TransformComponent + cualquier subset de
    ///        {MeshRenderer, Light, RigidBody, PrefabLink}. La entidad se
    ///        considera "root"; sin children por ahora.
    /// @param name Nombre lógico del prefab. Se escribe en el JSON. Si vacío,
    ///             se deja sin "name".
    /// @throws std::runtime_error si no se puede abrir el archivo.
    static void save(Entity entity, const std::string& name,
                     const AssetManager& assets,
                     const std::filesystem::path& path);

    /// @brief Carga un `.moodprefab` desde disco. Devuelve nullopt si el
    ///        archivo no existe / esta corrupto / version > soportada.
    static std::optional<SavedPrefab> load(const std::filesystem::path& path);
};

} // namespace Mood
