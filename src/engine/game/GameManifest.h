#pragma once

// Manifest del juego empaquetado (`game.json` adyacente al
// MoodPlayer.exe). Hito 21 Bloque 4: indica que proyecto + mapa
// cargar al arrancar. El editor lo genera al "Empaquetar proyecto"
// (Bloque 5); el player lo lee en su ctor.
//
// Schema v1:
//   {
//     "version": 1,
//     "name": "Mi juego",
//     "project": "Project.moodproj",       // path relativo al manifest
//     "default_map": "maps/level1.moodmap"  // path relativo al .moodproj
//   }

#include <filesystem>
#include <optional>
#include <string>

namespace Mood {

struct GameManifest {
    int version = 1;
    std::string name;
    /// @brief Path al `.moodproj` relativo al directorio del manifest.
    std::filesystem::path projectRelative;
    /// @brief Path al `.moodmap` relativo al root del proyecto.
    std::filesystem::path defaultMapRelative;

    /// @brief Lee `manifestPath` (json). Devuelve nullopt si no existe,
    ///        no parsea, o le faltan campos minimos.
    static std::optional<GameManifest>
        loadFromFile(const std::filesystem::path& manifestPath);
};

} // namespace Mood
