#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/world/GridMap.h"
#include "systems/AudioSystem.h"
#include "systems/ScriptSystem.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

void EditorApplication::buildInitialTestMap() {
    // 8x8 con bordes sólidos (grid.png) y columna central (brick.png) para
    // validar texturas por tile. Mismo estado que arrancaba el editor desde
    // el Hito 5; se reusa al cerrar un proyecto para volver a un baseline
    // conocido.
    m_map = GridMap(8u, 8u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");
    m_wallTextureId = grid;

    for (u32 i = 0; i < m_map.width(); ++i) {
        m_map.setTile(i, 0u,                   TileType::SolidWall, grid);
        m_map.setTile(i, m_map.height() - 1u,  TileType::SolidWall, grid);
    }
    for (u32 j = 0; j < m_map.height(); ++j) {
        m_map.setTile(0u,                  j, TileType::SolidWall, grid);
        m_map.setTile(m_map.width() - 1u,  j, TileType::SolidWall, grid);
    }
    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("Mapa de prueba cargado: prueba_8x8 ({} tiles solidos)",
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
    // Los sol::state mapeados por entt::entity del ScriptSystem quedan
    // huerfanos: hay que tirarlos antes de recrear entidades con los
    // mismos handles numericos.
    if (m_scriptSystem) m_scriptSystem->clear();
    // AudioSystem::clear() -> AudioDevice::stopAll(): los sonidos de los
    // AudioSourceComponent de las entidades que estamos por destruir.
    if (m_audioSystem) m_audioSystem->clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

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
            const MaterialAssetId tileMat =
                m_assetManager->loadMaterialFromTexture(
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
            rb.bodyId = m_physicsWorld->createBody(t.position, shape,
                                                    rb.halfExtents, type, rb.mass);
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
    }
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
    // de textura, generamos un material wrapper auto.
    const MaterialAssetId matId =
        m_assetManager->loadMaterialFromTexture(texture);
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
