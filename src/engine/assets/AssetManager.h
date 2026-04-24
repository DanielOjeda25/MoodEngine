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

class ITexture;

/// @brief Identificador estable de una textura dentro de un `AssetManager`.
///        Valor 0 se reserva para la textura "missing": pedir `getTexture(0)`
///        siempre devuelve algo renderizable.
using TextureAssetId = u32;

/// @brief Identificador estable de un AudioClip. Valor 0 reservado para el
///        clip "missing" (silencio de 100 ms) para que `getAudio(0)` nunca
///        sea null.
using AudioAssetId = u32;

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

    /// @brief Construye el manager y carga los fallbacks (missing.png y
    ///        missing.wav). Si alguno falla, lanza: la instalacion esta rota.
    /// @param rootDir Carpeta raiz de assets (default: "assets").
    /// @param textureFactory Factoria de texturas. EditorApplication pasa
    ///        una que crea `OpenGLTexture`. Los tests inyectan mocks.
    /// @param audioFactory Factoria de audio clips. Default: construye
    ///        `AudioClip` directo (no requiere hardware, solo inspecciona
    ///        metadata con ma_decoder). Los tests pueden inyectar stubs.
    AssetManager(std::string rootDir,
                 TextureFactory textureFactory,
                 AudioClipFactory audioFactory = {});
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

private:
    VFS m_vfs;
    TextureFactory m_textureFactory;
    AudioClipFactory m_audioFactory;

    // Texturas (Hito 5).
    std::unordered_map<std::string, TextureAssetId> m_textureCache;
    std::vector<std::unique_ptr<ITexture>> m_textures; // [0] = missing
    std::vector<std::string> m_texturePaths;           // paralelo a m_textures
    std::vector<std::filesystem::file_time_type> m_textureMtimes; // paralelo

    // Audio (Hito 9).
    std::unordered_map<std::string, AudioAssetId> m_audioCache;
    std::vector<std::unique_ptr<AudioClip>> m_audioClips; // [0] = missing.wav
};

} // namespace Mood
