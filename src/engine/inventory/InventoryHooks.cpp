#include "engine/inventory/InventoryHooks.h"

#include "core/Log.h"
#include "engine/scene/core/Entity.h"

namespace Mood::Inventory::Hooks {

namespace {
PickupHook g_pickup;
DropHook   g_drop;
UseHook    g_use;
} // namespace

void setPickupHook(PickupHook fn) {
    if (g_pickup && fn) {
        Log::script()->warn(
            "[inventory.on_pickup] sobrescribiendo callback previo (v1 = solo "
            "1 hook a la vez por evento)");
    }
    g_pickup = std::move(fn);
}

void setDropHook(DropHook fn) {
    if (g_drop && fn) {
        Log::script()->warn(
            "[inventory.on_drop] sobrescribiendo callback previo (v1 = solo "
            "1 hook a la vez por evento)");
    }
    g_drop = std::move(fn);
}

void setUseHook(UseHook fn) {
    if (g_use && fn) {
        Log::script()->warn(
            "[inventory.on_use] sobrescribiendo callback previo (v1 = solo "
            "1 hook a la vez por evento)");
    }
    g_use = std::move(fn);
}

void clearAll() {
    g_pickup = nullptr;
    g_drop   = nullptr;
    g_use    = nullptr;
}

void invokePickup(const std::string& item_path, int quantity) {
    if (g_pickup) g_pickup(item_path, quantity);
}

void invokeDrop(const std::string& item_path, int quantity) {
    if (g_drop) g_drop(item_path, quantity);
}

void invokeUse(Entity entity, const std::string& item_path) {
    if (g_use) g_use(entity, item_path);
}

bool hasPickupHook() { return static_cast<bool>(g_pickup); }
bool hasDropHook()   { return static_cast<bool>(g_drop);   }
bool hasUseHook()    { return static_cast<bool>(g_use);    }

} // namespace Mood::Inventory::Hooks
