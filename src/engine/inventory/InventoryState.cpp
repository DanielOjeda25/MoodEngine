#include "engine/inventory/InventoryState.h"

#include "engine/inventory/ItemAsset.h"

#include <algorithm>

namespace Mood::Inventory {

namespace {

// ---- Helpers internos ----

Entry* findEntryAtSlot(std::vector<Entry>& entries, int slot) {
    if (slot < 0) return nullptr;
    for (Entry& e : entries) {
        if (e.slot_index == slot && e.quantity > 0) return &e;
    }
    return nullptr;
}

const Entry* findEntryAtSlot(const std::vector<Entry>& entries, int slot) {
    if (slot < 0) return nullptr;
    for (const Entry& e : entries) {
        if (e.slot_index == slot && e.quantity > 0) return &e;
    }
    return nullptr;
}

/// @brief true si el item tiene `tagFilter` entre sus tags. Filter vacio
///        siempre matchea.
bool itemMatchesTag(const AssetManager& am, ItemAssetId id, const std::string& tagFilter) {
    if (tagFilter.empty()) return true;
    const Asset* a = am.getItem(id);
    if (!a) return false;
    for (const std::string& t : a->tags) {
        if (t == tagFilter) return true;
    }
    return false;
}

/// @brief Devuelve {stackable, max_stack effective}. max_stack=1 cuando
///        !stackable (defensivo) o cuando el field del asset es 0/negativo.
struct StackRulesEff { bool stackable; int max_stack; };

StackRulesEff stackRulesOf(const AssetManager& am, ItemAssetId id) {
    const Asset* a = am.getItem(id);
    if (!a) return {false, 1};
    const bool s = a->stack.stackable;
    const int  m = s ? std::max(1, a->stack.max_stack) : 1;
    return {s, m};
}

} // anon namespace

// =============================================================
// Lecturas puras
// =============================================================

bool State::has(ItemAssetId id, int minQty) const {
    return count(id) >= minQty;
}

int State::count(ItemAssetId id) const {
    int total = 0;
    for (const Entry& e : entries) {
        if (e.itemId == id) total += e.quantity;
    }
    return total;
}

int State::slotCapacity() const {
    switch (mode) {
        case LayoutMode::FlatList:       return config.max_items;
        case LayoutMode::Grid2D:         return config.grid_width * config.grid_height;
        case LayoutMode::EquipmentSlots: return static_cast<int>(config.equipment_slots.size());
    }
    return 0;
}

// =============================================================
// add (delega por mode)
// =============================================================

bool State::add(ItemAssetId id, int qty, const AssetManager& am) {
    if (qty <= 0) return false;
    if (id == 0)  return false;  // slot 0 = item vacio, no se agrega

    const auto [stackable, maxStack] = stackRulesOf(am, id);
    int remaining = qty;

    if (mode == LayoutMode::FlatList) {
        // 1) Llenar entries existentes con espacio (solo si stackable).
        if (stackable) {
            for (Entry& e : entries) {
                if (e.itemId != id) continue;
                if (e.quantity >= maxStack) continue;
                const int space = maxStack - e.quantity;
                const int taken = std::min(space, remaining);
                e.quantity += taken;
                remaining -= taken;
                if (remaining == 0) return true;
            }
        }
        // 2) Calcular entries nuevos necesarios para el remaining.
        const int newEntries = stackable
            ? (remaining + maxStack - 1) / maxStack
            : remaining;
        if (static_cast<int>(entries.size()) + newEntries > config.max_items) {
            return false;
        }
        // 3) Crear entries.
        while (remaining > 0) {
            Entry e;
            e.itemId   = id;
            e.quantity = stackable ? std::min(maxStack, remaining) : 1;
            entries.push_back(e);
            remaining -= e.quantity;
        }
        return true;
    }

    if (mode == LayoutMode::Grid2D) {
        const int capacity = slotCapacity();
        // 1) Llenar slots ocupados con mismo id (si stackable).
        if (stackable) {
            for (Entry& e : entries) {
                if (e.itemId != id) continue;
                if (e.quantity >= maxStack) continue;
                const int space = maxStack - e.quantity;
                const int taken = std::min(space, remaining);
                e.quantity += taken;
                remaining -= taken;
                if (remaining == 0) return true;
            }
        }
        // 2) Asignar a slots libres. Reservar primero (atomic): contar
        //    slots disponibles + necesarios antes de mutar.
        std::vector<bool> occupied(capacity, false);
        for (const Entry& e : entries) {
            if (e.slot_index >= 0 && e.slot_index < capacity && e.quantity > 0) {
                occupied[e.slot_index] = true;
            }
        }
        const int slotsNeeded = stackable
            ? (remaining + maxStack - 1) / maxStack
            : remaining;
        int freeSlots = 0;
        for (bool occ : occupied) if (!occ) ++freeSlots;
        if (freeSlots < slotsNeeded) return false;
        // 3) Crear entries en los primeros slots libres.
        for (int s = 0; s < capacity && remaining > 0; ++s) {
            if (occupied[s]) continue;
            Entry e;
            e.itemId     = id;
            e.quantity   = stackable ? std::min(maxStack, remaining) : 1;
            e.slot_index = s;
            entries.push_back(e);
            remaining -= e.quantity;
        }
        return true;
    }

    if (mode == LayoutMode::EquipmentSlots) {
        const int capacity = slotCapacity();
        // 1) Llenar slot ocupado por mismo id (si stackable).
        if (stackable) {
            for (Entry& e : entries) {
                if (e.itemId != id) continue;
                if (e.quantity >= maxStack) continue;
                const int space = maxStack - e.quantity;
                const int taken = std::min(space, remaining);
                e.quantity += taken;
                remaining -= taken;
                if (remaining == 0) return true;
            }
        }
        // 2) Calcular slots disponibles que matcheen tag_filter del item.
        std::vector<bool> occupied(capacity, false);
        for (const Entry& e : entries) {
            if (e.slot_index >= 0 && e.slot_index < capacity && e.quantity > 0) {
                occupied[e.slot_index] = true;
            }
        }
        const int slotsNeeded = stackable
            ? (remaining + maxStack - 1) / maxStack
            : remaining;
        int matchingFreeSlots = 0;
        for (int s = 0; s < capacity; ++s) {
            if (occupied[s]) continue;
            if (itemMatchesTag(am, id, config.equipment_slots[s].tag_filter)) {
                ++matchingFreeSlots;
            }
        }
        if (matchingFreeSlots < slotsNeeded) return false;
        // 3) Asignar a los primeros slots libres compatibles.
        for (int s = 0; s < capacity && remaining > 0; ++s) {
            if (occupied[s]) continue;
            if (!itemMatchesTag(am, id, config.equipment_slots[s].tag_filter)) continue;
            Entry e;
            e.itemId     = id;
            e.quantity   = stackable ? std::min(maxStack, remaining) : 1;
            e.slot_index = s;
            entries.push_back(e);
            remaining -= e.quantity;
        }
        return true;
    }

    return false;
}

// =============================================================
// remove (atomic: rechaza si insuficiente)
// =============================================================

bool State::remove(ItemAssetId id, int qty) {
    if (qty <= 0) return false;
    if (count(id) < qty) return false;
    int remaining = qty;
    // Iterar y restar de cada entry; borrar las que lleguen a 0.
    for (auto it = entries.begin(); it != entries.end() && remaining > 0; ) {
        if (it->itemId == id) {
            const int taken = std::min(it->quantity, remaining);
            it->quantity -= taken;
            remaining -= taken;
            if (it->quantity == 0) {
                it = entries.erase(it);
                continue;
            }
        }
        ++it;
    }
    return true;
}

// =============================================================
// placeAt (solo Grid2D / EquipmentSlots)
// =============================================================

bool State::placeAt(ItemAssetId id, int qty, int slotIndex, const AssetManager& am) {
    if (qty <= 0) return false;
    if (id == 0)  return false;
    if (mode == LayoutMode::FlatList) return false;  // FlatList no usa slots

    const int capacity = slotCapacity();
    if (slotIndex < 0 || slotIndex >= capacity) return false;

    // EquipmentSlots: validar tag_filter.
    if (mode == LayoutMode::EquipmentSlots) {
        if (!itemMatchesTag(am, id, config.equipment_slots[slotIndex].tag_filter)) {
            return false;
        }
    }

    const auto [stackable, maxStack] = stackRulesOf(am, id);

    Entry* existing = findEntryAtSlot(entries, slotIndex);
    if (existing != nullptr) {
        // Slot ocupado.
        if (existing->itemId != id) return false;        // distinto item -> reject
        if (!stackable)            return false;         // no stackable -> reject
        const int space = maxStack - existing->quantity;
        if (qty > space)            return false;        // overflow -> reject (atomic)
        existing->quantity += qty;
        return true;
    }

    // Slot vacio: crear entry. Solo cabe `min(qty, maxStack)`; si stackable=false
    // y qty>1, rechazar (no podemos meter >1 en un slot non-stackable).
    if (!stackable && qty > 1) return false;
    if (stackable && qty > maxStack) return false;

    Entry e;
    e.itemId     = id;
    e.quantity   = qty;
    e.slot_index = slotIndex;
    entries.push_back(e);
    return true;
}

// =============================================================
// moveSlot
// =============================================================

bool State::moveSlot(int fromSlot, int toSlot) {
    if (mode == LayoutMode::FlatList) return false;
    if (fromSlot == toSlot)           return false;
    const int capacity = slotCapacity();
    if (fromSlot < 0 || fromSlot >= capacity) return false;
    if (toSlot   < 0 || toSlot   >= capacity) return false;

    Entry* fromEntry = findEntryAtSlot(entries, fromSlot);
    if (fromEntry == nullptr) return false;          // origen vacio
    if (findEntryAtSlot(entries, toSlot) != nullptr) return false; // destino ocupado

    fromEntry->slot_index = toSlot;
    return true;
}

// =============================================================
// clear
// =============================================================

void State::clear() {
    entries.clear();
}

} // namespace Mood::Inventory
