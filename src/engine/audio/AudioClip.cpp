#include "engine/audio/AudioClip.h"

#include <miniaudio.h>

namespace Mood {

AudioClip::AudioClip(std::string logicalPath, std::string fsPath)
    : m_logicalPath(std::move(logicalPath)),
      m_fsPath(std::move(fsPath)) {
    // Inspeccion rapida con ma_decoder: nos da duracion y formato sin cargar
    // el PCM entero. Si falla (archivo roto, VFS mal, etc.) dejamos los
    // campos en 0 — la carga real fallara despues con un log mas util.
    ma_decoder decoder;
    ma_decoder_config cfg = ma_decoder_config_init_default();
    if (ma_decoder_init_file(m_fsPath.c_str(), &cfg, &decoder) != MA_SUCCESS) {
        return;
    }
    m_sampleRate = decoder.outputSampleRate;
    m_channels   = decoder.outputChannels;

    ma_uint64 totalFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) == MA_SUCCESS) {
        if (m_sampleRate > 0) {
            m_durationSeconds = static_cast<f32>(totalFrames) /
                                 static_cast<f32>(m_sampleRate);
        }
    }
    ma_decoder_uninit(&decoder);
}

} // namespace Mood
