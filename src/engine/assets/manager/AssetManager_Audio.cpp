// F2H24 Bloque C: AssetManager — operaciones sobre clips de audio.
// loadAudio / getAudio / audioPathOf.
//
// break-B5: storage delegado a AssetRegistry<AudioClip>. audioPathOf
// devuelve el path agregado al registry (en sync con AudioClip::logicalPath()
// que el factory rellena con el mismo string).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/audio/clips/AudioClip.h"

#include <utility>

namespace Mood {

AudioAssetId AssetManager::loadAudio(std::string_view logicalPath) {
    if (m_audioClips.contains(logicalPath)) {
        return m_audioClips.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: audio path '{}' rechazado por VFS. Fallback a missing.",
            logicalPath);
        m_audioClips.cacheAsFallback(logicalPath);
        return missingAudioId();
    }

    try {
        const std::string key{logicalPath};
        auto clip = m_audioFactory(key, fs.generic_string());
        AudioClip* clipPtr = clip.get();  // capture before move
        const AudioAssetId id = m_audioClips.add(key, std::move(clip));
        Log::assets()->info("AssetManager: cargado audio {} -> id {} ({:.2f}s, {}Hz, {}ch)",
                             logicalPath, id,
                             clipPtr->durationSeconds(),
                             clipPtr->sampleRate(),
                             clipPtr->channels());
        return id;
    } catch (const std::exception& e) {
        m_audioClips.cacheAsFallback(logicalPath);
        Log::assets()->warn(
            "AssetManager: fallback a missing.wav para '{}' ({})",
            logicalPath, e.what());
        return missingAudioId();
    }
}

AudioClip* AssetManager::getAudio(AudioAssetId id) const {
    // break-B5: loophole para retornar mutable desde const method.
    const auto& v = m_audioClips.all();
    if (v.empty()) return nullptr;
    if (id >= v.size()) return v[0].get();
    return v[id].get();
}

std::string AssetManager::audioPathOf(AudioAssetId id) const {
    return m_audioClips.pathOf(id);
}

} // namespace Mood
