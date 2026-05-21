#include "systems/physics/ForceFieldSystem.h"

#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>        // length, normalize, distance
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>                // std::max
#include <cmath>
#include <vector>

namespace Mood {

namespace {

// Mismo test OBB que TriggerSystem: lleva `worldPoint` al espacio local de
// la zona (respeta position + rotation, ignora scale -> halfExtents en
// metros directos). Si rotationEuler == (0,0,0) equivale a un AABB.
bool obbContainsWorldPoint(const TransformComponent& tf,
                           const glm::vec3& halfExtents,
                           const glm::vec3& worldPoint) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, tf.position);
    m = glm::rotate(m, glm::radians(tf.rotationEuler.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(tf.rotationEuler.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(tf.rotationEuler.z), glm::vec3(0, 0, 1));
    const glm::vec3 local = glm::vec3(glm::inverse(m) * glm::vec4(worldPoint, 1.0f));
    return local.x >= -halfExtents.x && local.x <= halfExtents.x
        && local.y >= -halfExtents.y && local.y <= halfExtents.y
        && local.z >= -halfExtents.z && local.z <= halfExtents.z;
}

} // namespace

void ForceFieldSystem::update(Scene& scene, PhysicsWorld& physics, f32 /*dt*/) {
    // Pre-recolectar los RigidBody Dynamic materializados (Static/Kinematic
    // no responden a fuerzas). Amortiza el costo si hay varias zonas.
    struct Body { u32 bodyId; glm::vec3 pos; f32 mass; };
    std::vector<Body> bodies;
    scene.forEach<RigidBodyComponent>(
        [&](Entity, RigidBodyComponent& rb) {
            if (rb.type != RigidBodyComponent::Type::Dynamic) return;
            if (rb.bodyId == 0) return;
            bodies.push_back({rb.bodyId, physics.bodyPosition(rb.bodyId), rb.mass});
        });
    if (bodies.empty()) return;

    constexpr f32 k_eps = 1e-4f;

    scene.forEach<TransformComponent, ForceFieldComponent>(
        [&](Entity, TransformComponent& tf, ForceFieldComponent& ff) {
            if (!ff.enabled) return;
            const glm::vec3 center = tf.position;
            const bool sphere = (ff.shape == ForceFieldComponent::Shape::Sphere);
            // `range` para el falloff radial: radio (sphere) o el halfExtent
            // mas grande (box).
            const f32 range = sphere
                ? ff.radius
                : std::max({ff.halfExtents.x, ff.halfExtents.y, ff.halfExtents.z});

            for (const Body& b : bodies) {
                // --- Test de pertenencia a la zona ---
                bool inside;
                if (sphere) {
                    inside = glm::distance(b.pos, center) <= ff.radius;
                } else {
                    inside = obbContainsWorldPoint(tf, ff.halfExtents, b.pos);
                }
                if (!inside) continue;

                // --- Direccion + magnitud de la fuerza ---
                glm::vec3 dir;
                f32 mag = ff.strength;
                if (ff.mode == ForceFieldComponent::Mode::Directional) {
                    const f32 len = glm::length(ff.direction);
                    dir = (len > k_eps) ? ff.direction / len : glm::vec3(0, 1, 0);
                } else { // Radial
                    const glm::vec3 d = b.pos - center;
                    const f32 dist = glm::length(d);
                    dir = (dist > k_eps) ? d / dist : glm::vec3(0, 1, 0);
                    if (ff.linearFalloff && range > k_eps) {
                        mag *= std::max(0.0f, 1.0f - dist / range);
                    }
                }

                glm::vec3 force = dir * mag;
                // ignoreMass: la magnitud se interpreta como aceleracion
                // (m/s^2) -> F = m*a para que todos los bodies se muevan
                // igual sin importar su masa (estilo viento / gravedad).
                if (ff.ignoreMass) force *= b.mass;

                physics.addForce(b.bodyId, force);
            }
        });
}

} // namespace Mood
