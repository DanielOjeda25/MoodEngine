#pragma once

// AudioClip: metadata de un archivo de audio decodificable por miniaudio.
// No es dueno del buffer de PCM — miniaudio maneja eso internamente via su
// resource manager. El AudioClip existe para que los componentes/paneles
// puedan referenciar un clip por id y leer su path + duracion sin tocar
// miniaudio directamente.

#include "core/Types.h"

#include <string>

namespace Mood {

class AudioClip {
public:
    /// @brief Construye un clip inspeccionando el archivo. Si `queryMetadata`
    ///        no se puede resolver (sin runtime miniaudio activo), los campos
    ///        de duracion/sample_rate quedan en cero. El clip sigue siendo
    ///        usable: la carga real ocurre en AudioDevice::play.
    /// @param logicalPath  Path logico (ej "audio/beep.wav").
    /// @param fsPath       Path resuelto del filesystem.
    AudioClip(std::string logicalPath, std::string fsPath);

    /// @brief Path del filesystem (lo que AudioDevice le pasa a miniaudio).
    const std::string& path() const { return m_fsPath; }

    /// @brief Path logico (el que ve el usuario en el AssetBrowser).
    const std::string& logicalPath() const { return m_logicalPath; }

    /// @brief Duracion total en segundos. 0 si no se pudo inspeccionar.
    f32 durationSeconds() const { return m_durationSeconds; }

    /// @brief Sample rate del archivo (44100, 48000, ...). 0 si no disponible.
    u32 sampleRate() const { return m_sampleRate; }

    /// @brief Canales del archivo (1=mono, 2=stereo, ...). 0 si no disponible.
    u32 channels() const { return m_channels; }

private:
    std::string m_logicalPath;
    std::string m_fsPath;
    f32 m_durationSeconds = 0.0f;
    u32 m_sampleRate = 0;
    u32 m_channels = 0;
};

} // namespace Mood
