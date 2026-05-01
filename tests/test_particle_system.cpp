// Tests del ParticleSystem (Hito 29 Bloque 1). Cubren la simulacion CPU
// pura: tasa de emision, lifetime, gravedad, reciclaje de slots,
// pool full, RNG deterministico.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/world/GridMap.h"
#include "systems/ParticleSystem.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <filesystem>
#include <memory>

using namespace Mood;

namespace {

// Helper: cuenta particulas vivas a mano (sin confiar en aliveCount).
u32 countAlive(const ParticleEmitterComponent& em) {
    u32 c = 0;
    for (u8 a : em.alive) if (a != 0) ++c;
    return c;
}

class StubTextureParticles : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory stubFactoryParticles() {
    return [](const std::string&) { return std::make_unique<StubTextureParticles>(); };
}

std::filesystem::path tempPathParticles(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_test_particles_") + suffix);
}

} // namespace

TEST_CASE("ParticleSystem: tasa de emision spawnea cantidad esperada") {
    Scene scene;
    Entity e = scene.createEntity("Emisor");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 10.0f;
    em.maxParticles = 32;
    em.lifetimeMin = em.lifetimeMax = 5.0f;

    ParticleSystem sys;
    // 0.5s a 10/s = 5 particulas. Hacemos un solo update con dt=0.5
    // para evitar acumulacion de error en sub-frames.
    sys.update(scene, 0.5f);
    CHECK(countAlive(em) == 5u);
    CHECK(em.aliveCount == 5u);
}

TEST_CASE("ParticleSystem: lifetime expira la particula") {
    Scene scene;
    Entity e = scene.createEntity("ShortLived");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 4.0f;     // 1 particula en 0.25s
    em.maxParticles = 8;
    em.lifetimeMin = em.lifetimeMax = 1.0f;

    ParticleSystem sys;
    sys.update(scene, 0.25f);          // spawnea 1
    REQUIRE(countAlive(em) == 1u);
    em.emitting = false;               // pausamos para no contaminar
    sys.update(scene, 1.0f);           // expira la particula (age=1.0 == lifetime)
    CHECK(countAlive(em) == 0u);
    CHECK(em.aliveCount == 0u);
}

TEST_CASE("ParticleSystem: gravityFactor=1 aplica -9.81 m/s^2 en Y") {
    Scene scene;
    Entity e = scene.createEntity("Gravedad");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate     = 0.0f;            // no emitir auto, controlamos manualmente
    em.maxParticles = 4;
    em.lifetimeMin = em.lifetimeMax = 10.0f;
    em.gravityFactor = 1.0f;
    em.velocityMin = em.velocityMax = glm::vec3(0.0f, 5.0f, 0.0f);

    ParticleSystem sys;
    sys.update(scene, 0.0001f);  // ensurePoolSized + emit attempt (rate=0 => 0)

    // Inyectamos una particula manualmente al slot 0.
    em.alive[0]      = 1;
    em.positions[0]  = glm::vec3(0.0f);
    em.velocities[0] = glm::vec3(0.0f, 5.0f, 0.0f);
    em.ages[0]       = 0.0f;
    em.lifetimes[0]  = 10.0f;
    em.aliveCount    = 1;

    // dt=1: vy pasa de 5 a 5 + (-9.81*1) = -4.81. pos.y = 0 + (-4.81)*1 = -4.81.
    // (Nota: el sistema actualiza vel ANTES de pos, asi que pos usa la nueva vel.)
    sys.update(scene, 1.0f);
    CHECK(em.velocities[0].y == doctest::Approx(-4.81f).epsilon(0.01));
    CHECK(em.positions[0].y == doctest::Approx(-4.81f).epsilon(0.01));
}

TEST_CASE("ParticleSystem: pool full descarta sin crashear") {
    Scene scene;
    Entity e = scene.createEntity("PoolFull");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 100.0f;
    em.maxParticles = 4;          // intencionalmente chica
    em.lifetimeMin = em.lifetimeMax = 5.0f;

    ParticleSystem sys;
    sys.update(scene, 1.0f);      // intentaria spawnear 100 — solo entran 4
    CHECK(countAlive(em) == 4u);
    CHECK(em.aliveCount == 4u);
}

