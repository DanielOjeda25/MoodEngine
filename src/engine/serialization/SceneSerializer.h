#pragma once

// Serializacion de mapas (.moodmap) a/desde JSON. Las texturas y meshes se
// guardan por path logico (string) porque los asset ids son volatiles entre
// sesiones.
//
// Schema v2 (Hito 10):
//   {
//     "version": 2,
//     "name": "prueba_8x8",
//     "width": 8,
//     "height": 8,
//     "tileSize": 3.0,
//     "tiles": [
//       {"type": "solid_wall", "texture": "textures/grid.png"},
//       {"type": "empty",      "texture": "textures/missing.png"},
//       ...
//     ],
//     "entities": [   // nuevo en v2 — entidades no-tile spawneadas (meshes)
//       {
//         "tag": "Mesh_meshes/pyramid.obj",
//         "transform": {
//           "position":      [x, y, z],
//           "rotationEuler": [x, y, z],
//           "scale":         [x, y, z]
//         },
//         "mesh_renderer": {
//           "mesh_path": "meshes/pyramid.obj",
//           "materials": ["textures/brick.png", ...]
//         }
//       },
//       ...
//     ]
//   }
// `tiles` tiene `width*height` entradas en orden row-major (y*width + x).
// `entities` solo contiene entidades cuyo tag NO empieza con "Tile_" (esas
// se re-crean desde `GridMap` via `rebuildSceneFromMap`). Archivos v1 se
// leen igual (entities queda vacio).

#include "core/Types.h"
#include "engine/world/GridMap.h"

#include <glm/vec3.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class AssetManager;
class Scene;

/// @brief Copia persistida de un MeshRendererComponent para round-trip.
///        `meshPath` es el path logico que resuelve `AssetManager::loadMesh`;
///        fallback a missing (cubo) si el archivo no existe.
///        `materials` guarda un path logico por submesh; entradas vacias o
///        faltantes caen al slot 0 del AssetManager (missing.png).
struct SavedMeshRenderer {
    std::string meshPath;
    std::vector<std::string> materials;
};

/// @brief Copia persistida de un LightComponent (Hito 11).
///        `type` se guarda como string ("directional"/"point") para no atar
///        el archivo a la enumeracion del enum.
struct SavedLight {
    std::string type{"point"}; // "directional" | "point"
    glm::vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 radius = 10.0f;                    // solo Point
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // solo Directional
    bool enabled = true;
};

/// @brief Copia persistida de una entidad no-tile. Hito 10 agrego mesh
///        renderer; Hito 11 agrega light. Otros componentes (Script, Audio)
///        siguen sin persistirse (ver no-goals de cada hito).
struct SavedEntity {
    std::string tag;
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f};
    glm::vec3 scale{1.0f};
    std::optional<SavedMeshRenderer> meshRenderer;
    std::optional<SavedLight> light;
};

struct SavedMap {
    std::string name;
    GridMap map;
    std::vector<SavedEntity> entities;
};

class SceneSerializer {
public:
    /// @brief Guarda `map` + entidades no-tile de `scene` como JSON en `path`.
    ///        Las texturas/meshes se escriben como paths logicos via
    ///        `assets.pathOf(id)` / `assets.meshPathOf(id)`.
    ///        `scene` puede ser null (tests / flujos legacy que solo manejan
    ///        tiles): en ese caso `entities` queda vacio en el JSON.
    /// @throws std::runtime_error si no se puede abrir el archivo para escritura.
    static void save(const GridMap& map, const std::string& name,
                     const Scene* scene, const AssetManager& assets,
                     const std::filesystem::path& path);

    /// @brief Carga un mapa desde JSON. Las texturas y meshes se reabren via
    ///        `assets.loadTexture(...)` / `assets.loadMesh(...)` — si fallan,
    ///        caen al missing.
    /// @return `nullopt` si el archivo no existe, esta mal formado, o la
    ///         version declarada supera la soportada. Loguea el motivo en
    ///         el canal `assets`.
    static std::optional<SavedMap> load(const std::filesystem::path& path,
                                        AssetManager& assets);
};

} // namespace Mood
