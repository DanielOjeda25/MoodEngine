// F4H2: AssetManager — operaciones sobre Weapon specs (.moodweapon).
// Mismo patron que `AssetManager_Item.cpp` / `AssetManager_Quest.cpp`:
// lookup en cache, resolve via VFS, carga desde disco con
// `Weapon::Spec::loadFromFile`, fallback al slot 0 (spec vacio) si algo
// falla. Engine-generic: el motor no conoce categorias hardcoded de
// armas — los specs son data declarativa.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/gameplay/weapon/WeaponSpec.h"

#include <utility>

namespace Mood {

WeaponAssetId AssetManager::loadWeapon(std::string_view logicalPath) {
    if (m_weapons.contains(logicalPath)) {
        return m_weapons.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: weapon path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_weapons.cacheAsFallback(logicalPath);
        return missingWeaponId();
    }

    auto loaded = Weapon::Spec::loadFromFile(fs);
    if (!loaded.has_value()) {
        // Loggeo emitido por `loadFromFile`.
        m_weapons.cacheAsFallback(logicalPath);
        return missingWeaponId();
    }

    auto stored = std::make_unique<Weapon::Spec>(std::move(*loaded));
    const WeaponAssetId id = m_weapons.add(std::string{logicalPath},
                                              std::move(stored));
    Log::assets()->info("AssetManager: cargado weapon {} -> id {}", logicalPath, id);
    return id;
}

const Weapon::Spec* AssetManager::getWeapon(WeaponAssetId id) const {
    return m_weapons.get(id);
}

std::string AssetManager::weaponPathOf(WeaponAssetId id) const {
    return m_weapons.pathOf(id);
}

usize AssetManager::weaponCount() const {
    return m_weapons.count();
}

} // namespace Mood
