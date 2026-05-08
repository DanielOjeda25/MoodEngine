// F2H24 Bloque C: AssetManager — operaciones sobre clips de audio.
// loadAudio / getAudio / audioPathOf.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/audio/clips/AudioClip.h"

#include <utility>

namespace Mood {

AudioAssetId AssetManager::loadAudio(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_audioCache.find(key); it != m_audioCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: audio path '{}' rechazado por VFS. Fallback a missing.",
            logicalPath);
        m_audioCache.emplace(key, missingAudioId());
        return missingAudioId();
    }

    try {
        auto clip = m_audioFactory(key, fs.generic_string());
        const AudioAssetId id = static_cast<AudioAssetId>(m_audioClips.size());
        m_audioClips.push_back(std::move(clip));
        m_audioCache.emplace(key, id);
        Log::assets()->info("AssetManager: cargado audio {} -> id {} ({:.2f}s, {}Hz, {}ch)",
                             logicalPath, id,
                             m_audioClips.back()->durationSeconds(),
                             m_audioClips.back()->sampleRate(),
                             m_audioClips.back()->channels());
        return id;
    } catch (const std::exception& e) {
        m_audioCache.emplace(key, missingAudioId());
        Log::assets()->warn(
            "AssetManager: fallback a missing.wav para '{}' ({})",
            logicalPath, e.what());
        return missingAudioId();
    }
}

AudioClip* AssetManager::getAudio(AudioAssetId id) const {
    if (id >= m_audioClips.size()) {
        return m_audioClips[missingAudioId()].get();
    }
    return m_audioClips[id].get();
}

std::string AssetManager::audioPathOf(AudioAssetId id) const {
    if (id >= m_audioClips.size()) {
        return m_audioClips[missingAudioId()]->logicalPath();
    }
    return m_audioClips[id]->logicalPath();
}

} // namespace Mood
