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
// Schema v2 (Hito 41): agrega `bodies` (Dynamic body snapshots) y
// `script_globals` (Lua globals filtradas). Loader v2 sigue leyendo
// archivos v1 (los nuevos campos quedan vacios).
//
// Convenciones:
//   - Paths con `/` (forward slashes), sin Windows backslashes.
//   - Floats con precision por default de nlohmann::json (suficiente).
//   - El loader es defensivo: campos faltantes caen a defaults sin lanzar.

#include "core/Types.h"
#include "engine/game/GameState.h"
#include "engine/scripting/ExposedProperty.h"  // ExposedValue (Hito 41 B)

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mood::SaveLoad {

/// @brief Hito 41 A: snapshot de un Dynamic body. Se identifica por
///        el `entityTag` (estable entre sesiones); el bodyId raw de
///        Jolt es volatil. Pose + velocidades = todo el state runtime
///        del body que el motor reconstruye al load (Static/Kinematic
///        no se snapshootan — su pose es derivada del Transform del
///        `.moodmap`).
struct BodySnapshot {
    std::string entityTag;
    glm::vec3   position{0.0f};
    glm::vec4   rotationQuat{0.0f, 0.0f, 0.0f, 1.0f}; // (x, y, z, w)
    glm::vec3   linearVelocity{0.0f};
    glm::vec3   angularVelocity{0.0f};
};

/// @brief Hito 41 B: snapshot de globals Lua de un script. Solo tipos
///        primitivos (number/bool/string/vec3) — funciones, tables,
///        userdata se omiten silencioso.
struct ScriptGlobalsSnapshot {
    std::string scriptPath;
    /// Mapa name → ExposedValue. Reusamos el variant del Hito 24
    /// para no duplicar tipos.
    std::unordered_map<std::string, ExposedValue> globals;
};

struct SaveData {
    std::string mapPath;        // path logico al `.moodmap` activo
    HudState    hud;
    glm::vec3   playerPosition{0.0f};
    f32         playerYaw   = -90.0f;
    f32         playerPitch = 0.0f;
    /// Hito 41 A: vector de snapshots Dynamic body (identificados por tag).
    std::vector<BodySnapshot> bodies;
    /// Hito 41 B: globals Lua filtradas por script.
    std::vector<ScriptGlobalsSnapshot> scriptGlobals;
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
