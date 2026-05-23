// F2H51: AssetManager — operaciones sobre Item assets (.mooditem).
// Mismo patron que `AssetManager_Dialog.cpp`: lookup en cache, resolve
// via VFS, carga desde disco con `Inventory::Asset::loadFromFile`, fallback
// al slot 0 (asset vacio) si algo falla.
//
// break-B5: storage delegado a AssetRegistry<Inventory::Asset>.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/inventory/ItemAsset.h"

#include <utility>

namespace Mood {

ItemAssetId AssetManager::loadItem(std::string_view logicalPath) {
    if (m_items.contains(logicalPath)) {
        return m_items.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: item path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_items.cacheAsFallback(logicalPath);
        return missingItemId();
    }

    auto loaded = Inventory::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_items.cacheAsFallback(logicalPath);
        return missingItemId();
    }

    auto stored = std::make_unique<Inventory::Asset>(std::move(*loaded));
    const ItemAssetId id = m_items.add(std::string{logicalPath},
                                         std::move(stored));
    Log::assets()->info("AssetManager: cargado item {} -> id {}", logicalPath, id);
    return id;
}

const Inventory::Asset* AssetManager::getItem(ItemAssetId id) const {
    return m_items.get(id);
}

std::string AssetManager::itemPathOf(ItemAssetId id) const {
    return m_items.pathOf(id);
}

usize AssetManager::itemCount() const {
    return m_items.count();
}

} // namespace Mood
