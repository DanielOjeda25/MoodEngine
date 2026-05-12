// F2H51: AssetManager — operaciones sobre Item assets (.mooditem).
// Mismo patron que `AssetManager_Dialog.cpp`: lookup en cache, resolve
// via VFS, carga desde disco con `Inventory::Asset::loadFromFile`, fallback
// al slot 0 (asset vacio) si algo falla.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/inventory/ItemAsset.h"

#include <utility>

namespace Mood {

ItemAssetId AssetManager::loadItem(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_itemCache.find(key); it != m_itemCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: item path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_itemCache.emplace(key, missingItemId());
        return missingItemId();
    }

    auto loaded = Inventory::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_itemCache.emplace(key, missingItemId());
        return missingItemId();
    }

    auto stored = std::make_unique<Inventory::Asset>(std::move(*loaded));
    const ItemAssetId id = static_cast<ItemAssetId>(m_items.size());
    m_items.push_back(std::move(stored));
    m_itemPaths.push_back(key);
    m_itemCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado item {} -> id {}", logicalPath, id);
    return id;
}

const Inventory::Asset* AssetManager::getItem(ItemAssetId id) const {
    if (id >= m_items.size()) {
        return m_items[missingItemId()].get();
    }
    return m_items[id].get();
}

std::string AssetManager::itemPathOf(ItemAssetId id) const {
    if (id >= m_itemPaths.size()) {
        return m_itemPaths[missingItemId()];
    }
    return m_itemPaths[id];
}

usize AssetManager::itemCount() const {
    return m_items.size();
}

} // namespace Mood
