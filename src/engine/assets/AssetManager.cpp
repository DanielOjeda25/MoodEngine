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
        m_texturePaths.emplace_back(k_missingPath);
        // mtime stamp para el hot-reload. Si el archivo no existe (tests con
        // mocks), file_time_type por defecto funciona igual — nunca va a ser
        // "menor" que uno real en futuros reloadChanged().
        std::error_code ec_mtime;
        m_textureMtimes.push_back(std::filesystem::last_write_time(missingFs, ec_mtime));
        // Cachear el path del missing al id 0 para que una llamada futura a
        // loadTexture("textures/missing.png") no cree una entrada duplicada.
        m_textureCache.emplace(k_missingPath, missingTextureId());
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
            auto fresh = m_factory(fs.generic_string());
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
