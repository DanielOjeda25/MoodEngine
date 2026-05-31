// F4H7: AssetManager — operaciones sobre Enemy specs (.moodenemy).
// Mismo patron que `AssetManager_Weapon.cpp`: lookup en cache, resolve
// via VFS, carga desde disco con `Enemy::Spec::loadFromFile`, fallback
// al slot 0 (spec vacio) si algo falla. Engine-generic.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/gameplay/enemy/EnemySpec.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

namespace Mood {

EnemyAssetId AssetManager::loadEnemy(std::string_view logicalPath) {
    if (m_enemies.contains(logicalPath)) {
        return m_enemies.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: enemy path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_enemies.cacheAsFallback(logicalPath);
        return missingEnemyId();
    }

    auto loaded = Enemy::Spec::loadFromFile(fs);
    if (!loaded.has_value()) {
        m_enemies.cacheAsFallback(logicalPath);
        return missingEnemyId();
    }

    auto stored = std::make_unique<Enemy::Spec>(std::move(*loaded));
    const EnemyAssetId id = m_enemies.add(std::string{logicalPath},
                                            std::move(stored));
    Log::assets()->info("AssetManager: cargado enemy {} -> id {}", logicalPath, id);
    return id;
}

const Enemy::Spec* AssetManager::getEnemy(EnemyAssetId id) const {
    return m_enemies.get(id);
}

std::string AssetManager::enemyPathOf(EnemyAssetId id) const {
    return m_enemies.pathOf(id);
}

usize AssetManager::enemyCount() const {
    return m_enemies.count();
}

std::vector<AssetManager::EnemyListEntry>
AssetManager::enumerateEnemies(bool rescanFromDisk) {
    if (rescanFromDisk) {
        std::error_code ec;
        constexpr const char* k_enemyDir            = "assets/enemies";
        constexpr const char* k_enemyLogicalPrefix  = "enemies/";
        std::filesystem::directory_iterator it(k_enemyDir, ec);
        if (!ec) {
            for (const auto& entry : it) {
                if (!entry.is_regular_file()) continue;
                const auto ext = entry.path().extension().string();
                std::string lower(ext.size(), '\0');
                std::transform(ext.begin(), ext.end(), lower.begin(),
                                [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (lower != ".moodenemy") continue;
                const std::string logical =
                    std::string(k_enemyLogicalPrefix) +
                    entry.path().filename().string();
                if (!m_enemies.contains(logical)) {
                    loadEnemy(logical);
                }
            }
        }
    }

    std::vector<EnemyListEntry> out;
    const usize n = m_enemies.count();
    out.reserve(n > 0 ? n - 1 : 0);
    for (EnemyAssetId id = 1; id < n; ++id) {
        EnemyListEntry e;
        e.id = id;
        e.logicalPath = m_enemies.pathOf(id);
        const Enemy::Spec* spec = m_enemies.get(id);
        if (spec != nullptr && !spec->displayName.empty()) {
            e.displayName = spec->displayName;
        } else {
            std::filesystem::path p(e.logicalPath);
            e.displayName = p.stem().string();
        }
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(),
              [](const EnemyListEntry& a, const EnemyListEntry& b) {
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
