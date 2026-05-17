#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/DeleteEntityCommand.h"
#include "editor/commands/SetTileCommand.h"  // F2H62 polish: delete tile -> Empty
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/grid/GridMap.h"
#include "systems/audio/AudioSystem.h"
#include "systems/scripting/ScriptSystem.h"

#include <glm/vec3.hpp>
#include <glm/common.hpp>          // glm::mix
#include <glm/gtc/quaternion.hpp>   // Hito 41 fix-up: euler → quat

#include <cmath>            // std::sin
#include <cstdio>           // F2H62 polish: sscanf para parsear "Tile_X_Y"
#include <string>

namespace Mood {

void EditorApplication::buildInitialTestMap() {
    // Sala abierta 16x16 con piso plano + UNA columna brick central
    // como obstaculo de prueba para pathfinding y selecciones. Sin
    // muros perimetrales — esos quedaban como una hilera de cubos
    // dentados que se veian feo. El skybox cubre el horizonte.
    // El piso es una entidad agregada en rebuildSceneFromMap.
    // F2H23 polish: mapa default 8x8 con tileSize=1.5 = Floor de 12x12m.
    // Antes era 16x16 con tileSize=3 = Floor de 48m, lo que hacia que un
    // brush primitivo 1x1x1m se viera diminuto al lado del piso. Mas
    // chico = mejor escala visual con los brushes default. El dev puede
    // agrandar el mapa desde el Inspector si lo necesita.
    m_map = GridMap(8u, 8u, 1.5f);
    const TextureAssetId grid  = m_assetManager->loadTexture("textures/grid.png");
    m_wallTextureId = grid;

    // F2H17 Bloque G: el mapa nuevo arranca vacio. Antes spawneabamos
    // un cubo brick.png en el centro como prueba del Hito 4/5; con el
    // workflow CSG de F2H11+ ese cubo era ruido visual constante. Si
    // se necesita un brush de prueba, el dev usa "Anadir Box Brush"
    // del menu.
    Log::world()->info("Mapa nuevo cargado: arena_16x16 vacia");
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
            // F2H40: trackear el halfExtents inicial sincronizado al body.
            if (rb.bodyId != 0) {
                rb.lastSyncedHalfExtents = rb.halfExtents;
            }
        });

    // F2H40: pase de re-sync para bodies ya materializados. Cubre dos
    // casos que pre-F2H40 desincronizaban visual y colision:
    //   (a) Box body con `Transform.scale` cambiado via Inspector / gizmo.
    //       Auto-update `halfExtents = t.scale * 0.5` (Box es la unica
    //       shape donde la equivalencia es semanticamente intuitiva —
    //       Sphere/Capsule tienen halfExtents.x = radius / etc.).
    //   (b) Cualquier shape con `halfExtents` editado directo en el
    //       Inspector. El sync compara contra `lastSyncedHalfExtents`.
    // `setBodyHalfExtents` llama Jolt `BodyInterface::SetShape` que
    // preserva pose + velocity + contacts (no destroy + recreate).
    m_scene->forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity, TransformComponent& t, RigidBodyComponent& rb) {
            if (rb.bodyId == 0) return;
            // (a) Box auto-sync desde Transform.scale.
            if (rb.shape == RigidBodyComponent::Shape::Box) {
                const glm::vec3 desired = t.scale * 0.5f;
                constexpr f32 k_eps = 1e-4f;
                if (std::abs(desired.x - rb.halfExtents.x) > k_eps ||
                    std::abs(desired.y - rb.halfExtents.y) > k_eps ||
                    std::abs(desired.z - rb.halfExtents.z) > k_eps) {
                    rb.halfExtents = desired;
                }
            }
            // (b) Resync al body si difiere del ultimo aplicado.
            constexpr f32 k_eps = 1e-4f;
            if (std::abs(rb.halfExtents.x - rb.lastSyncedHalfExtents.x) > k_eps ||
                std::abs(rb.halfExtents.y - rb.lastSyncedHalfExtents.y) > k_eps ||
                std::abs(rb.halfExtents.z - rb.lastSyncedHalfExtents.z) > k_eps) {
                CollisionShape shape = CollisionShape::Box;
                switch (rb.shape) {
                    case RigidBodyComponent::Shape::Box:     shape = CollisionShape::Box;     break;
                    case RigidBodyComponent::Shape::Sphere:  shape = CollisionShape::Sphere;  break;
                    case RigidBodyComponent::Shape::Capsule: shape = CollisionShape::Capsule; break;
                }
                m_physicsWorld->setBodyHalfExtents(rb.bodyId, shape, rb.halfExtents);
                rb.lastSyncedHalfExtents = rb.halfExtents;
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
            // Headbob suave: 3.5 Hz, amplitud 5 cm. Empieza desde 0 y
            // mantiene sin spike al recomenzar el movimiento.
            // Hito 34 D: amplitud escalada por velocity horizontal [0..1]
            // — full bob caminando, ~50% crouched, 0 quieto.
            // F2H41 fix lateral: bajado de 5 Hz a 3.5 Hz para que los
            // pasos no se sientan "muy pegados". A walkSpeed 5.5 m/s
            // (ver EditorPlayMode.cpp), 3.5 Hz da ~1.6 m por paso —
            // stride humana realista. Amplitud subida a 5 cm para
            // compensar la menor frecuencia y mantener visibilidad.
            constexpr f32 k_bobFreq = 3.5f * 6.2831853f; // 3.5 Hz a rad/s
            constexpr f32 k_bobAmp  = 0.05f;
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

    // F2H62 polish: tiles del mapa (Tag "Tile_X_Y") son rendereados como
    // entidades pero el "source of truth" es m_map (GridMap). Si el dev
    // pide "Eliminar" sobre un tile, lo natural es que la celda quede
    // vacia -- no que la entidad se borre y reaparezca al rebuild. Lo
    // hacemos via SetTileCommand para que sea undoable.
    // Pre-fix: el delete sobre tiles era bloqueado con un log info que
    // el dev no leia; parecia un bug.
    const bool isTile = tagName.size() >= 5
        && tagName.compare(0, 5, "Tile_") == 0;
    if (isTile) {
        u32 tx = 0, ty = 0;
        if (std::sscanf(tagName.c_str(), "Tile_%u_%u", &tx, &ty) == 2) {
            const TileType oldType = m_map.tileAt(tx, ty);
            const TextureAssetId oldTex = m_map.tileTextureAt(tx, ty);
            if (oldType == TileType::Empty) {
                Log::editor()->info("Delete: tile ({}, {}) ya estaba vacio", tx, ty);
                return;
            }
            auto sync = [this](u32 x, u32 y, TextureAssetId tex) {
                updateTileEntity(x, y, tex);
            };
            m_history.push(std::make_unique<SetTileCommand>(
                &m_map, std::move(sync), tx, ty,
                oldType, oldTex,
                TileType::Empty, 0u,
                "Vaciar tile"));
            markDirty();
            m_ui.setStatusMessage("Tile (" + std::to_string(tx) + ", " +
                                    std::to_string(ty) + ") vaciado");
            Log::editor()->info("Delete: tile ({}, {}) -> Empty", tx, ty);
            return;
        }
        Log::editor()->warn(
            "Delete: '{}' parece tile pero no pude parsear coords; abortado",
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

    // F2H62 polish: si el tile esta en Empty (Set Empty o Delete), NO crear
    // ni actualizar la entidad -- destruirla si existe. Sino el "borrado"
    // dejaba un cubo zombie con textura missing.
    if (m_map.tileAt(tileX, tileY) == TileType::Empty) {
        if (static_cast<bool>(found)) {
            m_scene->destroyEntity(found);
        }
        return;
    }

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
