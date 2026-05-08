// F2H24: demos chicos de "Ayuda > Agregar X":
// Rotator, HUD demo, PhysicsBox, Environment, PointLight, AudioSource,
// FireParticles, Trigger. Demos pesados / showcase viven en
// DemoSpawners_Stress.cpp.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <cstdio>
#include <string>

namespace Mood {

void EditorApplication::processSpawnRotatorRequest() {
    if (!(m_ui.consumeSpawnRotatorRequest() && m_scene)) return;
    Entity r = m_scene->createEntity("Rotador");
    auto& t = r.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 4.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    // Usa el cubo fallback del AssetManager (slot 0) como mesh del rotador.
    // Material instance unico (createMaterialFromTexture, no loadMaterialFromTexture):
    // si reusaramos el material cacheado, editar el tint de UN rotador
    // pintaria a TODOS los rotadores spawneados.
    const MaterialAssetId rotMat =
        m_assetManager->createMaterialFromTexture(m_wallTextureId);
    r.addComponent<MeshRendererComponent>(m_assetManager->missingMeshId(), rotMat);
    r.addComponent<ScriptComponent>(std::string{"assets/scripts/rotator.lua"});
    Log::editor()->info("Spawned rotador demo en (0, 4, 0)");
    pushCreatedEntities({r}, "Spawn rotador demo");
}

void EditorApplication::processSpawnHudDemoRequest() {
    if (!(m_ui.consumeSpawnHudDemoRequest() && m_scene)) return;
    // No tiene mesh visible — la entidad existe solo para hostear el
    // ScriptComponent. En Editor Mode aparece como icono Audio/Light? No
    // tiene componente especial, asi que no aparece en el overlay; queda
    // solo en el Hierarchy. En Play Mode el script ejercita la tabla
    // `hud` y los efectos se ven en el HUD/menu de pausa del juego.
    Entity e = m_scene->createEntity("HudDemo");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 0.0f, 0.0f);
    e.addComponent<ScriptComponent>(std::string{"assets/scripts/hud_demo.lua"});
    Log::editor()->info(
        "Spawned HudDemo. Entrar en Play Mode para ver el HUD reaccionar "
        "(HP drena 1/s; HP=0 -> pausa forzada).");
    pushCreatedEntities({e}, "Spawn HUD demo");
}

void EditorApplication::processSpawnPhysicsBoxRequest() {
    if (!(m_ui.consumeSpawnPhysicsBoxRequest() && m_scene && m_assetManager)) return;
    // Hito 41 fix: tag unico para que SaveLoad pueda matchear cada caja
    // por separado. Con tags duplicados los snapshots se sobrescribian
    // en el byTag map del load → cajas quedaban apiladas y Jolt las
    // disparaba como si "desaparecieran".
    int suffix = 1;
    std::string tagName;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "CajaFisica_%02d", suffix);
        tagName = buf;
        bool collision = false;
        m_scene->forEach<TagComponent>(
            [&](Entity, TagComponent& tag) {
                if (tag.name == tagName) collision = true;
            });
        if (!collision) break;
        ++suffix;
    }
    Entity box = m_scene->createEntity(tagName);
    auto& t = box.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 6.0f, 0.0f);
    t.scale    = glm::vec3(1.0f);
    // Material instance unico por caja (ver nota en processSpawnRotatorRequest).
    const MaterialAssetId boxMat =
        m_assetManager->createMaterialFromTexture(m_wallTextureId);
    box.addComponent<MeshRendererComponent>(
        m_assetManager->missingMeshId(), boxMat);
    box.addComponent<RigidBodyComponent>(
        RigidBodyComponent::Type::Dynamic,
        RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f, 0.5f, 0.5f),
        5.0f);
    Log::physics()->info("Spawned caja fisica en (0, 6, 0) con mass=5kg");
    pushCreatedEntities({box}, "Spawn caja fisica");
}

void EditorApplication::processSpawnEnvironmentRequest() {
    if (!(m_ui.consumeSpawnEnvironmentRequest() && m_scene)) return;
    // Si ya hay una, avisar y seleccionarla en lugar de duplicar.
    Entity existing{};
    m_scene->forEach<EnvironmentComponent>(
        [&](Entity e, EnvironmentComponent&) {
            if (!static_cast<bool>(existing)) existing = e;
        });
    if (static_cast<bool>(existing)) {
        Log::editor()->warn("Ya existe un Environment; seleccionando el existente.");
        m_ui.setSelectedEntity(existing);
    } else {
        Entity env = m_scene->createEntity("Environment");
        env.addComponent<EnvironmentComponent>();
        m_ui.setSelectedEntity(env);
        Log::editor()->info("Spawned entidad Environment con defaults");
        pushCreatedEntities({env}, "Spawn Environment");
    }
}

