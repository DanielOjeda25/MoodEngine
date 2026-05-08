// F2H24: nucleo de PlayerApplication. Lifecycle del player partido en
// archivos parciales:
//   _Init.cpp     — ctor / dtor / tryLoadGameManifest (boot + shutdown)
//   _Frame.cpp    — processEvents / beginFrame / endFrame / updateCamera
//                   / updateRigidBodies / run() (loop principal)
//   _SaveLoad.cpp — main menu + save/load (.moodsave) + quicksave / saveAs
//
// Aca solo viven los helpers compartidos por todos:
// mapWorldOrigin (geometria del mapa), buildTestMap (placeholder map
// del fallback) y rebuildSceneFromMap (instanciar tiles + piso).

#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <string>

namespace Mood {

glm::vec3 PlayerApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

void PlayerApplication::buildTestMap() {
    // Sala abierta 16x16 (mismo placeholder que el editor: piso + 1
    // columna brick). Sin muros perimetrales.
    m_map = GridMap(16u, 16u, 3.0f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    const TextureAssetId brick = m_assetManager->loadTexture("textures/brick.png");
    (void)grid;

    m_map.setTile(m_map.width() / 2u, m_map.height() / 2u,
                  TileType::SolidWall, brick);
    Log::world()->info("MoodPlayer: arena 16x16 ({} tiles solidos)",
                        m_map.solidCount());
}

void PlayerApplication::rebuildSceneFromMap() {
    m_scene->registry().clear();

    const glm::vec3 origin = mapWorldOrigin();
    const f32 tileSize = m_map.tileSize();

    // Piso plano (mismo patron que EditorApplication::rebuildSceneFromMap).
    {
        const f32 mapW = static_cast<f32>(m_map.width())  * tileSize;
        const f32 mapH = static_cast<f32>(m_map.height()) * tileSize;
        const TextureAssetId floorTex =
            m_assetManager->loadTexture("textures/grid.png");
        Entity floor = m_scene->createEntity("Floor");
        auto& tf = floor.getComponent<TransformComponent>();
        tf.position = glm::vec3(
            origin.x + mapW * 0.5f,
            origin.y - 0.05f,
            origin.z + mapH * 0.5f);
        tf.scale = glm::vec3(mapW, 0.1f, mapH);
        // Material instance unico (paridad con EditorScene::rebuildSceneFromMap).
        const MaterialAssetId floorMat =
            m_assetManager->createMaterialFromTexture(floorTex);
        floor.addComponent<MeshRendererComponent>(
            m_assetManager->missingMeshId(), floorMat);
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

            // Cada tile recibe su propio material instance (paridad con
            // EditorScene::rebuildSceneFromMap).
            const MaterialAssetId tileMat =
                m_assetManager->createMaterialFromTexture(
                    m_map.tileTextureAt(x, y));
            e.addComponent<MeshRendererComponent>(
                m_assetManager->missingMeshId(), tileMat);

            // RigidBody static para los tiles (mismo flow que el editor):
            // permite que entidades dinamicas (cajas spawnedas via script)
            // colisionen contra las paredes.
            e.addComponent<RigidBodyComponent>(
                RigidBodyComponent::Type::Static,
                RigidBodyComponent::Shape::Box,
                glm::vec3(tileSize * 0.5f),
                0.0f);
        }
    }
}

} // namespace Mood
