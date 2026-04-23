#include "engine/assets/AssetManager.h"

#include "core/Log.h"
#include "engine/render/ITexture.h"

#include <stdexcept>
#include <utility>

namespace Mood {

namespace {

constexpr const char* k_missingPath = "textures/missing.png";

} // namespace

AssetManager::AssetManager(std::string rootDir, TextureFactory factory)
    : m_vfs(std::filesystem::path(std::move(rootDir))),
      m_factory(std::move(factory)) {
    if (!m_factory) {
        throw std::runtime_error("AssetManager: se requiere una TextureFactory");
    }

    // Slot 0 reservado para la textura missing. Si no carga, la instalacion
    // esta rota: dejar que la excepcion se propague.
    const auto missingFs = m_vfs.resolve(k_missingPath);
    if (missingFs.empty()) {
        throw std::runtime_error(
            std::string("AssetManager: path logico 'textures/missing.png' rechazado por VFS"));
    }
    try {
        m_textures.emplace_back(m_factory(missingFs.generic_string()));
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AssetManager: no se pudo cargar missing.png ('") +
            missingFs.generic_string() + "'): " + e.what());
    }
    Log::assets()->info("AssetManager: fallback 'missing' cargado desde {}", missingFs.generic_string());
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
        auto tex = m_factory(fs.generic_string());
        const TextureAssetId id = static_cast<TextureAssetId>(m_textures.size());
        m_textures.push_back(std::move(tex));
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

} // namespace Mood
