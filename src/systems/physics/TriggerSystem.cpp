#include "systems/physics/TriggerSystem.h"

#include "core/Log.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/scripting/ScriptSystem.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <unordered_set>
#include <vector>

namespace Mood {

namespace {

// Hito 39 B: testea si `worldPoint` cae en el AABB del trigger,
// transformado al espacio LOCAL del trigger (respeta rotation +
// translation del Transform; ignora scale para que halfExtents siga
// siendo metros directos). Si rotationEuler == (0,0,0), el resultado
// es identico al AABB axis-aligned anterior.
bool obbContainsWorldPoint(const TransformComponent& tf,
                            const glm::vec3& halfExtents,
                            const glm::vec3& worldPoint) {
    // Construir matrix solo con position + rotation (sin scale).
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

void TriggerSystem::update(Scene& scene,
                            PhysicsWorld& physics,
                            ScriptSystem& scripts,
                            u32 playerCharId) {
    // Hito 37 B: pre-recolectamos las posiciones de TODAS las entidades
    // con RigidBody (Dynamic + Kinematic — Static no se mueve, no aporta
    // valor para el flank-detection). Hacemos esto antes del loop de
    // triggers para amortizar el costo si hay muchos triggers.
    // F2H73: guardamos tambien el tag de cada body para el filtro
    // `requiredTag` (solo disparan los bodies cuyo Tag coincide).
    struct BodyEntry { u32 entityRaw; glm::vec3 position; std::string tag; };
    std::vector<BodyEntry> bodyEntries;
    scene.forEach<TransformComponent, RigidBodyComponent>(
        [&](Entity be, TransformComponent& bt, RigidBodyComponent& rb) {
            if (rb.type == RigidBodyComponent::Type::Static) return;
            std::string tag;
            if (be.hasComponent<TagComponent>()) {
                tag = be.getComponent<TagComponent>().name;
            }
            bodyEntries.push_back(
                {static_cast<u32>(be.handle()), bt.position, std::move(tag)});
        });

    const glm::vec3 playerPos = (playerCharId != 0)
        ? physics.characterPosition(playerCharId)
        : glm::vec3(0.0f);
    const bool playerActive = (playerCharId != 0);

    scene.forEach<TransformComponent, TriggerComponent>(
        [&](Entity e, TransformComponent& tf, TriggerComponent& tr) {
            // F2H73: disabled -> no dispatcha nada. oneShot ya disparado ->
            // el trigger esta "muerto" hasta recargar el mapa.
            if (!tr.enabled) return;
            if (tr.oneShot && tr.fired) return;

            // === Player char (Hito 33 + 36 C) ===
            // F2H73: triggersOnPlayer=false ignora al player (solo bodies).
            if (!playerActive || !tr.triggersOnPlayer) {
                // Sin player (o ignorado): forzar playerInside=false sin
                // disparar exit. Cubre cambio de proyecto que recrea el char.
                tr.playerInside = false;
            } else {
                const bool insideNow =
                    obbContainsWorldPoint(tf, tr.halfExtents, playerPos);
                if (insideNow != tr.playerInside) {
                    tr.playerInside = insideNow;
                    const char* evt =
                        insideNow ? "on_trigger_enter" : "on_trigger_exit";
                    Log::script()->info(
                        "[trigger#{}] player flank -> {} (dispatch {})",
                        static_cast<u32>(e.handle()),
                        insideNow ? "INSIDE" : "OUTSIDE", evt);
                    scripts.dispatchEvent(e.handle(), evt);
                    // F2H73: one-shot se arma al primer enter.
                    if (insideNow && tr.oneShot) {
                        tr.fired = true;
                        return;  // no procesar bodies este frame; ya disparo.
                    }
                } else if (insideNow) {
                    scripts.dispatchEvent(e.handle(), "on_trigger_stay");
                }
            }

            // === Dynamic / Kinematic bodies (Hito 37 B) ===
            // 1) Test contra todos los bodies del frame: detectar enters
            //    + emitir stay para los que ya estaban dentro.
            std::unordered_set<u32> insideThisFrame;
            for (const auto& be : bodyEntries) {
                // F2H73: filtro por tag — si requiredTag no esta vacio, solo
                // los bodies con ese Tag cuentan (no entran al set ni
                // disparan).
                if (!tr.requiredTag.empty() && be.tag != tr.requiredTag) {
                    continue;
                }
                if (!obbContainsWorldPoint(tf, tr.halfExtents, be.position)) {
                    continue;
                }
                insideThisFrame.insert(be.entityRaw);
                if (tr.bodiesInside.count(be.entityRaw) == 0) {
                    // Flank false→true: enter.
                    Log::script()->info(
                        "[trigger#{}] body#{} flank -> INSIDE (dispatch on_trigger_body_enter)",
                        static_cast<u32>(e.handle()), be.entityRaw);
                    scripts.dispatchEvent(e.handle(),
                        "on_trigger_body_enter", be.entityRaw);
                    // F2H73: one-shot se arma al primer body enter.
                    if (tr.oneShot) {
                        tr.fired = true;
                        for (u32 eid : insideThisFrame) tr.bodiesInside.insert(eid);
                        return;
                    }
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
                    Log::script()->info(
                        "[trigger#{}] body#{} flank -> OUTSIDE (dispatch on_trigger_body_exit)",
                        static_cast<u32>(e.handle()), *it);
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
