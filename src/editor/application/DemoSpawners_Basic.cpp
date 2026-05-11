// F2H24: demos chicos de "Ayuda > Agregar X":
// Rotator, HUD demo, PhysicsBox, Environment, PointLight, AudioSource,
// FireParticles, Trigger. Demos pesados / showcase viven en
// DemoSpawners_Stress.cpp.

#include "editor/application/EditorApplication.h"
#include "editor/application/DemoSpawners_Internal.h"     // F2H50

#include "core/Log.h"
#include "editor/panels/narrative/DialogBrowserPanel.h"   // F2H47
#include "editor/panels/narrative/DialogEditorPanel.h"    // F2H47
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/dialog/DialogAsset.h"                    // F2H47
#include "engine/render/resources/MeshAsset.h"            // F2H50
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneSerializer.h"   // F2H50
#include "engine/world/grid/GridMap.h"                    // F2H50

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

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

// F2H50: extraido del handler original para que `processGenerateNarrative
// DemoMapRequest` tambien pueda asegurar que el .mooddialog existe sin
// disparar los side effects de UI (abrir el DialogEditor + refresh del
// browser). Devuelve true si el archivo ya existia o se genero ok.
bool detail::ensureDemoIntroDialogExists(const std::filesystem::path& demoPath) {
    namespace fs = std::filesystem;
    if (fs::exists(demoPath)) return true;

    Dialog::Asset a;
    a.metadata().name = "demo_intro";
    a.metadata().default_portrait = "characters/demo_npc.png";
    a.metadata().default_audio_bus = "voice";

    // Nodo 1: saludo inicial con 2 choices.
    const NodeGraph::NodeId greet = a.addLine(glm::vec2(40.0f, 200.0f));
    {
        Dialog::Line line;
        line.text_literal = "Hola viajero. Es peligroso ir solo... pero veo que tienes "
                            "agallas. Que te trae a estas tierras?";
        line.choices = {
            Dialog::Choice{"", "Vengo a derrotar al dragon.", "", ""},
            Dialog::Choice{"", "Solo estoy de paso.",          "", ""},
        };
        a.writeLine(greet, line);
    }

    // Nodo 2: respuesta heroica.
    const NodeGraph::NodeId hero = a.addLine(glm::vec2(360.0f, 100.0f));
    {
        Dialog::Line line;
        line.text_literal = "Valiente! Lleva esta espada — el dragon ha quemado mi aldea.";
        a.writeLine(hero, line);
    }

    // Nodo 3: respuesta despreocupada.
    const NodeGraph::NodeId casual = a.addLine(glm::vec2(360.0f, 300.0f));
    {
        Dialog::Line line;
        line.text_literal = "Como quieras. Que los caminos te traten bien.";
        a.writeLine(casual, line);
    }

    // Linkear: greet.choice_0 -> hero.in; greet.choice_1 -> casual.in.
    const NodeGraph::Node* nGreet  = a.graph().findNode(greet);
    const NodeGraph::Node* nHero   = a.graph().findNode(hero);
    const NodeGraph::Node* nCasual = a.graph().findNode(casual);
    if (nGreet && nHero && nCasual &&
        nGreet->outputs.size() >= 2 &&
        !nHero->inputs.empty() &&
        !nCasual->inputs.empty()) {
        a.graph().addLink(nGreet->outputs[0].id, nHero->inputs[0].id);
        a.graph().addLink(nGreet->outputs[1].id, nCasual->inputs[0].id);
    }

    if (!a.saveToFile(demoPath)) {
        Log::editor()->error("[DialogDemo] no pude guardar '{}'",
                              demoPath.generic_string());
        return false;
    }
    Log::editor()->info("[DialogDemo] generado en '{}'",
                          demoPath.generic_string());
    return true;
}

void EditorApplication::processSpawnDialogDemoRequest() {
    if (!m_ui.consumeSpawnDialogDemoRequest()) return;

    namespace fs = std::filesystem;
    const fs::path demoPath = fs::current_path() / "assets" / "dialogs" / "demo_intro.mooddialog";

    if (!detail::ensureDemoIntroDialogExists(demoPath)) return;

    // Refrescar el browser + abrir en el editor.
    m_ui.dialogBrowser().refresh();
    m_ui.dialogEditor().openFromFile(demoPath);
}