void EditorApplication::processSpawnPointLightRequest() {
    if (!(m_ui.consumeSpawnPointLightRequest() && m_scene)) return;
    Entity light = m_scene->createEntity("Luz demo");
    auto& t = light.getComponent<TransformComponent>();
    t.position = glm::vec3(0.0f, 4.0f, 0.0f);
    LightComponent lc{};
    lc.type      = LightComponent::Type::Point;
    lc.color     = glm::vec3(1.0f, 0.95f, 0.85f); // tibia, no neon
    lc.intensity = 1.5f;
    lc.radius    = 12.0f;
    lc.enabled   = true;
    light.addComponent<LightComponent>(lc);
    Log::editor()->info("Spawned luz puntual en (0, 4, 0) con radius=12");
    pushCreatedEntities({light}, "Spawn luz puntual");
}

void EditorApplication::processSpawnAudioSourceRequest() {
    if (!(m_ui.consumeSpawnAudioSourceRequest() && m_scene && m_assetManager)) return;
    const AudioAssetId beepId = m_assetManager->loadAudio("audio/beep.wav");
    Entity src = m_scene->createEntity("AudioSource demo");
    auto& t = src.getComponent<TransformComponent>();
    t.position = glm::vec3(-10.0f, 1.5f, -10.0f);
    // Sin MeshRenderer: es una entidad invisible (marcador audio).
    AudioSourceComponent asrc{beepId};
    asrc.loop = true;
    asrc.is3D = true;
    asrc.playOnStart = true;
    asrc.volume = 0.8f;
    src.addComponent<AudioSourceComponent>(asrc);
    Log::editor()->info("Spawned audio source demo en (-10, 1.5, -10)");
    pushCreatedEntities({src}, "Spawn audio source");
}

void EditorApplication::processSpawnFireParticlesRequest() {
    if (!(m_ui.consumeSpawnFireParticlesRequest() && m_scene
          && m_assetManager)) return;

    Entity e = m_scene->createEntity("Fuego");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = glm::vec3(0.0f, 0.5f, 0.0f);

    ParticleEmitterComponent em{};
    em.emitRate     = 60.0f;
    em.lifetimeMin  = 1.0f;
    em.lifetimeMax  = 1.5f;
    em.velocityMin  = glm::vec3(-0.4f, 1.2f, -0.4f);
    em.velocityMax  = glm::vec3( 0.4f, 2.0f,  0.4f);
    em.sizeStart    = 0.30f;
    em.sizeEnd      = 0.05f;
    em.colorStart   = glm::vec4(1.0f, 0.75f, 0.20f, 1.0f);
    em.colorEnd     = glm::vec4(1.0f, 0.10f, 0.0f, 0.0f);
    em.gravityFactor = -0.05f;     // negativo = sube ligeramente
    em.maxParticles = 256;
    em.emitting     = true;
    em.additive     = true;
    em.texture = m_assetManager->loadTexture("textures/particle_fire.png");

    e.addComponent<ParticleEmitterComponent>(em);

    Log::editor()->info(
        "Spawned demo de particulas (fuego) en (0, 0.5, 0): rate={}, "
        "lifetime={:.1f}-{:.1f}s, additive blend",
        em.emitRate, em.lifetimeMin, em.lifetimeMax);
    pushCreatedEntities({e}, "Spawn particulas demo");
}

void EditorApplication::processSpawnTriggerRequest() {
    if (!(m_ui.consumeSpawnTriggerRequest() && m_scene)) return;

    Entity e = m_scene->createEntity("Trigger demo");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = glm::vec3(0.0f, 1.0f, 0.0f);

    TriggerComponent tc{};
    tc.halfExtents = glm::vec3(1.0f, 1.0f, 1.0f);  // 2x2x2m
    e.addComponent<TriggerComponent>(tc);

    // Adjuntamos el script demo (loguea enter/exit). El usuario puede
    // sobreescribir el path desde el Inspector si quiere otro callback.
    e.addComponent<ScriptComponent>(std::string{"assets/scripts/trigger_demo.lua"});

    Log::editor()->info(
        "Spawned trigger demo en (0, 1, 0) con halfExtents=(1,1,1) + "
        "script trigger_demo.lua. Solo dispatcha en Play Mode.");
    pushCreatedEntities({e}, "Spawn trigger demo");
}

} // namespace Mood
