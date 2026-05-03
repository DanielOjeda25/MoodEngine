#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/DeleteEntityCommand.h"
#include "engine/assets/AssetManager.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/GridMap.h"
#include "systems/AudioSystem.h"
#include "systems/ScriptSystem.h"

#include <glm/vec3.hpp>
#include <glm/common.hpp>          // glm::mix
#include <glm/gtc/quaternion.hpp>   // Hito 41 fix-up: euler → quat

#include <cmath>            // std::sin
#include <string>

namespace Mood {

void EditorApplication::buildInitialTestMap() {
    // Sala abierta 16x16 con piso plano + UNA columna brick central
    // como obstaculo de prueba para pathfinding y selecciones. Sin
    // muros perimetrales — esos quedaban como una hilera de cubos
    // dentados que se veian feo. El skybox cubre el horizonte.
    // El piso es una entidad agregada en rebuildSceneFromMap.
    m_map = GridMap(16u, 16u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");
    m_wallTextureId = grid;

    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("Mapa de prueba cargado: arena_16x16 ({} tiles solidos)",
                       m_map.solidCount());
}

void EditorApplication::rebuildSceneFromMap() {
    // Vision actual: Scene es derivada del GridMap. Cada tile solido =
    // 1 entidad con Transform + MeshRenderer. Limpiamos en-place el
    // registry (en vez de recrear el Scene completo) para no invalidar
    // los punteros que tienen los paneles Hierarchy/Inspector.
    if (!m_scene) m_scene = std::make_unique<Scene>();
    // Hito 12: antes del clear, destruir los bodies de Jolt asociados a
    // RigidBodyComponent — tras registry.clear() perdemos los componentes
    // y los bodyIds quedarian huerfanos en PhysicsWorld.
    if (m_physicsWorld) {
        m_scene->forEach<RigidBodyComponent>([&](Entity, RigidBodyComponent& rb) {
            if (rb.bodyId != 0) {
                m_physicsWorld->destroyBody(rb.bodyId);
                rb.bodyId = 0;
            }
        });
    }
    m_scene->registry().clear();
    m_ui.setSelectedEntity(Entity{}); // la seleccion apuntaba a un handle ya
                                       // destruido; invalidarla.
    // Hito 27: los Entity handles dentro de comandos del history apuntan
    // a entidades destruidas; vaciar el stack es la unica opcion segura.
    m_history.clear();
    // Los sol::state mapeados por entt::entity del ScriptSystem quedan
    // huerfanos: hay que tirarlos antes de recrear entidades con los
    // mismos handles numericos.
    if (m_scriptSystem) m_scriptSystem->clear();
    // AudioSystem::clear() -> AudioDevice::stopAll(): los sonidos de los
    // AudioSourceComponent de las entidades que estamos por destruir.
    if (m_audioSystem) m_audioSystem->clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

    // Piso plano que cubre toda el area del mapa. Es un cubo escalado
    // muy fino — alto 0.1m, posicion Y = -0.05 para que la cara
    // superior quede al ras del piso (y=0). Sin este piso el player
    // camina sobre el plano implicito y los demos se sienten flotando.
    {
        const f32 mapW = static_cast<f32>(m_map.width())  * tileSize;
        const f32 mapH = static_cast<f32>(m_map.height()) * tileSize;
        Entity floor = m_scene->createEntity("Floor");
        auto& tf = floor.getComponent<TransformComponent>();
        tf.position = glm::vec3(
            origin.x + mapW * 0.5f,
            origin.y - 0.05f,
            origin.z + mapH * 0.5f);
        tf.scale = glm::vec3(mapW, 0.1f, mapH);
        // Material instance unico por entidad: editar el albedoTint del
        // piso desde el Inspector NO debe contagiar a los tiles ni a otras
        // entidades que usen la misma textura.
        const MaterialAssetId floorMat =
            m_assetManager->createMaterialFromTexture(m_wallTextureId);
        floor.addComponent<MeshRendererComponent>(
            m_assetManager->missingMeshId(), floorMat);
        // Static body para que dynamics caigan sobre el piso (Jolt).
        floor.addComponent<RigidBodyComponent>(
            RigidBodyComponent::Type::Static,
            RigidBodyComponent::Shape::Box,
            glm::vec3(mapW * 0.5f, 0.05f, mapH * 0.5f),
            0.0f);
    }

    for (u32 y = 0; y < m_map.height(); ++y) {
        for (u32 x = 0; x < m_map.width(); ++x) {
            if (!m_map.isSolid(x, y)) continue;

            const std::string name = "Tile_" + std::to_string(x) + "_" + std::to_string(y);
            Entity e = m_scene->createEntity(name);

            auto& t = e.getComponent<TransformComponent>();
            t.position = glm::vec3(
                origin.x + (static_cast<f32>(x) + 0.5f) * tileSize,
                origin.y + 0.5f * tileSize,
                origin.z + (static_cast<f32>(y) + 0.5f) * tileSize);
            t.scale = glm::vec3(tileSize);

            // Mesh id 0 = cubo primitivo generado por el AssetManager.
            // Hito 17: el tile se renderiza con un material auto-generado a
            // partir de la textura del tile (envuelve la textura en un
            // MaterialAsset mate dieléctrico para que el shader PBR la
            // muestre similar al lit Phong anterior).
            // Cada tile recibe su PROPIO material instance
            // (createMaterialFromTexture, no loadMaterialFromTexture
            // cacheado): asi editar el color de un tile no contagia a los
            // demas. La memoria extra es despreciable (un MaterialAsset
            // pesa ~80 bytes; un mapa con 256 tiles serian ~20 KB).
            const MaterialAssetId tileMat =
                m_assetManager->createMaterialFromTexture(
                    m_map.tileTextureAt(x, y));
            e.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(), tileMat);

            // Hito 12: cada tile solido tambien es un static body en Jolt
            // (Box de tileSize/2 en los 3 ejes). updateRigidBodies materializa
            // el body en el proximo frame.
            e.addComponent<RigidBodyComponent>(
                RigidBodyComponent::Type::Static,
                RigidBodyComponent::Shape::Box,
                glm::vec3(tileSize * 0.5f),
                0.0f);
        }
    }
}

void EditorApplication::updateRigidBodies(f32 dt) {
    if (!m_scene || !m_physicsWorld) return;

    // 1) Materializar bodies nuevos (bodyId==0) con los valores iniciales
    //    de la entidad.
    m_scene->forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
            if (rb.bodyId != 0) return;
            CollisionShape shape = CollisionShape::Box;
            switch (rb.shape) {
                case RigidBodyComponent::Shape::Box:     shape = CollisionShape::Box; break;
                case RigidBodyComponent::Shape::Sphere:  shape = CollisionShape::Sphere; break;
                case RigidBodyComponent::Shape::Capsule: shape = CollisionShape::Capsule; break;
            }
            BodyType type = BodyType::Static;
            switch (rb.type) {
                case RigidBodyComponent::Type::Static:    type = BodyType::Static; break;
                case RigidBodyComponent::Type::Kinematic: type = BodyType::Kinematic; break;
                case RigidBodyComponent::Type::Dynamic:   type = BodyType::Dynamic; break;
            }
            // Hito 41 fix-up #2: pasar rotation del Transform al body
            // (antes el body Dynamic se creaba siempre con identidad).
            const glm::quat q = glm::quat(glm::radians(t.rotationEuler));
            rb.bodyId = m_physicsWorld->createBody(t.position, shape,
                                                    rb.halfExtents, type, rb.mass,
                                                    rb.friction,
                                                    glm::vec4(q.x, q.y, q.z, q.w));
            // Aplicar pending vels del Save/Load si las hay (caso load
            // antes de entrar a Play Mode — body recien materializado).
            if (rb.hasPendingVel && rb.bodyId != 0) {
                m_physicsWorld->setBodyLinearVelocity(rb.bodyId, rb.pendingLinearVel);
                m_physicsWorld->setBodyAngularVelocity(rb.bodyId, rb.pendingAngularVel);
                rb.hasPendingVel = false;
                rb.pendingLinearVel = glm::vec3(0.0f);
                rb.pendingAngularVel = glm::vec3(0.0f);
            }
        });

    // 2) Stepear la simulacion SOLO en Play Mode. En Editor Mode los bodies
    //    existen (para debug draw futuro) pero no se mueven.
    if (m_mode == EditorMode::Play) {
        m_physicsWorld->step(dt);
        // 3) Sync body -> Transform para bodies dinamicos. Los Static no se
        //    mueven, se salta el read para ahorrar calls.
        m_scene->forEach<TransformComponent, RigidBodyComponent>(
            [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
                if (rb.type == RigidBodyComponent::Type::Static) return;
                if (rb.bodyId == 0) return;
                t.position = m_physicsWorld->bodyPosition(rb.bodyId);
            });

        // Hito 30: sync de la camara Play con la pos del character post-step.
        // eyeOffset cambia con crouch (halfHeight 0.1 vs standing 0.5).
        // Hito 31 D: el eye Y interpola con m_crouchVisualT (smooth) +
        // suma headbob (sin() del time accumulator que avanza al caminar).
        if (m_playerCharId != 0) {
            constexpr f32 k_eyeStanding = 0.5f + 0.4f - 0.2f;  // 0.7
            constexpr f32 k_eyeCrouched = 0.1f + 0.4f - 0.2f;  // 0.3
            const f32 eye = glm::mix(k_eyeStanding, k_eyeCrouched, m_crouchVisualT);
            // Headbob suave: 5 Hz, amplitud 4 cm. Empieza desde 0 y
            // mantiene sin spike al recomenzar el movimiento.
            // Hito 34 D: amplitud escalada por velocity horizontal [0..1]
            // — full bob caminando, ~50% crouched, 0 quieto.
            constexpr f32 k_bobFreq = 5.0f * 6.2831853f; // 5 Hz a rad/s
            constexpr f32 k_bobAmp  = 0.04f;
            const f32 bobY = std::sin(m_headbobTime * k_bobFreq) * k_bobAmp
                              * m_horizSpeed01;
            const glm::vec3 charPos = m_physicsWorld->characterPosition(m_playerCharId);
            m_playCamera.setPosition(charPos + glm::vec3(0.0f, eye + bobY, 0.0f));
        }
    }
}

