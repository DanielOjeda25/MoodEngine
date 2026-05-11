// F2H24 Bloque C: nucleo de AssetManager. Lifecycle del manager partido
// en archivos parciales por tipo de asset:
//   _Texture.cpp  — loadTexture + loadEmbeddedTexture + pathOf + getTexture
//   _Audio.cpp    — loadAudio + audioPathOf + getAudio
//   _Mesh.cpp     — loadMesh + getMesh + meshPathOf
//   _Prefab.cpp   — loadPrefab + getPrefab + prefabPathOf
//   _Material.cpp — load/loadFromTexture/createFromTexture/createForMesh
//                   /create/get/pathOf/saveMaterial
//
// Aca solo viven los helpers compartidos por todos:
// - ctor + dtor (inicializa todos los slots 0 / fallbacks).
// - createDynamicMesh (helper para callers que necesitan generar mesh
//   sin pasar por el cache).
// - reloadChanged (hot-reload de texturas, ataca m_textureMtimes).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/assets/primitives/PrimitiveMeshes.h"
#include "engine/animation/clips/AnimationClip.h"  // F2H49
#include "engine/audio/clips/AudioClip.h"
#include "engine/dialog/DialogAsset.h"  // F2H48
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/serialization/PrefabSerializer.h"

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
// F2H48: sentinela del dialog fallback (slot 0). Asset sin nodos.
constexpr const char* k_emptyDialogPath    = "__empty_dialog";
// F2H49: sentinela del AnimationClip fallback (slot 0). Sin tracks.
constexpr const char* k_emptyAnimClipPath  = "__empty_clip";
// Sentinela del material fallback (slot 0). Albedo blanco, mate medio.
constexpr const char* k_defaultMaterialPath = "__default_material";

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
                            MeshFactory meshFactory,
                            TextureMemoryFactory textureMemoryFactory)
    : m_vfs(std::filesystem::path(std::move(rootDir))),
      m_textureFactory(std::move(textureFactory)),
      m_textureMemoryFactory(std::move(textureMemoryFactory)),
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

    // ---- Slot reservado: esfera primitiva (Hito 17) ----
    {
        auto sphereData = createSphereMesh(32u);
        auto sphereMesh = std::make_unique<MeshAsset>();
        sphereMesh->logicalPath = "__primitive_sphere";
        SubMesh sm{};
        sm.materialIndex = 0;
        u32 strideFloats = 0;
        for (const auto& a : sphereData.attributes) strideFloats += a.componentCount;
        sm.vertexCount = static_cast<u32>(sphereData.vertices.size() / strideFloats);
        sm.mesh = m_meshFactory(sphereData.vertices, sphereData.attributes);
        if (sm.mesh != nullptr) {
            sphereMesh->submeshes.push_back(std::move(sm));
            m_primitiveSphereId = static_cast<MeshAssetId>(m_meshes.size());
            m_meshes.emplace_back(std::move(sphereMesh));
            m_meshCache.emplace("__primitive_sphere", m_primitiveSphereId);
            Log::assets()->info(
                "AssetManager: esfera primitiva generada en slot {} ({} verts)",
                m_primitiveSphereId, sm.vertexCount);
        } else {
            Log::assets()->warn("AssetManager: MeshFactory devolvio null para la esfera (skip)");
        }
    }

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

    // ---- Slot 0 material: default (Hito 17). Albedo blanco, mate medio.
    //      Cualquier MeshRenderer sin material asignado cae aca. Render
    //      con este material se ve dieléctrico mate (similar al Phong
    //      previo con uShininess=32).
    {
        auto def = std::make_unique<MaterialAsset>();
        def->logicalPath = k_defaultMaterialPath;
        def->albedoTint = glm::vec3(1.0f);
        def->metallicMult  = 0.0f;
        def->roughnessMult = 0.5f;
        def->aoMult = 1.0f;
        // El default es tambien el "material missing": si una entidad lo
        // usa por no tener material asignado, queremos que el patron magenta
        // de missing.png salga visible (no blanco puro disfrazado de feature).
        def->useAlbedoMap = true;
        m_materials.emplace_back(std::move(def));
        m_materialPaths.emplace_back(k_defaultMaterialPath);
        m_materialCache.emplace(k_defaultMaterialPath, missingMaterialId());
    }
    Log::assets()->info("AssetManager: material default generado en slot 0");

    // ---- Slot 0 dialog (F2H48): asset vacio sin nodos / sin start_node.
    //      Sirve como fallback cuando un id es invalido o el archivo no
    //      se pudo parsear. DialogSystem::start lo rechaza por start_node
    //      invalido — el caller espera ese path.
    {
        auto empty = std::make_unique<Dialog::Asset>();
        empty->metadata().name = "(empty)";
        m_dialogs.emplace_back(std::move(empty));
        m_dialogPaths.emplace_back(k_emptyDialogPath);
        m_dialogCache.emplace(k_emptyDialogPath, missingDialogId());
    }
    Log::assets()->info("AssetManager: dialog 'vacio' generado en slot 0");

    // ---- Slot 0 AnimationClip (F2H49): clip vacio sin tracks.
    //      `AnimationSystem` puede recibir este id sin crashear cuando un
    //      asset falla al cargar — al no haber tracks, el evaluate
    //      mantiene el localBindTransform de todos los huesos (T-pose).
    {
        auto empty = std::make_unique<AnimationClip>();
        empty->name = "(empty)";
        empty->duration = 0.0f;
        m_animationClips.emplace_back(std::move(empty));
        m_animationClipPaths.emplace_back(k_emptyAnimClipPath);
        m_animationClipCache.emplace(k_emptyAnimClipPath,
                                      missingAnimationClipId());
    }
    Log::assets()->info("AssetManager: animation clip 'vacio' generado en slot 0");
}

AssetManager::~AssetManager() = default;

std::unique_ptr<IMesh> AssetManager::createDynamicMesh(
    const std::vector<f32>& vertices,
    const std::vector<VertexAttribute>& attributes) const {
    if (!m_meshFactory) return nullptr;
    return m_meshFactory(vertices, attributes);
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