TEST_CASE("ParticleSystem: emitting=false pausa spawn pero sigue avanzando vivas") {
    Scene scene;
    Entity e = scene.createEntity("Pausable");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 10.0f;
    em.maxParticles = 16;
    em.lifetimeMin = em.lifetimeMax = 10.0f;

    ParticleSystem sys;
    sys.update(scene, 0.5f);     // spawnea 5
    REQUIRE(countAlive(em) == 5u);
    em.emitting = false;
    sys.update(scene, 1.0f);     // no spawn, las 5 siguen
    CHECK(countAlive(em) == 5u);
}

TEST_CASE("ParticleSystem: spawn position = TransformComponent.position") {
    Scene scene;
    Entity e = scene.createEntity("Localizado");
    e.getComponent<TransformComponent>().position = glm::vec3(5.0f, 6.0f, 7.0f);
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate     = 1.0f;
    em.maxParticles = 4;
    em.lifetimeMin = em.lifetimeMax = 10.0f;
    em.velocityMin = em.velocityMax = glm::vec3(0.0f); // sin velocidad

    ParticleSystem sys;
    sys.update(scene, 1.0f);    // spawnea 1
    REQUIRE(em.aliveCount >= 1u);
    // Buscamos la primera viva.
    for (u32 i = 0; i < em.alive.size(); ++i) {
        if (em.alive[i] == 0) continue;
        // pos = spawnPos + vel*dt = (5,6,7) + 0 = (5,6,7). Pero el update
        // YA aplico vel*dt (que es 0 aqui) y gravedad (0 aqui).
        CHECK(em.positions[i].x == doctest::Approx(5.0f));
        CHECK(em.positions[i].y == doctest::Approx(6.0f));
        CHECK(em.positions[i].z == doctest::Approx(7.0f));
        break;
    }
}

TEST_CASE("ParticleSystem: pool size matchea maxParticles tras primer update") {
    Scene scene;
    Entity e = scene.createEntity("Resize");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 0.0f;
    em.maxParticles = 17;
    em.lifetimeMin = em.lifetimeMax = 1.0f;

    CHECK(em.positions.empty());
    ParticleSystem sys;
    sys.update(scene, 0.0001f);
    CHECK(em.positions.size() == 17u);
    CHECK(em.velocities.size() == 17u);
    CHECK(em.alive.size() == 17u);
}

TEST_CASE("ParticleSystem: dt<=0 es no-op") {
    Scene scene;
    Entity e = scene.createEntity("NoOp");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 100.0f;
    em.maxParticles = 16;

    ParticleSystem sys;
    sys.update(scene, 0.0f);
    sys.update(scene, -1.0f);
    CHECK(em.aliveCount == 0u);
    // Tampoco se inicializa la pool — la lazy-alloc espera dt>0.
    CHECK(em.positions.empty());
}

TEST_CASE("ParticleSystem: emisor recicla slots de particulas muertas") {
    Scene scene;
    Entity e = scene.createEntity("Reciclador");
    auto& em = e.addComponent<ParticleEmitterComponent>();
    em.emitRate    = 5.0f;
    em.maxParticles = 4;        // se llena rapido
    em.lifetimeMin = em.lifetimeMax = 0.5f;

    ParticleSystem sys;
    // 1s @ 5/s = 5 spawn requests. Pool=4 -> primera tanda llena. Luego
    // dt=1.0s mata las 4 (lifetime=0.5s). El frame siguiente vuelve a
    // emitir hasta llenar.
    sys.update(scene, 0.8f);
    const u32 cnt1 = countAlive(em);
    CHECK(cnt1 == 4u);
    sys.update(scene, 1.0f);   // expira las primeras + spawnea hasta lim
    CHECK(countAlive(em) == 4u);
}

