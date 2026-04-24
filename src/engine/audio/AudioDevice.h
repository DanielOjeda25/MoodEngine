#pragma once

// AudioDevice: wrapper RAII sobre `ma_engine` (API high-level de miniaudio
// que combina device + mixer + decoder + sonidos posicionales).
//
// Diseno null-safe: si `ma_engine_init` falla (driver roto, no hay hardware,
// CI sin audio), el AudioDevice queda en modo "mute" — play()/stop() son
// no-ops, el resto del motor sigue funcionando.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <memory>
#include <string>

// Forward decls: evitamos arrastrar miniaudio.h en headers. La impl los
// resuelve en AudioDevice.cpp.
struct ma_engine;
struct ma_sound;

namespace Mood {

class AudioClip;

/// @brief Handle opaco que identifica una reproduccion en curso. 0 = invalido
///        (reproduccion que nunca arrancó o que ya termino). Es estable hasta
///        que el AudioDevice decide recolectar el slot.
using SoundHandle = u32;

class AudioDevice {
public:
    /// @brief Inicializa `ma_engine` con device default. Si falla, el objeto
    ///        queda en estado mute: `isValid()` devuelve false y play() no
    ///        hace nada. No lanza.
    AudioDevice();
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    /// @brief True si el engine inicializo correctamente y puede reproducir.
    bool isValid() const;

    /// @brief Sample rate configurado por el device (44100, 48000, etc.).
    u32 sampleRate() const;

    /// @brief Canales del output (1 = mono, 2 = stereo).
    u32 channels() const;

    /// @brief Cantidad de reproducciones activas (post-sweep). Expuesto
    ///        para tests y debug, no es parte de la API estable.
    usize activeSoundCount() const;

    /// @brief Reproduce el clip. Devuelve un handle para `stop()` posterior.
    ///        En modo mute o con clip null, devuelve 0 silencioso.
    /// @param clip        Clip pre-decodificado a reproducir.
    /// @param volume      Lineal [0..1], se escala al master.
    /// @param loop        Si true, reproduce en loop hasta stop().
    /// @param is3D        Si true, la posicion importa para atenuacion 3D.
    /// @param worldPos    Posicion en el mundo (solo relevante si is3D).
    SoundHandle play(const AudioClip& clip, f32 volume, bool loop,
                     bool is3D, const glm::vec3& worldPos);

    /// @brief Detiene una reproduccion activa. No-op si el handle no es valido
    ///        (la reproduccion ya termino naturalmente o nunca existio).
    void stop(SoundHandle h);

    /// @brief Detiene todas las reproducciones activas. Util al limpiar la
    ///        escena (rebuildSceneFromMap) para que los loops no queden
    ///        huerfanos cuando las entidades se destruyen.
    void stopAll();

    /// @brief Actualiza la posicion 3D de un sonido activo (si es is3D).
    ///        No-op si el handle no es valido.
    void setSoundPosition(SoundHandle h, const glm::vec3& worldPos);

    /// @brief Configura la posicion + orientacion del listener. Llamar cada
    ///        frame desde el sistema que sabe cual es la camara activa.
    void setListener(const glm::vec3& worldPos,
                     const glm::vec3& forward,
                     const glm::vec3& up);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mood
