#include "systems/AudioSystem.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioClip.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

namespace Mood {

void AudioSystem::update(Scene& scene, f32 /*dt*/) {
    scene.forEach<AudioSourceComponent>([&](Entity e, AudioSourceComponent& src) {
        // Resolver posicion del source: si hay Transform en la misma entidad,
        // usar su posicion. Sino, centro del mundo. (Un audio source sin
        // transform es valido solo si !is3D.)
        glm::vec3 pos(0.0f);
        if (e.hasComponent<TransformComponent>()) {
            pos = e.getComponent<TransformComponent>().position;
        }

        // playOnStart: disparar la reproduccion una vez.
        if (src.playOnStart && !src.started) {
            AudioClip* clip = m_assets.getAudio(src.clip);
            if (clip != nullptr) {
                src.handle = m_device.play(*clip, src.volume, src.loop,
                                            src.is3D, pos);
                src.started = true;
                if (src.handle != 0) {
                    Log::audio()->info("Reproduciendo '{}'{}",
                                        clip->logicalPath(),
                                        src.is3D ? " (3D)" : "");
                }
            }
        }

        // Posicional: sincronizar la posicion del sound cada frame si cambia.
        if (src.is3D && src.started && src.handle != 0) {
            m_device.setSoundPosition(src.handle, pos);
        }
    });
}

void AudioSystem::setListener(const glm::vec3& worldPos,
                               const glm::vec3& forward,
                               const glm::vec3& up) {
    m_device.setListener(worldPos, forward, up);
}

void AudioSystem::clear() {
    // Detiene todos los sonidos activos: las entidades que tenian
    // AudioSourceComponent estan por desaparecer (rebuildSceneFromMap hace
    // registry.clear()), y sus loops sin dueno quedarian colgados en el
    // device sin nadie a quien llamar stop().
    m_device.stopAll();
}

} // namespace Mood
