// F2H48: AssetManager — operaciones sobre Dialog assets (.mooddialog).
// Mismo patron que `AssetManager_Prefab.cpp`: lookup en cache, resolve
// via VFS, carga desde disco con `Dialog::Asset::loadFromFile`, fallback
// al slot 0 (asset vacio) si algo falla.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/dialog/DialogAsset.h"

#include <utility>

namespace Mood {

DialogAssetId AssetManager::loadDialog(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_dialogCache.find(key); it != m_dialogCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: dialog path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_dialogCache.emplace(key, missingDialogId());
        return missingDialogId();
    }

    auto loaded = Dialog::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_dialogCache.emplace(key, missingDialogId());
        return missingDialogId();
    }

    auto stored = std::make_unique<Dialog::Asset>(std::move(*loaded));
    const DialogAssetId id = static_cast<DialogAssetId>(m_dialogs.size());
    m_dialogs.push_back(std::move(stored));
    m_dialogPaths.push_back(key);
    m_dialogCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado dialog {} -> id {}", logicalPath, id);
    return id;
}

const Dialog::Asset* AssetManager::getDialog(DialogAssetId id) const {
    if (id >= m_dialogs.size()) {
        return m_dialogs[missingDialogId()].get();
    }
    return m_dialogs[id].get();
}

std::string AssetManager::dialogPathOf(DialogAssetId id) const {
    if (id >= m_dialogPaths.size()) {
        return m_dialogPaths[missingDialogId()];
    }
    return m_dialogPaths[id];
}

usize AssetManager::dialogCount() const {
    return m_dialogs.size();
}

} // namespace Mood
