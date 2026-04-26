#pragma once

// AssetManager: catalogo central de assets cargados (Hito 5 Bloque 2).
// Por ahora solo texturas; modelos/shaders/sonidos entran en hitos siguientes.
//
// Los assets se piden por path logico (relativo a la raiz de assets/). Si el
// archivo no existe o la carga falla, se devuelve el id de la textura
// "missing" (chequered rosa/negro) y se loguea en el canal `assets`. No lanza.
//
// El VFS del Bloque 3 va a reemplazar la resolucion simple `assets/<path>`
// por un lookup mas completo; la API publica no cambia.

#include "core/Types.h"
#include "engine/render/RendererTypes.h"
#include "platform/VFS.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Mood {

class AudioClip;

class IMesh;
class ITexture;
struct MeshAsset;
struct SavedPrefab;

/// @brief Identificador estable de una textura dentro de un `AssetManager`.
///        Valor 0 se reserva para la textura "missing": pedir `getTexture(0)`
///        siempre devuelve algo renderizable.
using TextureAssetId = u32;

/// @brief Identificador estable de un AudioClip. Valor 0 reservado para el
///        clip "missing" (silencio de 100 ms) para que `getAudio(0)` nunca
///        sea null.
using AudioAssetId = u32;

/// @brief Identificador estable de un MeshAsset. Valor 0 reservado para el
///        mesh "missing" (cubo del Hito 3) para que `getMesh(0)` nunca sea
///        null.
using MeshAssetId = u32;

/// @brief Identificador estable de un Prefab (Hito 14). Valor 0 reservado
///        para un "prefab vacio" (root sin componentes), asi `getPrefab(0)`
///        nunca devuelve null. Loadeo lazy: el `.moodprefab` se parsea al
///        invocar `loadPrefab`, no al escanear el directorio.
using PrefabAssetId = u32;

class AssetManager {
public:
    /// @brief Factoria de texturas: recibe un path del filesystem (resuelto
    ///        por el VFS) y devuelve una ITexture lista. Inyectable para
    ///        poder testear AssetManager sin contexto GL (mocks).
    using TextureFactory =
        std::function<std::unique_ptr<ITexture>(const std::string& filesystemPath)>;

    /// @brief Factoria de audio clips. Recibe path logico + filesystem y
    ///        devuelve un AudioClip con la metadata poblada. Inyectable para
    ///        testear sin hardware (mocks que devuelven clip con duracion=0).
    using AudioClipFactory =
        std::function<std::unique_ptr<AudioClip>(const std::string& logicalPath,
                                                  const std::string& filesystemPath)>;

    /// @brief Factoria de IMesh (VAO/VBO). Recibe un buffer interleaved de
    ///        floats + layout y devuelve un mesh listo para draw. Produccion
    ///        pasa `OpenGLMesh`; tests inyectan mocks que solo guardan el
    ///        vertex count.
    using MeshFactory =
        std::function<std::unique_ptr<IMesh>(const std::vector<f32>& vertices,
                                              const std::vector<VertexAttribute>& attributes)>;

    /// @brief Construye el manager y carga los fallbacks (missing.png,
    ///        missing.wav y missing mesh = cubo). Si alguno falla, lanza: la
    ///        instalacion esta rota.
    /// @param rootDir Carpeta raiz de assets (default: "assets").
    /// @param textureFactory Factoria de texturas. EditorApplication pasa
    ///        una que crea `OpenGLTexture`. Los tests inyectan mocks.
    /// @param audioFactory Factoria de audio clips. Default: construye
    ///        `AudioClip` directo (no requiere hardware, solo inspecciona
    ///        metadata con ma_decoder). Los tests pueden inyectar stubs.
    /// @param meshFactory Factoria de IMesh. Requerida para poder generar
    ///        el mesh de fallback (cubo primitivo) y para importar modelos
    ///        via assimp.
    AssetManager(std::string rootDir,
                 TextureFactory textureFactory,
                 AudioClipFactory audioFactory = {},
                 MeshFactory meshFactory = {});
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    /// @brief Carga una textura por path logico (ej. "textures/grid.png")
    ///        o devuelve el id cacheado si ya estaba cargada. En fallo,
    ///        devuelve `missingTextureId()` y loguea en `assets`.
    TextureAssetId loadTexture(std::string_view logicalPath);

    /// @brief Devuelve el ITexture para el id dado. Nunca es null:
    ///        ids invalidos (fuera de rango) caen al fallback missing.
    ITexture* getTexture(TextureAssetId id) const;

    /// @brief Path logico con el que se cargo la textura. Usado por los
    ///        serializers para persistir la referencia como string estable
    ///        entre sesiones (los ids son volatiles). Id invalido o missing
    ///        devuelve el path del missing.
    std::string pathOf(TextureAssetId id) const;

    /// @brief Id de la textura de fallback. Siempre vale 0.
    TextureAssetId missingTextureId() const { return 0; }

    /// @brief Cantidad de texturas en cache (incluye missing).
    usize textureCount() const { return m_textures.size(); }

