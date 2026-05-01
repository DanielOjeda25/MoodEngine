#include "systems/TriggerSystem.h"

#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/ScriptSystem.h"

namespace Mood {

namespace {

bool aabbContainsPoint(const glm::vec3& center,
                        const glm::vec3& halfExtents,
                        const glm::vec3& point) {
    return point.x >= (center.x - halfExtents.x)
        && point.x <= (center.x + halfExtents.x)
        && point.y >= (center.y - halfExtents.y)
        && point.y <= (center.y + halfExtents.y)
        && point.z >= (center.z - halfExtents.z)
        && point.z <= (center.z + halfExtents.z);
}

} // namespace

void TriggerSystem::update(Scene& scene,
                            PhysicsWorld& physics,
                            ScriptSystem& scripts,
                            u32 playerCharId) {
    if (playerCharId == 0) {
        // Sin player: forzamos playerInside=false en todos los triggers
        // (sin disparar exits — el player nunca estuvo "dentro" desde la
        // perspectiva del juego). Esto cubre el caso de cambio de
        // proyecto: el player char se recrea y cualquier trigger
        // arranca "limpio".
        scene.forEach<TriggerComponent>(
            [](Entity, TriggerComponent& tr) { tr.playerInside = false; });
        return;
    }

    const glm::vec3 playerPos = physics.characterPosition(playerCharId);

    scene.forEach<TransformComponent, TriggerComponent>(
        [&](Entity e, TransformComponent& tf, TriggerComponent& tr) {
            const bool insideNow =
                aabbContainsPoint(tf.position, tr.halfExtents, playerPos);
            if (insideNow == tr.playerInside) return;  // sin cambio

            tr.playerInside = insideNow;
            const char* event = insideNow
                ? "on_trigger_enter"
                : "on_trigger_exit";
            scripts.dispatchEvent(e.handle(), event);
        });
}

} // namespace Mood
