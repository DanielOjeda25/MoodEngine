// F2H49 Bloque C: AssetManager — operaciones sobre AnimationClip standalone.
// loadAnimationClip / getAnimationClip / animationClipPathOf.
//
// Sigue el mismo patron que `AssetManager_Mesh.cpp`:
//   - Slot 0 reservado para fallback (clip vacio creado en el ctor).
//   - Cache por path logico evita doble-load.
//   - En fallo de loader, cacheamos el id 0 para no reintentar cada frame.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/animation/clips/AnimationClip.h"
#include "engine/assets/loaders/MeshLoader.h"

#include <utility>

namespace Mood {

AnimationClipAssetId AssetManager::loadAnimationClip(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_animationClipCache.find(key); it != m_animationClipCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: clip path '{}' rechazado por VFS. Fallback a clip vacio.",
            logicalPath);
        m_animationClipCache.emplace(key, missingAnimationClipId());
        return missingAnimationClipId();
    }

    auto clip = loadAnimationClipWithAssimp(key, fs.generic_string());
    if (clip == nullptr) {
        // Loggeo ya emitido por loadAnimationClipWithAssimp.
        m_animationClipCache.emplace(key, missingAnimationClipId());
        return missingAnimationClipId();
    }

    const AnimationClipAssetId id =
        static_cast<AnimationClipAssetId>(m_animationClips.size());
    m_animationClips.push_back(std::move(clip));
    m_animationClipPaths.push_back(key);
    m_animationClipCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado clip '{}' -> id {}", logicalPath, id);
    return id;
}

AnimationClip* AssetManager::getAnimationClip(AnimationClipAssetId id) const {
    if (id >= m_animationClips.size()) {
        return m_animationClips[missingAnimationClipId()].get();
    }
    return m_animationClips[id].get();
}

std::string AssetManager::animationClipPathOf(AnimationClipAssetId id) const {
    if (id >= m_animationClipPaths.size()) {
        return m_animationClipPaths[missingAnimationClipId()];
    }
    return m_animationClipPaths[id];
}

usize AssetManager::animationClipCount() const {
    return m_animationClips.size();
}

} // namespace Mood