    /// @brief Revisa el mtime de cada PNG cargado y re-invoca la factoria
    ///        para las texturas cuyo archivo cambio en disco. Conserva el
    ///        `TextureAssetId` (swapea el unique_ptr); callsites que
    ///        guardaron el id siguen viendo la textura actualizada.
    ///        IMPORTANTE: este metodo borra los GLuint de las texturas
    ///        cambiadas. Llamar ENTRE frames (antes de `beginFrame`) para
    ///        evitar que ImGui dibuje con un handle recien destruido.
    /// @return cantidad de texturas efectivamente recargadas.
    usize reloadChanged();

    // ---- Audio ----

    /// @brief Carga (o devuelve cacheado) un AudioClip por path logico. En
    ///        fallo devuelve `missingAudioId()` (silencio de 100 ms) y loguea
    ///        al canal assets.
    AudioAssetId loadAudio(std::string_view logicalPath);

    /// @brief Devuelve el AudioClip del id. Nunca null: ids invalidos caen
    ///        al fallback missing.
    AudioClip* getAudio(AudioAssetId id) const;

    /// @brief Id del clip fallback ("audio/missing.wav"). Siempre vale 0.
    AudioAssetId missingAudioId() const { return 0; }

    /// @brief Cantidad de clips cacheados (incluye missing).
    usize audioCount() const { return m_audioClips.size(); }

    /// @brief Path logico con el que se cargo un clip (para serializadores y
    ///        el AssetBrowser). Id invalido devuelve el path de missing.
    std::string audioPathOf(AudioAssetId id) const;

    // ---- Mesh (Hito 10) ----

    /// @brief Carga (o devuelve cacheado) un MeshAsset por path logico (p.ej.
    ///        "meshes/suzanne.obj"). En fallo devuelve `missingMeshId()` (cubo
    ///        primitivo) y loguea en el canal assets. Imports via assimp.
    MeshAssetId loadMesh(std::string_view logicalPath);

    /// @brief Devuelve el MeshAsset del id. Nunca null: ids invalidos caen al
    ///        fallback missing (cubo).
    MeshAsset* getMesh(MeshAssetId id) const;

    /// @brief Id del mesh fallback (cubo generado en el constructor). Siempre
    ///        vale 0.
    MeshAssetId missingMeshId() const { return 0; }

    /// @brief Path logico asociado al mesh. Slot 0 devuelve el path sentinela
    ///        "__missing_cube" (no existe en disco; solo uso interno del
    ///        serializer para detectar "sin mesh real cargado").
    std::string meshPathOf(MeshAssetId id) const;

    /// @brief Cantidad de meshes cacheados (incluye missing).
    usize meshCount() const { return m_meshes.size(); }

    // ---- Prefab (Hito 14) ----

    /// @brief Carga (o devuelve cacheado) un prefab por path logico (p.ej.
    ///        "prefabs/torch.moodprefab"). En fallo devuelve `missingPrefabId()`
    ///        (prefab vacio) y loguea al canal `assets`.
    PrefabAssetId loadPrefab(std::string_view logicalPath);

    /// @brief Devuelve el SavedPrefab del id. Nunca null: ids invalidos caen
    ///        al slot 0 (prefab vacio).
    const SavedPrefab* getPrefab(PrefabAssetId id) const;

    /// @brief Id del prefab vacio (placeholder). Siempre vale 0.
    PrefabAssetId missingPrefabId() const { return 0; }

    /// @brief Path logico con el que se cargo el prefab. Slot 0 devuelve el
    ///        sentinela `"__empty_prefab"` (no existe en disco).
    std::string prefabPathOf(PrefabAssetId id) const;

    /// @brief Cantidad de prefabs cacheados (incluye el slot 0).
    usize prefabCount() const { return m_prefabs.size(); }

private:
    VFS m_vfs;
    TextureFactory m_textureFactory;
    AudioClipFactory m_audioFactory;
    MeshFactory m_meshFactory;

    // Texturas (Hito 5).
    std::unordered_map<std::string, TextureAssetId> m_textureCache;
    std::vector<std::unique_ptr<ITexture>> m_textures; // [0] = missing
    std::vector<std::string> m_texturePaths;           // paralelo a m_textures
    std::vector<std::filesystem::file_time_type> m_textureMtimes; // paralelo

    // Audio (Hito 9).
    std::unordered_map<std::string, AudioAssetId> m_audioCache;
    std::vector<std::unique_ptr<AudioClip>> m_audioClips; // [0] = missing.wav

    // Mesh (Hito 10). [0] = cubo primitivo (fallback).
    std::unordered_map<std::string, MeshAssetId> m_meshCache;
    std::vector<std::unique_ptr<MeshAsset>> m_meshes;

    // Prefab (Hito 14). [0] = prefab vacio (root sin componentes).
    std::unordered_map<std::string, PrefabAssetId> m_prefabCache;
    std::vector<std::unique_ptr<SavedPrefab>> m_prefabs;
    std::vector<std::string> m_prefabPaths; // paralelo a m_prefabs
};

} // namespace Mood
