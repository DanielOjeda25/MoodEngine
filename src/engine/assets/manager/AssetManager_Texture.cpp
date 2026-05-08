// F2H24 Bloque C: AssetManager — operaciones sobre texturas.
// loadTexture / loadEmbeddedTexture / getTexture / pathOf.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/render/rhi/ITexture.h"

#include <utility>

namespace Mood {

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

TextureAssetId AssetManager::loadEmbeddedTexture(const std::string& cacheKey,
                                                    const std::vector<u8>& bytes) {
    if (auto it = m_textureCache.find(cacheKey); it != m_textureCache.end()) {
        return it->second;
    }
    if (!m_textureMemoryFactory) {
        Log::assets()->warn(
            "AssetManager: loadEmbeddedTexture('{}') sin TextureMemoryFactory. "
            "Fallback a missing.", cacheKey);
        m_textureCache.emplace(cacheKey, missingTextureId());
        return missingTextureId();
    }
    if (bytes.empty()) {
        Log::assets()->warn(
            "AssetManager: loadEmbeddedTexture('{}') con buffer vacio. "
            "Fallback a missing.", cacheKey);
        m_textureCache.emplace(cacheKey, missingTextureId());
        return missingTextureId();
    }
    try {
        auto tex = m_textureMemoryFactory(bytes, cacheKey);
        const TextureAssetId id = static_cast<TextureAssetId>(m_textures.size());
        m_textures.push_back(std::move(tex));
        m_texturePaths.push_back(cacheKey);
        // No mtime: la textura no vive en disco como archivo independiente.
        m_textureMtimes.push_back(std::filesystem::file_time_type{});
        m_textureCache.emplace(cacheKey, id);
        Log::assets()->info(
            "AssetManager: cargada textura embedded '{}' -> id {}", cacheKey, id);
        return id;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "AssetManager: fallo decodificando textura embedded '{}': {}. "
            "Fallback a missing.", cacheKey, e.what());
        m_textureCache.emplace(cacheKey, missingTextureId());
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

} // namespace Mood
