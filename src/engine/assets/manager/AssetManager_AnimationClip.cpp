// F2H49 Bloque C: AssetManager — operaciones sobre AnimationClip standalone.
// loadAnimationClip / getAnimationClip / animationClipPathOf.
//
// Sigue el mismo patron que `AssetManager_Mesh.cpp`:
//   - Slot 0 reservado para fallback (clip vacio creado en el ctor).
//   - Cache por path logico evita doble-load.
//   - En fallo de loader, cacheamos el id 0 para no reintentar cada frame.
//
// break-B5: storage delegado a AssetRegistry<AnimationClip>. Notese que
// getAnimationClip mantiene el return de `AnimationClip*` (no const) — el
// preview renderer + el AssetBrowser lo mutan via `clip->name = ...`. Se
// pasa por `.all()[id].get()` para bypassar la const-correctness del
// registry (unique_ptr<T>::get() devuelve T* aunque la unique_ptr sea
// const-reference).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/animation/clips/AnimationClip.h"
#include "engine/assets/loaders/MeshLoader.h"

#include <utility>

namespace Mood {

AnimationClipAssetId AssetManager::loadAnimationClip(std::string_view logicalPath) {
    if (m_animationClips.contains(logicalPath)) {
        return m_animationClips.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: clip path '{}' rechazado por VFS. Fallback a clip vacio.",
            logicalPath);
        m_animationClips.cacheAsFallback(logicalPath);
        return missingAnimationClipId();
    }

    const std::string key{logicalPath};
    auto clip = loadAnimationClipWithAssimp(key, fs.generic_string());
    if (clip == nullptr) {
        // Loggeo ya emitido por loadAnimationClipWithAssimp.
        m_animationClips.cacheAsFallback(logicalPath);
        return missingAnimationClipId();
    }

    const AnimationClipAssetId id = m_animationClips.add(key, std::move(clip));
    Log::assets()->info("AssetManager: cargado clip '{}' -> id {}", logicalPath, id);
    return id;
}

AnimationClip* AssetManager::getAnimationClip(AnimationClipAssetId id) const {
    // break-B5: getAnimationClip devuelve `AnimationClip*` (no const) por
    // contrato pre-break. Bypassamos la const-correctness del registry
    // pasando por `.all()` — unique_ptr<T>::get() devuelve T* incluso
    // cuando la unique_ptr es const-referenciada.
    const auto& v = m_animationClips.all();
    if (v.empty()) return nullptr;
    if (id >= v.size()) return v[0].get();
    return v[id].get();
}

std::string AssetManager::animationClipPathOf(AnimationClipAssetId id) const {
    return m_animationClips.pathOf(id);
}

usize AssetManager::animationClipCount() const {
    return m_animationClips.count();
}

} // namespace Mood
