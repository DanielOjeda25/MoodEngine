#include "engine/assets/AssetManager.h"

#include "core/Log.h"
#include "engine/assets/MeshLoader.h"
#include "engine/assets/PrimitiveMeshes.h"
#include "engine/audio/AudioClip.h"
#include "engine/render/IMesh.h"
#include "engine/render/ITexture.h"
#include "engine/render/MeshAsset.h"
#include "engine/serialization/PrefabSerializer.h"

#include <stdexcept>
#include <utility>

namespace Mood {

namespace {

constexpr const char* k_missingTexturePath = "textures/missing.png";
constexpr const char* k_missingAudioPath   = "audio/missing.wav";
// Sentinela para el mesh fallback. No es un path real del VFS: el mesh se
// genera programaticamente (cubo primitivo). El serializer usa este string
// para distinguir "mesh por defecto" de uno importado.
constexpr const char* k_missingMeshPath    = "__missing_cube";
// Sentinela del prefab fallback (slot 0). Tampoco existe en disco; el
// AssetManager genera un SavedPrefab vacio para que `getPrefab(0)` no sea
// null.
constexpr const char* k_emptyPrefabPath    = "__empty_prefab";

/// @brief Mesh stub sin GL — permite que los tests construyan AssetManager
///        sin contexto OpenGL (default de MeshFactory). Produccion siempre
///        pasa una factoria real que crea OpenGLMesh.
class NullMesh : public IMesh {
public:
    explicit NullMesh(u32 count) : m_count(count) {}
    void bind() const override {}
    void unbind() const override {}
    u32 vertexCount() const override { return m_count; }
private:
    u32 m_count;
};

} // namespace

AssetManager::AssetManager(std::string rootDir,
                            TextureFactory textureFactory,
                            AudioClipFactory audioFactory,
                            MeshFactory meshFactory)
    : m_vfs(std::filesystem::path(std::move(rootDir))),
      m_textureFactory(std::move(textureFactory)),
      m_audioFactory(std::move(audioFactory)),
      m_meshFactory(std::move(meshFactory)) {
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
    // Mesh factory por defecto: produce NullMesh (sin GL). Tests pueden
    // construir AssetManager sin contexto OpenGL; produccion siempre pasa
    // una factoria real que cree `OpenGLMesh`.
    if (!m_meshFactory) {
        m_meshFactory = [](const std::vector<f32>& verts,
                           const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
            u32 stride = 0;
            for (const auto& a : attrs) stride += a.componentCount;
            const u32 count = (stride > 0)
                ? static_cast<u32>(verts.size() / stride) : 0;
            return std::make_unique<NullMesh>(count);
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

    // ---- Slot 0 mesh: cubo primitivo (no requiere archivo) ----
    // Se usa como fallback cuando assimp falla y como mesh por defecto del
    // render scene-driven para entidades-tile (Hito 10 Bloque 4).
    {
        auto cubeData = createCubeMesh();
        auto missingMesh = std::make_unique<MeshAsset>();
        missingMesh->logicalPath = k_missingMeshPath;
        SubMesh sm{};
        sm.materialIndex = 0;
        // Stride en floats = suma de componentCount de los attributes. Hito 11
        // subio de 8 (pos+color+uv) a 11 (+normal).
        u32 strideFloats = 0;
        for (const auto& a : cubeData.attributes) strideFloats += a.componentCount;
        sm.vertexCount = static_cast<u32>(cubeData.vertices.size() / strideFloats);
        sm.mesh = m_meshFactory(cubeData.vertices, cubeData.attributes);
        if (sm.mesh == nullptr) {
            throw std::runtime_error(
                "AssetManager: MeshFactory devolvio null para el cubo fallback");
        }
        missingMesh->submeshes.push_back(std::move(sm));
        m_meshes.emplace_back(std::move(missingMesh));
        m_meshCache.emplace(k_missingMeshPath, missingMeshId());
    }
    Log::assets()->info("AssetManager: fallback mesh 'cubo primitivo' generado en slot 0");

    // ---- Slot 0 prefab: SavedPrefab vacio (no requiere archivo). Sirve
    //      como "default" cuando un id de prefab es invalido o el archivo
    //      no se pudo parsear. Su `root` queda tag vacio + Transform default.
    {
        auto empty = std::make_unique<SavedPrefab>();
        empty->name = "(empty)";
        empty->root.tag = ""; // se completa en spawn si se usa como fallback
        m_prefabs.emplace_back(std::move(empty));
        m_prefabPaths.emplace_back(k_emptyPrefabPath);
        m_prefabCache.emplace(k_emptyPrefabPath, missingPrefabId());
    }
    Log::assets()->info("AssetManager: prefab 'vacio' generado en slot 0");
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

// ----------------------------------------------------------------------------
// Mesh (Hito 10)
// ----------------------------------------------------------------------------

MeshAssetId AssetManager::loadMesh(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_meshCache.find(key); it != m_meshCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: mesh path '{}' rechazado por VFS. Fallback a missing (cubo).",
            logicalPath);
        m_meshCache.emplace(key, missingMeshId());
        return missingMeshId();
    }

    auto asset = loadMeshWithAssimp(key, fs.generic_string(), m_meshFactory);
    if (asset == nullptr) {
        // Loggeo ya emitido por loadMeshWithAssimp. Cacheamos el fallback
        // para no reintentar cada frame.
        m_meshCache.emplace(key, missingMeshId());
        return missingMeshId();
    }

    const MeshAssetId id = static_cast<MeshAssetId>(m_meshes.size());
    m_meshes.push_back(std::move(asset));
    m_meshCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado mesh {} -> id {}", logicalPath, id);
    return id;
}

MeshAsset* AssetManager::getMesh(MeshAssetId id) const {
    if (id >= m_meshes.size()) {
        return m_meshes[missingMeshId()].get();
    }
    return m_meshes[id].get();
}

std::string AssetManager::meshPathOf(MeshAssetId id) const {
    if (id >= m_meshes.size()) {
        return m_meshes[missingMeshId()]->logicalPath;
    }
    return m_meshes[id]->logicalPath;
}

// ----------------------------------------------------------------------------
// Hot-reload de texturas (Hito 5 Bloque 2)
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// Prefab (Hito 14)
// ----------------------------------------------------------------------------

PrefabAssetId AssetManager::loadPrefab(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_prefabCache.find(key); it != m_prefabCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: prefab path '{}' rechazado por VFS. Fallback al vacio.",
            logicalPath);
        m_prefabCache.emplace(key, missingPrefabId());
        return missingPrefabId();
    }

    auto loaded = PrefabSerializer::load(fs);
    if (!loaded.has_value()) {
        // Loggeo ya emitido por el serializer.
        m_prefabCache.emplace(key, missingPrefabId());
        return missingPrefabId();
    }

    auto stored = std::make_unique<SavedPrefab>(std::move(*loaded));
    const PrefabAssetId id = static_cast<PrefabAssetId>(m_prefabs.size());
    m_prefabs.push_back(std::move(stored));
    m_prefabPaths.push_back(key);
    m_prefabCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado prefab {} -> id {}", logicalPath, id);
    return id;
}

const SavedPrefab* AssetManager::getPrefab(PrefabAssetId id) const {
    if (id >= m_prefabs.size()) {
        return m_prefabs[missingPrefabId()].get();
    }
    return m_prefabs[id].get();
}

std::string AssetManager::prefabPathOf(PrefabAssetId id) const {
    if (id >= m_prefabPaths.size()) {
        return m_prefabPaths[missingPrefabId()];
    }
    return m_prefabPaths[id];
}

} // namespace Mood
