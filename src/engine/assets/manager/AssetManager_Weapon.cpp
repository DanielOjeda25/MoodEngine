// F4H2: AssetManager — operaciones sobre Weapon specs (.moodweapon).
// Mismo patron que `AssetManager_Item.cpp` / `AssetManager_Quest.cpp`:
// lookup en cache, resolve via VFS, carga desde disco con
// `Weapon::Spec::loadFromFile`, fallback al slot 0 (spec vacio) si algo
// falla. Engine-generic: el motor no conoce categorias hardcoded de
// armas — los specs son data declarativa.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/gameplay/weapon/WeaponSpec.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
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

std::vector<AssetManager::WeaponListEntry>
AssetManager::enumerateWeapons(bool rescanFromDisk) {
    // 1) Defensivo: scan filesystem y `loadWeapon` los que falten en cache.
    //    Sin esto, abrir el Inspector antes de que el AssetBrowser haya
    //    rescaneado deja el combo vacio.
    if (rescanFromDisk) {
        std::error_code ec;
        constexpr const char* k_weaponDir          = "assets/weapons";
        constexpr const char* k_weaponLogicalPrefix = "weapons/";
        std::filesystem::directory_iterator it(k_weaponDir, ec);
        if (!ec) {
            for (const auto& entry : it) {
                if (!entry.is_regular_file()) continue;
                const auto ext = entry.path().extension().string();
                std::string lower(ext.size(), '\0');
                std::transform(ext.begin(), ext.end(), lower.begin(),
                                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (lower != ".moodweapon") continue;
                const std::string logical =
                    std::string(k_weaponLogicalPrefix) +
                    entry.path().filename().string();
                if (!m_weapons.contains(logical)) {
                    loadWeapon(logical);
                }
            }
        }
    }

    // 2) Iterar el cache. Slot 0 es el spec vacio fallback — saltarlo.
    std::vector<WeaponListEntry> out;
    const usize n = m_weapons.count();
    out.reserve(n > 0 ? n - 1 : 0);
    for (WeaponAssetId id = 1; id < n; ++id) {
        WeaponListEntry e;
        e.id = id;
        e.logicalPath = m_weapons.pathOf(id);
        const Weapon::Spec* spec = m_weapons.get(id);
        if (spec != nullptr && !spec->displayName.empty()) {
            e.displayName = spec->displayName;
        } else {
            // Fallback al stem del path lógico (sin "weapons/" prefix ni ext).
            std::filesystem::path p(e.logicalPath);
            e.displayName = p.stem().string();
        }
        out.push_back(std::move(e));
    }

    // 3) Sort case-insensitive por displayName.
    std::sort(out.begin(), out.end(),
              [](const WeaponListEntry& a, const WeaponListEntry& b) {
                  auto toLower = [](std::string s) {
                      std::transform(s.begin(), s.end(), s.begin(),
                                      [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                      return s;
                  };
                  return toLower(a.displayName) < toLower(b.displayName);
              });
    return out;
}

} // namespace Mood
