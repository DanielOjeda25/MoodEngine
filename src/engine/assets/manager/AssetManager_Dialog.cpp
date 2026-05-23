// F2H48: AssetManager — operaciones sobre Dialog assets (.mooddialog).
// Mismo patron que `AssetManager_Prefab.cpp`: lookup en cache, resolve
// via VFS, carga desde disco con `Dialog::Asset::loadFromFile`, fallback
// al slot 0 (asset vacio) si algo falla.
//
// break-B5: storage delegado a AssetRegistry<Dialog::Asset>. El boilerplate
// (cache map + vector + paths paralelos) lo encapsula la plantilla; aca
// solo queda la logica especifica del Dialog (resolve via VFS + parse).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/dialog/DialogAsset.h"

#include <utility>

namespace Mood {

DialogAssetId AssetManager::loadDialog(std::string_view logicalPath) {
    if (m_dialogs.contains(logicalPath)) {
        return m_dialogs.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: dialog path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_dialogs.cacheAsFallback(logicalPath);
        return missingDialogId();
    }

    auto loaded = Dialog::Asset::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_dialogs.cacheAsFallback(logicalPath);
        return missingDialogId();
    }

    auto stored = std::make_unique<Dialog::Asset>(std::move(*loaded));
    const DialogAssetId id = m_dialogs.add(std::string{logicalPath},
                                             std::move(stored));
    Log::assets()->info("AssetManager: cargado dialog {} -> id {}", logicalPath, id);
    return id;
}

const Dialog::Asset* AssetManager::getDialog(DialogAssetId id) const {
    return m_dialogs.get(id);
}

std::string AssetManager::dialogPathOf(DialogAssetId id) const {
    return m_dialogs.pathOf(id);
}

usize AssetManager::dialogCount() const {
    return m_dialogs.count();
}

} // namespace Mood
