#include "systems/TriggerSystem.h"

#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/ScriptSystem.h"

#include <unordered_set>
#include <vector>

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
    // Hito 37 B: pre-recolectamos las posiciones de TODAS las entidades
    // con RigidBody (Dynamic + Kinematic — Static no se mueve, no aporta
    // valor para el flank-detection). Hacemos esto antes del loop de
    // triggers para amortizar el costo si hay muchos triggers.
    struct BodyEntry { u32 entityRaw; glm::vec3 position; };
    std::vector<BodyEntry> bodyEntries;
    scene.forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity be, TransformComponent& bt, RigidBodyComponent& rb) {
            if (rb.type == RigidBodyComponent::Type::Static) return;
            bodyEntries.push_back({static_cast<u32>(be.handle()), bt.position});
        });

    const glm::vec3 playerPos = (playerCharId != 0)
        ? physics.characterPosition(playerCharId)
        : glm::vec3(0.0f);
    const bool playerActive = (playerCharId != 0);

    scene.forEach<TransformComponent, TriggerComponent>(
        [&](Entity e, TransformComponent& tf, TriggerComponent& tr) {
            // === Player char (Hito 33 + 36 C) ===
            if (!playerActive) {
                // Sin player: forzar playerInside=false sin disparar exit
                // (el player nunca estuvo "dentro" desde la perspectiva
                // del juego). Cubre cambio de proyecto que recrea el char.
                tr.playerInside = false;
            } else {
                const bool insideNow =
                    aabbContainsPoint(tf.position, tr.halfExtents, playerPos);
                if (insideNow != tr.playerInside) {
                    tr.playerInside = insideNow;
                    scripts.dispatchEvent(e.handle(),
                        insideNow ? "on_trigger_enter" : "on_trigger_exit");
                } else if (insideNow) {
                    scripts.dispatchEvent(e.handle(), "on_trigger_stay");
                }
            }

            // === Dynamic / Kinematic bodies (Hito 37 B) ===
            // 1) Test contra todos los bodies del frame: detectar enters
            //    + emitir stay para los que ya estaban dentro.
            std::unordered_set<u32> insideThisFrame;
            for (const auto& be : bodyEntries) {
                if (!aabbContainsPoint(tf.position, tr.halfExtents, be.position)) {
                    continue;
                }
                insideThisFrame.insert(be.entityRaw);
                if (tr.bodiesInside.count(be.entityRaw) == 0) {
                    // Flank false→true: enter.
                    scripts.dispatchEvent(e.handle(),
                        "on_trigger_body_enter", be.entityRaw);
                } else {
                    // Sigue dentro: stay.
                    scripts.dispatchEvent(e.handle(),
                        "on_trigger_body_stay", be.entityRaw);
                }
            }
            // 2) Bodies que estaban en el set previo pero NO en
            //    insideThisFrame: dispatch exit. Tambien limpia entries
            //    de bodies destruidos (handle no aparece en bodyEntries).
            for (auto it = tr.bodiesInside.begin(); it != tr.bodiesInside.end();) {
                if (insideThisFrame.count(*it) == 0) {
                    scripts.dispatchEvent(e.handle(),
                        "on_trigger_body_exit", *it);
                    it = tr.bodiesInside.erase(it);
                } else {
                    ++it;
                }
            }
            // 3) Insertar enters detectados.
            for (u32 eid : insideThisFrame) {
                tr.bodiesInside.insert(eid);
            }
        });
}

} // namespace Mood
