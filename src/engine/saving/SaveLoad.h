#pragma once

// Save/Load del gameplay state — Hito 38 A.
//
// `.moodsave` es un JSON aparte del `.moodmap`:
//   - El `.moodmap` es estatico (level design): paredes, mesh renderers,
//     scripts asignados, parametros del particle emitter, etc. NO cambia
//     durante el juego.
//   - El `.moodsave` es runtime (gameplay state): hp/ammo del HUD, pose
//     del player char, yaw/pitch de la camara FPS. Cambia frame a frame.
//
// Schema v1:
//   {
//     "version": 1,
//     "map_path": "maps/level1.moodmap",
//     "hud": { "hp": 75, "ammo": 22 },
//     "player": {
//       "position": [x, y, z],
//       "yaw":   45.0,
//       "pitch": -10.0
//     }
//   }
//
// V1 NO persiste:
//   - Snapshots de Dynamic bodies (Jolt body pose + linear/angular vel).
//     Los Dynamic respawnean en su Transform original al cargar el map.
//   - Globals Lua (variables top-level del script). El dev puede hacerlas
//     "exposed" via `engine.exposed` y se persisten en el `.moodmap` —
//     reusar ese mecanismo para state persistente entre saves implicaria
//     bumpear el `.moodmap` en cada save, fuera de scope para v1.
//
// Convenciones:
//   - Paths con `/` (forward slashes), sin Windows backslashes.
//   - Floats con precision por default de nlohmann::json (suficiente).
//   - El loader es defensivo: campos faltantes caen a defaults sin lanzar.

#include "core/Types.h"
#include "engine/game/GameState.h"

#include <glm/vec3.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace Mood::SaveLoad {

struct SaveData {
    std::string mapPath;        // path logico al `.moodmap` activo
    HudState    hud;
    glm::vec3   playerPosition{0.0f};
    f32         playerYaw   = -90.0f;
    f32         playerPitch = 0.0f;
};

/// @brief Escribe `d` como JSON en `path`. Crea directorios padre si
///        no existen. Retorna true en exito; false (con log al canal
///        engine) si falla I/O o serializacion.
bool save(const SaveData& d, const std::filesystem::path& path);

/// @brief Lee un `.moodsave` JSON. Retorna `nullopt` si el archivo no
///        existe, esta mal formado, o la version supera la soportada.
///        Loguea el motivo en el canal engine.
std::optional<SaveData> load(const std::filesystem::path& path);

} // namespace Mood::SaveLoad
