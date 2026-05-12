#pragma once

// ItemAsset (F2H51): asset .mooditem — definicion declarativa de un
// item del juego. Lo consume el Item Browser (autoria), el
// InventoryComponent (instancias en mundo) y eventualmente el HUD
// widget de inventario + bindings Lua (F2H52).
//
// Filosofia engine-grade: el motor NO conoce "weapon" / "potion" /
// "armor" como categorias hardcoded. Solo tags (strings libres) +
// stats key-value libre. El dev del juego define su propia ontologia.
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "id": "iron_sword",
//     "name_key": "items.iron_sword.name",
//     "name_literal": "",
//     "description_key": "items.iron_sword.desc",
//     "description_literal": "",
//     "icon_path": "icons/items/iron_sword.png",
//     "model_path": "",
//     "tags": ["weapon", "metal", "common"],
//     "stats": { "damage": 12.5, "weight": 3.0 },
//     "stack": { "stackable": false, "max_stack": 1 },
//     "slot_size": { "width": 1, "height": 2 }
//   }
//
// Convenciones (no-enforcement del motor):
// - `id` matchea el filename del .mooditem (sin extension). El editor
//   garantiza la invariante al crear/renombrar.
// - Primera entrada de `tags` define la categoria visible en el
//   Item Browser. El dev del juego decide la ontologia.
// - `slot_size` solo lo consume el layout grid_2d del InventoryComponent
//   (F2H51 Bloque D). Lista plana y equipment_slots lo ignoran.
//
// Este modulo NO depende de ImGui — testeable en mood_tests.

#include "core/Types.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Mood::Inventory {

/// @brief Reglas de stackeo del item.
struct StackRules {
    bool stackable = false;  // si true, varias unidades comparten un slot
    int  max_stack = 1;      // cap del stack (solo se respeta si stackable=true)
};

/// @brief Tamano del item en el layout grid_2d (Resident Evil style).
struct SlotSize {
    int width  = 1;          // cells horizontales que ocupa
    int height = 1;          // cells verticales que ocupa
};

/// @brief Extension del filesystem para los archivos de item.
inline constexpr const char* k_fileExtension = ".mooditem";

/// @brief Asset declarativo del item — datos puros, sin runtime.
class Asset {
public:
    Asset() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Campos (acceso directo — struct-like) -----

    std::string id;                          // ID estable (matchea filename)
    std::string name_key;                    // i18n key (vacio = literal)
    std::string name_literal;                // fallback literal (vacio = usa key)
    std::string description_key;             // i18n key de la descripcion
    std::string description_literal;         // fallback literal
    std::string icon_path;                   // path logico (VFS) al icono
    std::string model_path;                  // path opcional a .glb/.fbx del modelo

    std::vector<std::string> tags;           // strings libres; primer tag = categoria

    /// @brief Stats key-value libre. Uso `std::map` (ordenado) en lugar
    ///        de `unordered_map` para que el orden de serializacion
    ///        sea determinista — tests de roundtrip dependen de esto.
    std::map<std::string, float> stats;

    StackRules stack;                        // stackable + max_stack
    SlotSize   slot_size;                    // width x height para grid_2d

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + todos los campos).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna asset vacio + loggea error.
    static Asset fromJson(const nlohmann::json& j);

    // ----- I/O de disco (F2H51 editor side) -----

    /// @brief Carga un asset desde un archivo `.mooditem`. Retorna nullopt
    ///        si el archivo no existe / no parsea / schema version
    ///        incompatible. Si el JSON no trae `id`, usa el filename
    ///        (sin extension) como id.
    static std::optional<Asset> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el asset al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;
};

} // namespace Mood::Inventory
