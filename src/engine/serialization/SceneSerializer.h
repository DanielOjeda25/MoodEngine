#pragma once

// Serializacion de mapas (.moodmap) a/desde JSON. Las texturas se guardan
// por path logico (string) porque los TextureAssetId son volatiles entre
// sesiones.
//
// Schema (ver k_MoodmapFormatVersion en JsonHelpers.h):
//   {
//     "version": 1,
//     "name": "prueba_8x8",
//     "width": 8,
//     "height": 8,
//     "tileSize": 3.0,
//     "tiles": [
//       {"type": "solid_wall", "texture": "textures/grid.png"},
//       {"type": "empty",      "texture": "textures/missing.png"},
//       ...
//     ]
//   }
// `tiles` tiene `width*height` entradas en orden row-major (y*width + x).

#include "engine/world/GridMap.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Mood {

class AssetManager;

struct SavedMap {
    std::string name;
    GridMap map;
};

class SceneSerializer {
public:
    /// @brief Guarda `map` como JSON en `path`. Las texturas se escriben como
    ///        path logico resuelto via `assets.pathOf(id)`.
    /// @throws std::runtime_error si no se puede abrir el archivo para escritura.
    static void save(const GridMap& map, const std::string& name,
                     const AssetManager& assets,
                     const std::filesystem::path& path);

    /// @brief Carga un mapa desde JSON. Las texturas se reabren via
    ///        `assets.loadTexture(logicalPath)` — si fallan, caen al missing.
    /// @return `nullopt` si el archivo no existe, esta mal formado, o la
    ///         version declarada supera la soportada. Loguea el motivo en
    ///         el canal `assets`.
    static std::optional<SavedMap> load(const std::filesystem::path& path,
                                        AssetManager& assets);
};

} // namespace Mood
