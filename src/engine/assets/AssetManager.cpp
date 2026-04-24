#include "engine/assets/AssetManager.h"

#include "core/Log.h"
#include "engine/audio/AudioClip.h"
#include "engine/render/ITexture.h"

#include <stdexcept>
#include <utility>

namespace Mood {

namespace {

constexpr const char* k_missingTexturePath = "textures/missing.png";
constexpr const char* k_missingAudioPath   = "audio/missing.wav";

} // namespace

AssetManager::AssetManager(std::string rootDir,
                            TextureFactory textureFactory,
                            AudioClipFactory audioFactory)
    : m_vfs(std::filesystem::path(std::move(rootDir))),
      m_textureFactory(std::move(textureFactory)),
      m_audioFactory(std::move(audioFactory)) {
    if (!m_textureFactory) {
        throw std::runtime_error("AssetManager: se requiere una TextureFactory");
    }
    // Audio factory por defecto: construye AudioClip directo. No requiere
    // hardware; `AudioClip` inspecciona metadata con `ma_decoder_init_file`.
    if (!m_audioFactory) {
        m_audioFactory = [](const std::string& logical, const std::string& fs) {
            return std::make_unique<AudioClip>(logical, fs);
        };
    }

    // ---- Slot 0 textura: missing.png (obligatorio) ----
    const auto missingFs = m_vfs.resolve(k_missingTexturePath);
    if (missingFs.empty()) {
        throw std::runtime_error(
            std::string("AssetManager: path logico 'textures/missing.png' rechazado por VFS"));
    }
    try {
        m_textures.emplace_back(m_textureFactory(missingFs.generic_string()));
        m_texturePaths.emplace_back(k_missingTexturePath);
        std::error_code ec_mtime;
        m_textureMtimes.push_back(std::filesystem::last_write_time(missingFs, ec_mtime));
        m_textureCache.emplace(k_missingTexturePath, missingTextureId());
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AssetManager: no se pudo cargar missing.png ('") +
            missingFs.generic_string() + "'): " + e.what());
    }
    Log::assets()->info("AssetManager: fallback 'missing' cargado desde {}", missingFs.generic_string());

    // ---- Slot 0 audio: missing.wav (silencio 100ms; obligatorio) ----
    const auto missingAudioFs = m_vfs.resolve(k_missingAudioPath);
    if (missingAudioFs.empty()) {
        throw std::runtime_error(
            std::string("AssetManager: path logico 'audio/missing.wav' rechazado por VFS"));
    }
    try {
        m_audioClips.emplace_back(m_audioFactory(k_missingAudioPath,
                                                  missingAudioFs.generic_string()));
        m_audioCache.emplace(k_missingAudioPath, missingAudioId());
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AssetManager: no se pudo cargar missing.wav ('") +
            missingAudioFs.generic_string() + "'): " + e.what());
    }
    Log::assets()->info("AssetManager: fallback audio 'missing' cargado desde {}",
                         missingAudioFs.generic_string());
}

AssetManager::~AssetManager() = default;

TextureAssetId AssetManager::loadTexture(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_textureCache.find(key); it != m_textureCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: path logico '{}' rechazado por VFS (unsafe). Fallback a missing.",
            logicalPath);
        m_textureCache.emplace(key, missingTextureId());
        return missingTextureId();
    }

    try {
        auto tex = m_textureFactory(fs.generic_string());
        const TextureAssetId id = static_cast<TextureAssetId>(m_textures.size());
        m_textures.push_back(std::move(tex));
        m_texturePaths.push_back(key);
        std::error_code ec_mtime;
        m_textureMtimes.push_back(std::filesystem::last_write_time(fs, ec_mtime));
        m_textureCache.emplace(key, id);
        Log::assets()->info("AssetManager: cargada texture {} -> id {}", logicalPath, id);
        return id;
    } catch (const std::exception& e) {
        // Fallo de carga: cachear la ruta apuntando al missing (id 0) para
        // no reintentar cada frame y devolver algo renderizable.
        m_textureCache.emplace(key, missingTextureId());
        Log::assets()->warn(
            "AssetManager: fallback a missing.png para '{}' ({})",
            logicalPath, e.what());
        return missingTextureId();
    }
}

ITexture* AssetManager::getTexture(TextureAssetId id) const {
    if (id >= m_textures.size()) {
        return m_textures[missingTextureId()].get();
    }
    return m_textures[id].get();
}

std::string AssetManager::pathOf(TextureAssetId id) const {
    if (id >= m_texturePaths.size()) {
        return m_texturePaths[missingTextureId()];
    }
    return m_texturePaths[id];
}

// ----------------------------------------------------------------------------
// Audio
// ----------------------------------------------------------------------------

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

usize AssetManager::reloadChanged() {
    usize reloaded = 0;
    for (TextureAssetId id = 0; id < m_textures.size(); ++id) {
        const auto fs = m_vfs.resolve(m_texturePaths[id]);
        if (fs.empty()) continue; // path logico invalido (no deberia pasar)

        std::error_code ec;
        const auto mtime = std::filesystem::last_write_time(fs, ec);
        if (ec) {
            // Archivo desaparecio. No lo reemplazamos con el missing
            // (arruinaria referencias validas); solo logueamos una vez.
            continue;
        }
        if (mtime == m_textureMtimes[id]) continue;

        try {
            auto fresh = m_textureFactory(fs.generic_string());
            m_textures[id] = std::move(fresh); // dtor del viejo -> glDeleteTextures
            m_textureMtimes[id] = mtime;
            ++reloaded;
            Log::assets()->info("AssetManager: recargada '{}' (id {})",
                                 m_texturePaths[id], id);
        } catch (const std::exception& e) {
            Log::assets()->warn("AssetManager: recarga fallo para '{}': {}",
                                m_texturePaths[id], e.what());
        }
    }
    return reloaded;
}

} // namespace Mood
