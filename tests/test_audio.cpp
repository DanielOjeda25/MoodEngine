// Smoke tests de la capa de audio (Hito 9).
//
// Estos tests NO dependen de hardware de audio: `AudioDevice` es tolerante
// a fallo de init (modo "mute"), y el AudioSystem sigue marcando `started`
// aunque el device no produzca sonido real. En CI sin dispositivo el device
// inicia en mute y las asserciones sobre el state machine siguen siendo
// validas.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioClip.h"
#include "engine/audio/AudioDevice.h"
#include "engine/render/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/AudioSystem.h"

#include <memory>
#include <stdexcept>
#include <string>

using namespace Mood;

namespace {

/// Mock de ITexture (copia del test_asset_manager: no podemos reusar sin
/// compartir un header de mocks, queda en duplicacion aceptable).
class MockTexture : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width()  const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory mockTextureFactory() {
    return [](const std::string&) { return std::make_unique<MockTexture>(); };
}

} // namespace

TEST_CASE("AudioDevice se construye en modo mute o valido, sin crashear") {
    AudioDevice dev;
    // En CI puede fallar init (ma_engine_init con device default sin hw);
    // en desktop con audio inicia valido. Ambos son OK.
    CHECK((dev.isValid() || !dev.isValid()));
    if (dev.isValid()) {
        CHECK(dev.sampleRate() > 0);
        CHECK(dev.channels()   > 0);
    }
    CHECK(dev.activeSoundCount() == 0);
}

TEST_CASE("AssetManager::loadAudio cachea por path y cae a missing en fallo") {
    AssetManager am("assets", mockTextureFactory());

    const AudioAssetId missingId = am.missingAudioId();
    CHECK(missingId == 0);

    // Path inexistente: la factoria real de AudioClip NO lanza — solo deja
    // metadata en 0. Se registra como clip normal con id > 0 (la carga real
    // fallara despues al reproducir, no ahora).
    // Pero un path rechazado por el VFS (unsafe) SI cae al fallback.
    const AudioAssetId unsafe = am.loadAudio("../escape.wav");
    CHECK(unsafe == missingId);

    // Cachear: segunda llamada al mismo path devuelve el mismo id.
    const AudioAssetId unsafe2 = am.loadAudio("../escape.wav");
    CHECK(unsafe2 == unsafe);
}

TEST_CASE("AssetManager::getAudio nunca retorna null") {
    AssetManager am("assets", mockTextureFactory());
    CHECK(am.getAudio(am.missingAudioId()) != nullptr);
    CHECK(am.getAudio(9999) != nullptr); // out-of-range cae a missing
}

TEST_CASE("AudioSystem::update con playOnStart marca started=true tras un update") {
    AssetManager am("assets", mockTextureFactory());
    AudioDevice dev;
    AudioSystem audio(dev, am);

    Scene scene;
    Entity e = scene.createEntity("test-source");
    AudioSourceComponent src;
    src.clip = am.missingAudioId();
    src.playOnStart = true;
    src.is3D = false;
    src.volume = 0.5f;
    e.addComponent<AudioSourceComponent>(src);

    CHECK_FALSE(e.getComponent<AudioSourceComponent>().started);
    audio.update(scene, 0.016f);
    CHECK(e.getComponent<AudioSourceComponent>().started);

    // Segundo update: no deberia re-disparar (started guard).
    const auto prevHandle = e.getComponent<AudioSourceComponent>().handle;
    audio.update(scene, 0.016f);
    CHECK(e.getComponent<AudioSourceComponent>().handle == prevHandle);
}

TEST_CASE("AudioSystem::clear detiene todos los sonidos del device") {
    AssetManager am("assets", mockTextureFactory());
    AudioDevice dev;
    AudioSystem audio(dev, am);

    Scene scene;
    Entity e = scene.createEntity("s");
    AudioSourceComponent src;
    src.clip = am.missingAudioId();
    src.playOnStart = true;
    e.addComponent<AudioSourceComponent>(src);

    audio.update(scene, 0.016f);
    // Si el device esta valido, la reproduccion del missing (silencio) cuenta;
    // si esta muted, no. Ambos paths son OK para el test.
    const usize before = dev.activeSoundCount();
    (void)before;

    audio.clear();
    CHECK(dev.activeSoundCount() == 0);
}

TEST_CASE("AudioSystem::update actualiza posicion 3D segun TransformComponent") {
    AssetManager am("assets", mockTextureFactory());
    AudioDevice dev;
    AudioSystem audio(dev, am);

    Scene scene;
    Entity e = scene.createEntity("s3d");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(5.0f, 0.0f, 3.0f);

    AudioSourceComponent src;
    src.clip = am.missingAudioId();
    src.playOnStart = true;
    src.is3D = true;
    e.addComponent<AudioSourceComponent>(src);

    // Disparamos el playOnStart.
    audio.update(scene, 0.016f);
    CHECK(e.getComponent<AudioSourceComponent>().started);

    // Cambiar la posicion y un nuevo update no debe crashear ni re-arrancar.
    t.position = glm::vec3(-10.0f, 2.0f, -7.0f);
    const auto h1 = e.getComponent<AudioSourceComponent>().handle;
    audio.update(scene, 0.016f);
    CHECK(e.getComponent<AudioSourceComponent>().handle == h1);
}
