#include "engine/audio/AudioDevice.h"

#include "core/Log.h"
#include "engine/audio/AudioClip.h"

#include <miniaudio.h>

#include <unordered_map>

namespace Mood {

struct AudioDevice::Impl {
    ma_engine engine{};
    bool valid = false;
    // sound handle -> ma_sound (una reproduccion activa). unique_ptr para
    // que la direccion del ma_sound no se mueva si el mapa rehashea.
    std::unordered_map<SoundHandle, std::unique_ptr<ma_sound>> sounds;
    SoundHandle nextHandle = 1;
};

AudioDevice::AudioDevice()
    : m_impl(std::make_unique<Impl>()) {
    const ma_result r = ma_engine_init(nullptr, &m_impl->engine);
    if (r != MA_SUCCESS) {
        Log::audio()->warn("AudioDevice: ma_engine_init fallo (codigo {}): motor sigue sin audio.",
                            static_cast<int>(r));
        m_impl->valid = false;
        return;
    }
    m_impl->valid = true;
    Log::audio()->info("AudioDevice inicializado (sample_rate={}, channels={})",
                        sampleRate(), channels());
}

AudioDevice::~AudioDevice() {
    if (!m_impl) return;
    // Matamos los sonidos activos antes del engine_uninit. Sino miniaudio
    // deja warnings por recursos huerfanos en shutdown.
    for (auto& [h, sound] : m_impl->sounds) {
        if (sound) ma_sound_uninit(sound.get());
    }
    m_impl->sounds.clear();
    if (m_impl->valid) {
        ma_engine_uninit(&m_impl->engine);
        m_impl->valid = false;
    }
}

bool AudioDevice::isValid() const { return m_impl && m_impl->valid; }

u32 AudioDevice::sampleRate() const {
    if (!isValid()) return 0;
    return ma_engine_get_sample_rate(&m_impl->engine);
}

u32 AudioDevice::channels() const {
    if (!isValid()) return 0;
    return ma_engine_get_channels(&m_impl->engine);
}

usize AudioDevice::activeSoundCount() const {
    if (!m_impl) return 0;
    return m_impl->sounds.size();
}

SoundHandle AudioDevice::play(const AudioClip& clip, f32 volume, bool loop,
                               bool is3D, const glm::vec3& worldPos) {
    if (!isValid()) return 0;
    if (clip.path().empty()) return 0;

    // Sweep: reciclar slots de sounds que terminaron solos (one-shots sin
    // loop). Evita crecimiento monotonico del mapa.
    for (auto it = m_impl->sounds.begin(); it != m_impl->sounds.end();) {
        if (it->second && ma_sound_at_end(it->second.get()) == MA_TRUE) {
            ma_sound_uninit(it->second.get());
            it = m_impl->sounds.erase(it);
        } else {
            ++it;
        }
    }

    auto sound = std::make_unique<ma_sound>();
    const ma_uint32 flags = MA_SOUND_FLAG_DECODE;
    const ma_result r = ma_sound_init_from_file(&m_impl->engine,
                                                 clip.path().c_str(),
                                                 flags,
                                                 /*group*/ nullptr,
                                                 /*fence*/ nullptr,
                                                 sound.get());
    if (r != MA_SUCCESS) {
        Log::audio()->warn("AudioDevice::play fallo cargando '{}' (codigo {})",
                            clip.path(), static_cast<int>(r));
        return 0;
    }

    ma_sound_set_volume(sound.get(), volume);
    ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);

    // is3D controla la "spatialization" del sound. Si es false, el sonido se
    // mezcla al listener a full volume independiente de su posicion.
    ma_sound_set_spatialization_enabled(sound.get(), is3D ? MA_TRUE : MA_FALSE);
    if (is3D) {
        ma_sound_set_position(sound.get(), worldPos.x, worldPos.y, worldPos.z);
    }

    if (ma_sound_start(sound.get()) != MA_SUCCESS) {
        ma_sound_uninit(sound.get());
        Log::audio()->warn("AudioDevice::play: ma_sound_start fallo para '{}'", clip.path());
        return 0;
    }

    const SoundHandle h = m_impl->nextHandle++;
    if (m_impl->nextHandle == 0) m_impl->nextHandle = 1; // wrap, saltando 0
    m_impl->sounds.emplace(h, std::move(sound));
    return h;
}

void AudioDevice::stop(SoundHandle h) {
    if (!isValid() || h == 0) return;
    auto it = m_impl->sounds.find(h);
    if (it == m_impl->sounds.end()) return;
    if (it->second) {
        ma_sound_stop(it->second.get());
        ma_sound_uninit(it->second.get());
    }
    m_impl->sounds.erase(it);
}

void AudioDevice::stopAll() {
    if (!isValid()) return;
    for (auto& [h, sound] : m_impl->sounds) {
        if (sound) {
            ma_sound_stop(sound.get());
            ma_sound_uninit(sound.get());
        }
    }
    m_impl->sounds.clear();
}

void AudioDevice::setSoundPosition(SoundHandle h, const glm::vec3& worldPos) {
    if (!isValid() || h == 0) return;
    auto it = m_impl->sounds.find(h);
    if (it == m_impl->sounds.end() || !it->second) return;
    ma_sound_set_position(it->second.get(), worldPos.x, worldPos.y, worldPos.z);
}

void AudioDevice::setListener(const glm::vec3& worldPos,
                               const glm::vec3& forward,
                               const glm::vec3& up) {
    if (!isValid()) return;
    // miniaudio soporta multiples listeners; usamos solo el 0.
    ma_engine_listener_set_position(&m_impl->engine, 0, worldPos.x, worldPos.y, worldPos.z);
    ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
}

} // namespace Mood