TEST_CASE("SceneSerializer: round-trip de ParticleEmitterComponent en .moodmap") {
    AssetManager assets("assets", stubFactoryParticles());

    GridMap map(2u, 2u, 1.0f);
    Scene scene;
    Entity e = scene.createEntity("FuegoTest");
    e.getComponent<TransformComponent>().position = glm::vec3(1, 2, 3);

    ParticleEmitterComponent em{};
    em.emitRate     = 42.0f;
    em.lifetimeMin  = 0.5f;
    em.lifetimeMax  = 2.5f;
    em.velocityMin  = glm::vec3(-0.7f, 1.2f, -0.7f);
    em.velocityMax  = glm::vec3( 0.7f, 3.5f,  0.7f);
    em.sizeStart    = 0.42f;
    em.sizeEnd      = 0.07f;
    em.colorStart   = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);
    em.colorEnd     = glm::vec4(0.2f, 0.0f, 0.0f, 0.0f);
    em.gravityFactor = -0.3f;
    em.maxParticles = 128;
    em.emitting     = true;
    em.additive     = true;
    em.localSpace   = true;  // Hito 31 F
    em.texture = assets.loadTexture("textures/particle_fire.png");
    e.addComponent<ParticleEmitterComponent>(em);

    const auto path = tempPathParticles("emitter_roundtrip.moodmap");
    SceneSerializer::save(map, "particles", &scene, assets, path);
    REQUIRE(std::filesystem::exists(path));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);

    const SavedEntity& se = loaded->entities[0];
    REQUIRE(se.particleEmitter.has_value());
    const auto& pe = *se.particleEmitter;
    CHECK(pe.emitRate     == doctest::Approx(42.0f));
    CHECK(pe.lifetimeMin  == doctest::Approx(0.5f));
    CHECK(pe.lifetimeMax  == doctest::Approx(2.5f));
    CHECK(pe.velocityMin.y == doctest::Approx(1.2f));
    CHECK(pe.velocityMax.y == doctest::Approx(3.5f));
    CHECK(pe.sizeStart    == doctest::Approx(0.42f));
    CHECK(pe.sizeEnd      == doctest::Approx(0.07f));
    CHECK(pe.colorStart.r == doctest::Approx(1.0f));
    CHECK(pe.colorEnd.a   == doctest::Approx(0.0f));
    CHECK(pe.gravityFactor == doctest::Approx(-0.3f));
    CHECK(pe.maxParticles == 128u);
    CHECK(pe.emitting     == true);
    CHECK(pe.additive     == true);
    CHECK(pe.localSpace   == true);  // Hito 31 F
    CHECK(pe.texturePath  == "textures/particle_fire.png");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: localSpace default false se preserva al round-trip (Hito 31 F)") {
    // Garantia retro-compat: archivos viejos sin "local_space" leen
    // localSpace=false (el default).
    AssetManager assets("assets", stubFactoryParticles());

    GridMap map(1u, 1u, 1.0f);
    Scene scene;
    Entity e = scene.createEntity("DefaultLocalSpace");
    e.addComponent<ParticleEmitterComponent>(); // defaults — localSpace = false

    const auto path = tempPathParticles("default_localspace.moodmap");
    SceneSerializer::save(map, "default_localspace", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);
    REQUIRE(loaded->entities[0].particleEmitter.has_value());
    CHECK(loaded->entities[0].particleEmitter->localSpace == false);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: emitter sin textura persiste con texturePath vacio") {
    AssetManager assets("assets", stubFactoryParticles());

    GridMap map(1u, 1u, 1.0f);
    Scene scene;
    Entity e = scene.createEntity("SinTex");
    e.addComponent<ParticleEmitterComponent>(); // defaults — texture = 0

    const auto path = tempPathParticles("no_texture.moodmap");
    SceneSerializer::save(map, "no_texture", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);
    REQUIRE(loaded->entities[0].particleEmitter.has_value());
    CHECK(loaded->entities[0].particleEmitter->texturePath.empty());

    std::filesystem::remove(path);
}
