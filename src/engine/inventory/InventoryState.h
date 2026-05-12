#pragma once

// InventoryState (F2H51 Bloque D): contenedor del estado runtime de un
// `InventoryComponent`. Logica pura — sin ImGui, sin sistemas — para que
// el InventoryComponent sea testeable unitariamente. El pickup/drop
// integrado, el HUD widget y los bindings Lua viven en F2H52 sobre
// esta capa.
//
// Filosofia engine-grade: 3 layout modes son fundacionales (no future-work
// premature), porque queremos que el motor sirva a tanto Resident Evil
// (grid_2d) como Diablo (flat_list) como Skyrim (equipment_slots). El
// motor NO impone uno default por genero — el dev del juego decide.
//
// API publica:
//   - Lecturas puras: `has(id, minQty)`, `count(id)`, `slotCapacity()`.
//   - Mutaciones genericas: `add(id, qty, am)`, `remove(id, qty)`.
//   - Especificas de layout posicional: `placeAt(id, qty, slot, am)`,
//     `moveSlot(from, to)`.
//   - Reset: `clear()`.
//
// Semantica de stackeo:
//   - El `ItemAsset.stack` (stackable + max_stack) determina si varias
//     unidades comparten un slot. `InventoryState` consulta el am para
//     respetar la regla.
//   - `remove` es atomico (no parcial): si no hay suficiente entre
//     todos los entries del mismo id, rechaza sin modificar nada.
//
// Limitacion conocida v1: `slot_size` del ItemAsset (width x height para
// items que ocupan rectangulos > 1x1) se IGNORA — cada item ocupa
// 1 cell del grid_2d. Diferido a v2 cuando emerja necesidad real (el
// schema persiste el campo para no romper roundtrip).

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h"  // ItemAssetId

#include <string>
#include <vector>

namespace Mood::Inventory {

/// @brief Modo de layout del inventario. Configurable por instancia del
///        InventoryComponent — cada entidad decide su propio modo.
enum class LayoutMode {
    FlatList,         // lista plana con cap de entries (Diablo simplificado)
    Grid2D,           // grid WxH con slot_index (Resident Evil)
    EquipmentSlots,   // slots nombrados con tag_filter (Skyrim equipment)
};

/// @brief Slot nombrado para el modo EquipmentSlots. Cada slot acepta
///        solo items con el tag indicado (vacio = cualquiera).
struct EquipmentSlot {
    std::string name;        // ej. "head", "torso", "weapon_main"
    std::string tag_filter;  // tag requerido; "" = sin filtro
};

/// @brief Configuracion del layout. Solo los campos del modo activo se
///        usan; el resto se persiste pero se ignora.
struct LayoutConfig {
    // FlatList: cap del numero total de entries (NO de unidades).
    int max_items = 20;

    // Grid2D: dimensiones del grid en cells.
    int grid_width  = 4;
    int grid_height = 6;

    // EquipmentSlots: slots nombrados con filtro por tag.
    std::vector<EquipmentSlot> equipment_slots;
};

/// @brief Una entrada en el inventario: que item, cuanto, y donde
///        (si el modo lo requiere).
struct Entry {
    ItemAssetId itemId    = 0;   // 0 = item vacio (slot 0 del AssetManager)
    int         quantity  = 0;
    int         slot_index = -1; // -1 = sin slot asignado (FlatList lo ignora)
};

/// @brief Estado del inventario. Plano + serializable (sin runtime caches).
class State {
public:
    LayoutMode               mode = LayoutMode::FlatList;
    LayoutConfig             config;
    std::vector<Entry>       entries;

    // ----- Lecturas puras -----

    /// @brief true si hay al menos `minQty` unidades de `id` en total
    ///        (sumando todos los entries con ese id).
    bool has(ItemAssetId id, int minQty = 1) const;

    /// @brief Cantidad total de unidades de `id` sumando todos los entries.
    int  count(ItemAssetId id) const;

    /// @brief Capacidad total segun el modo activo. FlatList=max_items;
    ///        Grid2D=width*height; EquipmentSlots=size del array.
    int  slotCapacity() const;

    // ----- Mutaciones genericas (delegan en helpers segun mode) -----

    /// @brief Agrega `qty` unidades de `id` al inventario. Respeta
    ///        stackable + max_stack del ItemAsset (consultado en `am`).
    ///        Modo Grid2D / EquipmentSlots: asigna automaticamente al
    ///        primer slot libre que matchee filtros (si EquipmentSlots).
    ///        Si excede capacity, rechaza atomicamente (no agrega parcial).
    /// @return true si se agrego todo el qty solicitado; false si fallo.
    bool add(ItemAssetId id, int qty, const AssetManager& am);

    /// @brief Substrae `qty` unidades de `id`. Atomico: si no hay suficiente
    ///        entre todos los entries con ese id, rechaza sin modificar.
    ///        Borra entries que lleguen a quantity=0.
    bool remove(ItemAssetId id, int qty);

    // ----- Especificas de layout posicional -----

    /// @brief Coloca `qty` unidades de `id` en `slotIndex`. Solo Grid2D /
    ///        EquipmentSlots. Falla si:
    ///        - slotIndex fuera de rango,
    ///        - slot ocupado por item distinto,
    ///        - EquipmentSlots: tag_filter no matchea,
    ///        - stackable=false y slot ocupado.
    bool placeAt(ItemAssetId id, int qty, int slotIndex, const AssetManager& am);

    /// @brief Mueve la entry que ocupa `fromSlot` a `toSlot` (solo Grid2D
    ///        / EquipmentSlots). Falla si `toSlot` esta ocupado (no hay
    ///        swap automatico en v1) o `fromSlot` esta vacio.
    bool moveSlot(int fromSlot, int toSlot);

    // ----- Reset -----

    /// @brief Vacia entries pero preserva mode + config. Para "drop all"
    ///        u "open new inventory layout sin perder configuracion".
    void clear();
};

} // namespace Mood::Inventory
