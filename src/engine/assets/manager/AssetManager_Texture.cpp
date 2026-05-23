// F2H24 Bloque C: AssetManager — operaciones sobre texturas.
// loadTexture / loadEmbeddedTexture / getTexture / pathOf.
//
// break-B5: storage delegado a AssetRegistry<ITexture>. Mtime para hot-
// reload sigue siendo un vector paralelo (m_textureMtimes) dentro de
// AssetManager — el registry no asume estado de filesystem.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/render/rhi/ITexture.h"

#include <utility>

namespace Mood {

TextureAssetId AssetManager::loadTexture(std::string_view logicalPath) {
    if (m_textures.contains(logicalPath)) {
        return m_textures.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: path logico '{}' rechazado por VFS (unsafe). Fallback a missing.",
            logicalPath);
        m_textures.cacheAsFallback(logicalPath);
        return missingTextureId();
    }

    try {
        auto tex = m_textureFactory(fs.generic_string());
        const std::string key{logicalPath};
        std::error_code ec_mtime;
        const auto mtime = std::filesystem::last_write_time(fs, ec_mtime);
        const TextureAssetId id = m_textures.add(key, std::move(tex));
        m_textureMtimes.push_back(mtime);
        Log::assets()->info("AssetManager: cargada texture {} -> id {}", logicalPath, id);
        return id;
    } catch (const std::exception& e) {
        // Fallo de carga: cachear la ruta apuntando al missing (id 0) para
        // no reintentar cada frame y devolver algo renderizable.
        m_textures.cacheAsFallback(logicalPath);
        Log::assets()->warn(
            "AssetManager: fallback a missing.png para '{}' ({})",
            logicalPath, e.what());
        return missingTextureId();
    }
}

TextureAssetId AssetManager::loadEmbeddedTexture(const std::string& cacheKey,
                                                    const std::vector<u8>& bytes) {
    if (m_textures.contains(cacheKey)) {
        return m_textures.findByPath(cacheKey);
    }
    if (!m_textureMemoryFactory) {
        Log::assets()->warn(
            "AssetManager: loadEmbeddedTexture('{}') sin TextureMemoryFactory. "
            "Fallback a missing.", cacheKey);
        m_textures.cacheAsFallback(cacheKey);
        return missingTextureId();
    }
    if (bytes.empty()) {
        Log::assets()->warn(
            "AssetManager: loadEmbeddedTexture('{}') con buffer vacio. "
            "Fallback a missing.", cacheKey);
        m_textures.cacheAsFallback(cacheKey);
        return missingTextureId();
    }
    try {
        auto tex = m_textureMemoryFactory(bytes, cacheKey);
        const TextureAssetId id = m_textures.add(cacheKey, std::move(tex));
        // No mtime: la textura no vive en disco como archivo independiente.
        m_textureMtimes.push_back(std::filesystem::file_time_type{});
        Log::assets()->info(
            "AssetManager: cargada textura embedded '{}' -> id {}", cacheKey, id);
        return id;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "AssetManager: fallo decodificando textura embedded '{}': {}. "
            "Fallback a missing.", cacheKey, e.what());
        m_textures.cacheAsFallback(cacheKey);
        return missingTextureId();
    }
}

ITexture* AssetManager::getTexture(TextureAssetId id) const {
    // break-B5: loophole para retornar mutable desde const method.
    const auto& v = m_textures.all();
    if (v.empty()) return nullptr;
    if (id >= v.size()) return v[0].get();
    return v[id].get();
}

std::string AssetManager::pathOf(TextureAssetId id) const {
    return m_textures.pathOf(id);
}

} // namespace Mood