void EditorApplication::deleteSelectedEntity() {
    if (!m_scene) return;
    Entity selected = m_ui.selectedEntity();
    if (!selected) return;

    const std::string tagName =
        selected.hasComponent<TagComponent>()
            ? selected.getComponent<TagComponent>().name
            : std::string{"(sin tag)"};

    // Filtra tiles del mapa de prueba (Tile_X_Y) — esos vienen de
    // `m_map` y reaparecen al rebuild; eliminarlos solo confunde. Para
    // tiles, el flujo correcto es editarlos en el GridMap (futuro:
    // panel del mapa).
    const bool isTile = tagName.size() >= 5
        && tagName.compare(0, 5, "Tile_") == 0;
    if (isTile) {
        Log::editor()->info(
            "Delete: '{}' es un tile del mapa, no se elimina "
            "(usar editor de mapa para limpiar tiles).",
            tagName);
        return;
    }

    // Hito 27: ahora va por el HistoryStack — el comando captura un
    // snapshot pre-delete para permitir Ctrl+Z. El destroy + cleanup
    // del body de Jolt los hace `DeleteEntityCommand::execute()`.
    m_ui.setSelectedEntity(Entity{}); // limpiar seleccion antes del destroy
    DeleteEntityCommand::BodyCleanup cleanup;
    if (m_physicsWorld) {
        PhysicsWorld* pw = m_physicsWorld.get();
        cleanup = [pw](u32 bodyId) { pw->destroyBody(bodyId); };
    }
    auto cmd = std::make_unique<DeleteEntityCommand>(
        selected, m_scene.get(), m_assetManager.get(),
        std::move(cleanup),
        &m_history);  // Hito 32: para remap de handles post-undo
    m_history.push(std::move(cmd));
    markDirty();
}

