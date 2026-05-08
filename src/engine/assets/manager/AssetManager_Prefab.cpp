// F2H24 Bloque C: AssetManager — operaciones sobre prefabs (Hito 14).
// loadPrefab / getPrefab / prefabPathOf.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/scene/serialization/PrefabSerializer.h"

#include <utility>

namespace Mood {

PrefabAssetId AssetManager::loadPrefab(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_prefabCache.find(key); it != m_prefabCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: prefab path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_prefabCache.emplace(key, missingPrefabId());
        return missingPrefabId();
    }

    auto loaded = PrefabSerializer::load(fs);
    if (!loaded.has_value()) {
        // Loggeo ya emitido por el serializer.
        m_prefabCache.emplace(key, missingPrefabId());
        return missingPrefabId();
    }

    auto stored = std::make_unique<SavedPrefab>(std::move(*loaded));
    const PrefabAssetId id = static_cast<PrefabAssetId>(m_prefabs.size());
    m_prefabs.push_back(std::move(stored));
    m_prefabPaths.push_back(key);
    m_prefabCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado prefab {} -> id {}", logicalPath, id);
    return id;
}

const SavedPrefab* AssetManager::getPrefab(PrefabAssetId id) const {
    if (id >= m_prefabs.size()) {
        return m_prefabs[missingPrefabId()].get();
    }
    return m_prefabs[id].get();
}

std::string AssetManager::prefabPathOf(PrefabAssetId id) const {
    if (id >= m_prefabPaths.size()) {
        return m_prefabPaths[missingPrefabId()];
    }
    return m_prefabPaths[id];
}

} // namespace Mood
