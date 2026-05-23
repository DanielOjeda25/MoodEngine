// F2H53 Bloque B: AssetManager — operaciones sobre Quest assets (.moodquest).
// Mismo patron que `AssetManager_Item.cpp` (F2H51): lookup en cache, resolve
// via VFS, carga desde disco con `Quest::Asset::loadFromFile`, fallback al
// slot 0 (asset vacio) si algo falla.
//
// break-B5: storage delegado a AssetRegistry<Quest::Asset>.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/quest/QuestAsset.h"

#include <utility>

namespace Mood {

QuestAssetId AssetManager::loadQuest(std::string_view logicalPath) {
    if (m_quests.contains(logicalPath)) {
        return m_quests.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: quest path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_quests.cacheAsFallback(logicalPath);
        return missingQuestId();
    }

    auto loaded = Quest::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_quests.cacheAsFallback(logicalPath);
        return missingQuestId();
    }

    auto stored = std::make_unique<Quest::Asset>(std::move(*loaded));
    const QuestAssetId id = m_quests.add(std::string{logicalPath},
                                           std::move(stored));
    Log::assets()->info("AssetManager: cargado quest {} -> id {}", logicalPath, id);
    return id;
}

const Quest::Asset* AssetManager::getQuest(QuestAssetId id) const {
    return m_quests.get(id);
}

std::string AssetManager::questPathOf(QuestAssetId id) const {
    return m_quests.pathOf(id);
}

usize AssetManager::questCount() const {
    return m_quests.count();
}

} // namespace Mood