void EditorApplication::updateTileEntity(u32 tileX, u32 tileY, TextureAssetId texture) {
    if (!m_scene) return;
    const std::string name = "Tile_" + std::to_string(tileX) + "_" + std::to_string(tileY);

    // Busqueda linear sobre ~30 entidades. Suficiente hasta que los mapas
    // pasen de 16x16 o aparezca un Scene query indexado.
    Entity found{};
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(found) && tag.name == name) found = e;
    });

    // Hito 17: el slot por submesh es ahora un MaterialAssetId. Para drops
    // de textura, generamos un material wrapper auto unico
    // (createMaterialFromTexture): drop de la misma textura sobre 2 tiles
    // distintos NO los hace compartir material.
    const MaterialAssetId matId =
        m_assetManager->createMaterialFromTexture(texture);
    if (static_cast<bool>(found)) {
        if (found.hasComponent<MeshRendererComponent>()) {
            auto& mr = found.getComponent<MeshRendererComponent>();
            if (mr.materials.empty()) mr.materials.push_back(matId);
            else mr.materials[0] = matId;
        } else {
            found.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(), matId);
        }
        return;
    }

    // La tile no tenia entidad (era Empty). Creamos una con los mismos
    // defaults que rebuildSceneFromMap.
    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();
    Entity e = m_scene->createEntity(name);
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(
        origin.x + (static_cast<f32>(tileX) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize,
        origin.z + (static_cast<f32>(tileY) + 0.5f) * tileSize);
    t.scale = glm::vec3(tileSize);
    e.addComponent<MeshRendererComponent>(
        m_assetManager->missingMeshId(), matId);
}

} // namespace Mood
