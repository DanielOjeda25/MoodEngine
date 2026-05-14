#pragma once

// QuestAsset (F2H53): asset .moodquest — definicion declarativa de una
// mision del juego. Lo consume el Quest Browser (autoria), el QuestSystem
// runtime (state machine + evaluacion de objectives) y el HUD widget
// tracker.
//
// Filosofia engine-grade (regla VISION): el motor NO conoce "main quest"
// vs "side quest" vs "tutorial" como categorias hardcoded. `category` es
// un string libre. El motor NO conoce "kill quest" / "fetch quest"
// hardcoded — los objectives son predicates declarativos sobre primitivas
// ya existentes (inventory, dialog vars) + un escape hatch Lua.
//
// El motor SI conoce 3 predicate types declarativos que cubren el ~80% de
// las quests de cualquier juego (Skyrim/Witcher/Zelda), todos consumiendo
// primitivas que YA existen:
//   - `collect`: inventory.count(item_path) >= min_quantity   (F2H52)
//   - `talk`:    dialog.has_var(var_name)                     (F2H48 dialog vars)
//   - `reach`:   dialog.has_var(var_name)                     (dev setea var
//                en script de Trigger; "reach" es semantico, mismo eval que talk)
// El escape hatch `custom_lua` cubre cualquier predicate complejo (combinacion
// de inventory + dialog + cualquier binding global expuesto).
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "id": "rescatar_gata",
//     "name_key": "quests.rescatar_gata.name",
//     "name_literal": "",
//     "description_key": "quests.rescatar_gata.desc",
//     "description_literal": "",
//     "category": "side",
//     "objectives": [
//       {
//         "id": "find_cat",
//         "name_literal": "Encontra a Mimi",
//         "description_literal": "Anda al bosque norte",
//         "type": "reach",
//         "var_name": "entered_forest"
//       },
//       {
//         "id": "bring_cat",
//         "name_literal": "Devuelvela a Marta",
//         "type": "collect",
//         "item_path": "items/cat.mooditem",
//         "min_quantity": 1
//       },
//       {
//         "id": "secret",
//         "name_literal": "???",
//         "type": "custom_lua",
//         "custom_predicate": "inventory.has('items/silver_key.mooditem')"
//       }
//     ],
//     "rewards": [
//       { "type": "item", "item_path": "items/gold.mooditem", "quantity": 50 },
//       { "type": "var",  "var_name": "marta_grateful", "var_value": "yes" },
//       { "type": "lua",  "lua_script": "player_xp = player_xp + 100" }
//     ]
//   }
//
// Convenciones (no-enforcement del motor):
// - `id` matchea el filename del .moodquest (sin extension). El editor
//   garantiza la invariante al crear/renombrar.
// - `objectives[].id` es local al quest (unico dentro del array). Sirve
//   para `quest.objective_complete("rescatar_gata", "find_cat")` desde Lua.
// - Campos type-specific se ignoran segun `type` (ej. `var_name` en un
//   objective `collect` no se evalua). Serializan vacios.
//
// Este modulo NO depende de ImGui ni Lua — testeable en mood_tests.

#include "core/Types.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood::Quest {

/// @brief Tipo del predicate de un objective. La evaluacion runtime
///        (QuestSystem) genera la expresion Lua a partir de los campos.
enum class ObjectiveType : u8 {
    /// inventory.count(item_path) >= min_quantity
    Collect   = 0,
    /// dialog.has_var(var_name) — semantica "hablar con NPC X"
    Talk      = 1,
    /// dialog.has_var(var_name) — semantica "llegar a zona Y"
    Reach     = 2,
    /// Expresion Lua arbitraria evaluada por el quest script host.
    CustomLua = 3,
};

/// @brief Tipo de la reward al completar el quest.
enum class RewardType : u8 {
    /// inventory.add(player, item_path, quantity)
    Item = 0,
    /// dialog.set_var(var_name, var_value)
    Var  = 1,
    /// Ejecuta script Lua arbitrario.
    Lua  = 2,
};

/// @brief Un objective (paso) del quest. El quest se completa cuando
///        TODOS los objectives estan satisfechos. Sin dependencias entre
///        objectives en v1 — todos se evaluan en paralelo (el dev puede
///        simular orden con dialog.vars: el objective 2 chequea una var
///        que solo se setea cuando el objective 1 esta "done").
struct Objective {
    std::string id;                       // local al quest (ej. "find_cat")
    std::string name_key;                 // i18n key (vacio = literal)
    std::string name_literal;             // fallback literal
    std::string description_literal;      // descripcion opcional

    ObjectiveType type = ObjectiveType::CustomLua;

    // Campos type-specific (no relevantes se ignoran):
    std::string item_path;        // Collect: ItemAsset path logico
    int         min_quantity = 1; // Collect
    std::string var_name;         // Talk / Reach: dialog.has_var(var_name)
    std::string custom_predicate; // CustomLua: expresion Lua arbitraria
};

/// @brief Una reward que se aplica al completar el quest.
struct Reward {
    RewardType type = RewardType::Item;

    // Campos type-specific (no relevantes se ignoran):
    std::string item_path;        // Item: ItemAsset path
    int         quantity = 1;     // Item
    std::string var_name;         // Var: dialog.set_var key
    std::string var_value;        // Var: dialog.set_var value
    std::string lua_script;       // Lua: script arbitrario
};

/// @brief Extension del filesystem para archivos de quest.
inline constexpr const char* k_fileExtension = ".moodquest";

/// @brief Asset declarativo del quest — datos puros, sin runtime.
class Asset {
public:
    Asset() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Campos (acceso directo — struct-like) -----

    std::string id;                          // ID estable (matchea filename)
    std::string name_key;                    // i18n key (vacio = literal)
    std::string name_literal;                // fallback literal
    std::string description_key;             // i18n key (vacio = literal)
    std::string description_literal;         // fallback literal
    std::string category;                    // libre (ej. "main", "side", "tutorial")

    std::vector<Objective> objectives;       // 1+ objectives; quest completo = todos done
    std::vector<Reward>    rewards;          // 0+ rewards al completar

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + todos los campos).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna asset vacio + loggea error.
    static Asset fromJson(const nlohmann::json& j);

    // ----- I/O de disco (F2H53 editor side) -----

    /// @brief Carga un asset desde un archivo `.moodquest`. Retorna nullopt
    ///        si el archivo no existe / no parsea / schema version
    ///        incompatible. Si el JSON no trae `id`, usa el filename
    ///        (sin extension) como id (mismo patron que ItemAsset).
    static std::optional<Asset> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el asset al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;
};

// ----- Helpers de serializacion de enums -----

/// @brief ObjectiveType -> string para JSON (legible y estable a reorden).
const char* objectiveTypeToString(ObjectiveType t);

/// @brief string -> ObjectiveType. Unknown / vacio -> CustomLua (safe default).
ObjectiveType objectiveTypeFromString(const std::string& s);

/// @brief RewardType -> string para JSON.
const char* rewardTypeToString(RewardType t);

/// @brief string -> RewardType. Unknown / vacio -> Lua (safe default — el
///        callback Lua puede no hacer nada si el script esta vacio).
RewardType rewardTypeFromString(const std::string& s);

} // namespace Mood::Quest
