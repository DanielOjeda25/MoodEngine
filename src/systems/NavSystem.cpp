#include "systems/NavSystem.h"

#include "core/math/AABB.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/world/GridMap.h"
#include "engine/world/Pathfinding.h"
#include "systems/PhysicsSystem.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace Mood {

namespace {

// Convierte una posicion world (XZ) a coordenadas de tile usando el
// origen del mapa y tileSize. Retorna ivec2; el caller debe verificar
// rangos contra el width/height del mapa antes de usar.
glm::ivec2 worldToTile(const glm::vec3& world,
                       const glm::vec3& origin,
                       f32 tileSize) {
    const f32 lx = world.x - origin.x;
    const f32 lz = world.z - origin.z;
    return glm::ivec2(
        static_cast<int>(std::floor(lx / tileSize)),
        static_cast<int>(std::floor(lz / tileSize)));
}

// Centro world-space del tile (tx, tz).
glm::vec3 tileCenter(int tx, int tz, const glm::vec3& origin, f32 tileSize) {
    return glm::vec3(
        origin.x + (static_cast<f32>(tx) + 0.5f) * tileSize,
        origin.y + 0.5f * tileSize, // mismo Y que el player tipico
        origin.z + (static_cast<f32>(tz) + 0.5f) * tileSize);
}

} // namespace

void NavSystem::update(Scene& scene,
                       const GridMap& map,
                       const glm::vec3& mapWorldOrigin,
                       f32 dt) {
    const f32 tileSize = map.tileSize();
    const u32 W = map.width();
    const u32 H = map.height();

    scene.forEach<TransformComponent, NavAgentComponent>(
        [&](Entity, TransformComponent& tf, NavAgentComponent& nav) {
            if (!nav.active) return;

            // Repath check: throttle (0.5s) Y delta del target > 1m.
            nav.repathAccumulator += dt;
            const f32 targetMoved = glm::length(nav.target - nav.lastPathTarget);
            const bool needRepath =
                nav.path.empty()
                || nav.repathAccumulator >= k_repathInterval
                || targetMoved > k_repathTargetEpsilon;
            if (needRepath) {
                nav.repathAccumulator = 0.0f;

                const glm::ivec2 startTile =
                    worldToTile(tf.position, mapWorldOrigin, tileSize);
                const glm::ivec2 goalTile  =
                    worldToTile(nav.target,  mapWorldOrigin, tileSize);

                // Validacion de rangos antes de pedirle a A* — fuera del
                // grid se trata como "sin path".
                if (startTile.x < 0 || startTile.y < 0
                    || static_cast<u32>(startTile.x) >= W
                    || static_cast<u32>(startTile.y) >= H
                    || goalTile.x < 0 || goalTile.y < 0
                    || static_cast<u32>(goalTile.x)  >= W
                    || static_cast<u32>(goalTile.y)  >= H) {
                    nav.path.clear();
                    nav.pathIndex = 0;
                    nav.lastPathTarget = nav.target;
                    return;
                }

                const auto astarPath = Pathfinding::findPath(
                    map,
                    {static_cast<u32>(startTile.x), static_cast<u32>(startTile.y)},
                    {static_cast<u32>(goalTile.x),  static_cast<u32>(goalTile.y)});

                nav.path.clear();
                nav.path.reserve(astarPath.size());
                for (const auto& tc : astarPath) {
                    nav.path.push_back(glm::ivec2(static_cast<int>(tc.x),
                                                    static_cast<int>(tc.y)));
                }
                nav.pathIndex = 0;
                nav.lastPathTarget = nav.target;
            }

            if (nav.path.empty()) return;
            if (nav.pathIndex >= nav.path.size()) return; // ya llego

            // Steering: avanzar hacia el centro del proximo waypoint en
            // plano XZ. Y se mantiene fija.
            const glm::ivec2 wp = nav.path[nav.pathIndex];
            const glm::vec3 wpCenter = tileCenter(wp.x, wp.y, mapWorldOrigin, tileSize);
            const glm::vec3 to = wpCenter - tf.position;
            const glm::vec3 toXZ(to.x, 0.0f, to.z);
            const f32 distXZ = glm::length(toXZ);

            // Avanzar el waypoint si esta cerca. Margen = tileSize/2 para
            // que el agente no oscile sobre cada tile.
            if (distXZ < tileSize * 0.5f) {
                nav.pathIndex += 1;
                if (nav.pathIndex >= nav.path.size()) return;
            }

            // Steering vector unitario en XZ.
            if (distXZ < 1e-4f) return;
            const glm::vec3 dir = toXZ / distXZ;
            const glm::vec3 desired = dir * (nav.speed * dt);

            // Colision con el grid igual que el player. NavAgent tambien
            // usa moveAndSlide para no atravesar paredes en zonas donde
            // el path roza una esquina.
            AABB box{tf.position - m_halfExtents, tf.position + m_halfExtents};
            const glm::vec3 actual = moveAndSlide(map, mapWorldOrigin, box, desired);
            tf.position += actual;
        });
}

} // namespace Mood