// F2H50 Bloque A: genera un .moodmap demo de narrativa con 1 NPC armado
// con MeshRenderer (Y Bot) + Animator (auto-attach de anim_idle/talk/look_around)
// + DialogComponent (-> dialogs/demo_intro.mooddialog) + TriggerComponent (E
// para conversar). Asegura que el .mooddialog tambien exista. Despues abre
// el mapa en el editor.
//
// Por que generador programatico: el .moodmap resultante es DATA puro
// (editable como cualquier mapa, sin codigo). El generador es scaffold
// — un dev cualquiera puede usar el mismo flujo con sus propios assets:
// drag-drop su FBX, add DialogComponent + Trigger, save. Lo que hace este
// handler es automatizar ese setup para el demo.
//
// Scope minimal: solo el NPC. La grid de tiles default sirve de piso
// implicito (built-in en el editor). El dev puede ampliar la escena
// editando el .moodmap (agregar walls CSG, lighting, mas NPCs, etc.).
void EditorApplication::processGenerateNarrativeDemoMapRequest() {
    if (!m_ui.consumeGenerateNarrativeDemoMapRequest()) return;
    if (m_assetManager == nullptr) return;

    namespace fs = std::filesystem;
    const fs::path mapsDir   = fs::current_path() / "assets" / "maps";
    const fs::path mapPath   = mapsDir / "narrative_demo.moodmap";
    const fs::path dialogPath = fs::current_path() / "assets" / "dialogs" /
                                  "demo_intro.mooddialog";

    // Dependencia: el NPC apunta al demo_intro.mooddialog. Si no existe,
    // generarlo (mismo helper que el handler "Cargar dialogo demo").
    if (!detail::ensureDemoIntroDialogExists(dialogPath)) {
        Log::editor()->error(
            "[NarrativeDemo] no pude asegurar el .mooddialog en '{}'",
            dialogPath.generic_string());
        return;
    }

    // Si el .moodmap no existe todavia, lo armamos programaticamente
    // y lo guardamos a disco. Si existe (corrida previa), reabrimos
    // directamente — preserva edits que el dev haya hecho post-gen.
    if (!fs::exists(mapPath)) {
        std::error_code ec;
        fs::create_directories(mapsDir, ec);
        if (ec) {
            Log::editor()->error(
                "[NarrativeDemo] no pude crear '{}': {}",
                mapsDir.generic_string(), ec.message());
            return;
        }

        // Scene + GridMap locales (no tocan el estado del editor — el
        // .moodmap se serializa con paths logicos, asi al cargar se
        // re-resuelven contra el AssetManager destino). GridMap 10x10
        // tiles de 3m = 30x30m de piso implicito (default del motor).
        GridMap localMap(10, 10);
        Scene localScene;

        // NPC: Y Bot a 3m del spawn (origin), mirando hacia el player.
        const std::string npcMeshLogical = "characters/npc/npc.fbx";
        const MeshAssetId npcMeshId = m_assetManager->loadMesh(npcMeshLogical);
        const MeshAsset* npcAsset = m_assetManager->getMesh(npcMeshId);

        Entity npc = localScene.createEntity("NPC");
        auto& npcT = npc.getComponent<TransformComponent>();
        npcT.position = glm::vec3(3.0f, 0.0f, 0.0f);
        // Mirar al player spawn (origin) -> 180° en Y. Y Bot import lo
        // deja mirando -Z, queremos +X mirando hacia -X (el player).
        npcT.rotationEuler = (npcAsset != nullptr)
            ? (npcAsset->importRotationEuler + glm::vec3(0.0f, -90.0f, 0.0f))
            : glm::vec3(0.0f, -90.0f, 0.0f);

        // MeshRenderer con materiales colorizados (F2H49.1 maneja el
        // diffuse color de Mixamo cuando no hay texture maps).
        auto npcMats = m_assetManager->createMaterialsForMesh(npcMeshId);
        npc.addComponent<MeshRendererComponent>(npcMeshId, std::move(npcMats));

        // Animator + Skeleton + auto-attach siblings (F2H50 Bloque B).
        if (npcAsset != nullptr && npcAsset->hasSkeleton()) {
            AnimatorComponent anim{};
            anim.playing = true;
            anim.loop    = true;
            detail::attachSiblingAnimClips(*m_assetManager, npcMeshLogical, anim);
            npc.addComponent<AnimatorComponent>(anim);
            npc.addComponent<SkeletonComponent>(SkeletonComponent{});
        }

        // DialogComponent: linkea al .mooddialog generado.
        DialogComponent dc;
        dc.dialogPath = "dialogs/demo_intro.mooddialog";
        dc.autoStartOnInteract = true;
        npc.addComponent<DialogComponent>(dc);

        // TriggerComponent: 3x2x3m alrededor del NPC. Cuando el player
        // entra + presiona E, el DialogInteractSystem arranca el dialog.
        TriggerComponent tc;
        tc.halfExtents = glm::vec3(1.5f, 1.0f, 1.5f);
        npc.addComponent<TriggerComponent>(tc);

        // Persistir.
        try {
            SceneSerializer::save(localMap, mapPath.stem().generic_string(),
                                    &localScene, *m_assetManager, mapPath);
            Log::editor()->info("[NarrativeDemo] mapa generado: '{}'",
                                  mapPath.generic_string());
        } catch (const std::exception& e) {
            Log::editor()->error("[NarrativeDemo] save fallo: {}", e.what());
            return;
        }
    }

    // Abrir en el editor (existing open-map flow maneja el dirty prompt
    // si el user tenia cambios sin guardar en el proyecto actual).
    m_ui.requestOpenMap(mapPath);
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
