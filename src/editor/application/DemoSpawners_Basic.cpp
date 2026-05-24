// Spawners de features reales (Spawn menu / Welcome modal):
// PhysicsBox, Environment, PointLight, AudioSource, Trigger.
// post-v2.0.2 cleanup: borrados los demos historicos (Rotator, HudDemo,
// FireParticles, DialogDemo, NarrativeDemoMap) — sin entry point UI desde
// F2H57. Los stress tests viven en DemoSpawners_Stress.cpp.
//
// Nota historica: nombre del archivo se mantiene "DemoSpawners_*" por costo
// de rename (cross-CMakeLists). Renaming queda como polish opcional.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <string>
#include <utility>

namespace Mood {

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
