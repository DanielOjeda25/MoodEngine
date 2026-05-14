// F2H53 Bloque B: AssetManager — operaciones sobre Quest assets (.moodquest).
// Mismo patron que `AssetManager_Item.cpp` (F2H51): lookup en cache, resolve
// via VFS, carga desde disco con `Quest::Asset::loadFromFile`, fallback al
// slot 0 (asset vacio) si algo falla.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/quest/QuestAsset.h"

#include <utility>

namespace Mood {

QuestAssetId AssetManager::loadQuest(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_questCache.find(key); it != m_questCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: quest path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_questCache.emplace(key, missingQuestId());
        return missingQuestId();
    }

    auto loaded = Quest::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_questCache.emplace(key, missingQuestId());
        return missingQuestId();
    }

    auto stored = std::make_unique<Quest::Asset>(std::move(*loaded));
    const QuestAssetId id = static_cast<QuestAssetId>(m_quests.size());
    m_quests.push_back(std::move(stored));
    m_questPaths.push_back(key);
    m_questCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado quest {} -> id {}", logicalPath, id);
    return id;
}

const Quest::Asset* AssetManager::getQuest(QuestAssetId id) const {
    if (id >= m_quests.size()) {
        return m_quests[missingQuestId()].get();
    }
    return m_quests[id].get();
}

std::string AssetManager::questPathOf(QuestAssetId id) const {
    if (id >= m_questPaths.size()) {
        return m_questPaths[missingQuestId()];
    }
    return m_questPaths[id];
}

usize AssetManager::questCount() const {
    return m_quests.size();
}

} // namespace Mood
