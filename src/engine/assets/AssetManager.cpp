#include "engine/assets/AssetManager.h"

#include "core/Log.h"
#include "engine/render/ITexture.h"
#include "engine/render/opengl/OpenGLTexture.h"

#include <filesystem>
#include <stdexcept>

namespace Mood {

namespace {

constexpr const char* k_missingPath = "textures/missing.png";

std::string joinRoot(const std::string& root, std::string_view path) {
    std::filesystem::path p = std::filesystem::path(root) / std::string(path);
    return p.generic_string();
}

} // namespace

AssetManager::AssetManager(std::string rootDir)
    : m_rootDir(std::move(rootDir)) {
    // Slot 0 reservado para la textura missing. Si no carga, la instalacion
    // esta rota: dejar que la excepcion se propague.
    const std::string missingFs = joinRoot(m_rootDir, k_missingPath);
    try {
        m_textures.emplace_back(std::make_unique<OpenGLTexture>(missingFs));
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("AssetManager: no se pudo cargar missing.png ('") +
            missingFs + "'): " + e.what());
    }
    Log::assets()->info("AssetManager: fallback 'missing' cargado desde {}", missingFs);
}

AssetManager::~AssetManager() = default;

TextureAssetId AssetManager::loadTexture(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_textureCache.find(key); it != m_textureCache.end()) {
        return it->second;
    }

    const std::string fsPath = joinRoot(m_rootDir, logicalPath);
    try {
        auto tex = std::make_unique<OpenGLTexture>(fsPath);
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
