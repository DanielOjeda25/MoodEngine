// F2H24 Bloque C: AssetManager — operaciones sobre prefabs (Hito 14).
// loadPrefab / getPrefab / prefabPathOf.
//
// break-B5: storage delegado a AssetRegistry<SavedPrefab>.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/scene/serialization/PrefabSerializer.h"

#include <utility>

namespace Mood {

PrefabAssetId AssetManager::loadPrefab(std::string_view logicalPath) {
    if (m_prefabs.contains(logicalPath)) {
        return m_prefabs.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: prefab path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_prefabs.cacheAsFallback(logicalPath);
        return missingPrefabId();
    }

    auto loaded = PrefabSerializer::load(fs);
    if (!loaded.has_value()) {
        // Loggeo ya emitido por el serializer.
        m_prefabs.cacheAsFallback(logicalPath);
        return missingPrefabId();
    }

    auto stored = std::make_unique<SavedPrefab>(std::move(*loaded));
    const PrefabAssetId id = m_prefabs.add(std::string{logicalPath},
                                             std::move(stored));
    Log::assets()->info("AssetManager: cargado prefab {} -> id {}", logicalPath, id);
    return id;
}

const SavedPrefab* AssetManager::getPrefab(PrefabAssetId id) const {
    return m_prefabs.get(id);
}

std::string AssetManager::prefabPathOf(PrefabAssetId id) const {
    return m_prefabs.pathOf(id);
}

} // namespace Mood
