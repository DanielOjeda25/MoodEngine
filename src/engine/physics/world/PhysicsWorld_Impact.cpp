// F2H68: auto-ragdoll por impacto vehicle <-> NPC. Implementa el
// ContactListener + la API publica del map BodyID->Entity + cola de
// eventos. Split del PhysicsWorld principal para mantener el archivo
// chico (politica F2H66: cap blando 800 LOC por .cpp del modulo).
//
// Diseño general:
//   1. PhysicsSystem llama `OnContactAdded(b1, b2, ...)` cada vez que
//      detecta un contacto nuevo entre 2 bodies este step.
//   2. Calculamos la velocidad relativa proyectada sobre la normal del
//      contacto. Si supera `impactSpeedThreshold`, encolamos UN evento
//      por cada body (asi el drain en RagdollSystem puede elegir cual
//      es la victima animada).
//   3. RagdollSystem drena la cola pre-materialize. Para cada evento,
//      resolve `entityOfBody(victimBodyId)`, chequea
//      RagdollComponent::Animated, y si aplica: setea state=Ragdolling
//      + spawnImpulse + dirty=true.
//
// El callback SOLO encola: Jolt prohibe mutar bodies / shapes / world
// desde el listener (puede correr en threads del JobSystem). Por eso
// usamos `std::mutex` en la cola — defensivo aunque en la practica
// PhysicsSystem usualmente serializa los callbacks.

#include "engine/physics/world/PhysicsWorld_Internal.h"

#include "core/Log.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace Mood::physics_internal {

// Helper: convertir JPH::Vec3 -> glm::vec3.
static glm::vec3 toGlm(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

void ContactListener::OnContactAdded(const JPH::Body& body1,
                                       const JPH::Body& body2,
                                       const JPH::ContactManifold& manifold,
                                       JPH::ContactSettings& /*settings*/) {
    if (owner == nullptr) return;

    // Skip pares sin movimiento posible: si ambos bodies estan Static o
    // ambos son Kinematic sin velocidad linear, no hay impacto fisico.
    if (!body1.IsDynamic() && !body2.IsDynamic()) return;

    // Velocidad relativa en el punto de contacto (mundo). Usamos el
    // centro del manifold como punto de referencia; para impacto puntual
    // chassis-vs-NPC esto es lo suficientemente preciso.
    const JPH::Vec3 v1 = body1.GetLinearVelocity();
    const JPH::Vec3 v2 = body2.GetLinearVelocity();
    const JPH::Vec3 vrel = v1 - v2;  // velocidad de body1 vista desde body2

    // Jolt convencion (Vehicle/Constraints/ContactListener docs): la normal
    // del manifold apunta del body1 al body2 (la direccion para "empujar
    // body2 fuera del body1 por el camino mas corto").
    //
    // - vrel = v1 - v2.
    // - vrel.Dot(normal) > 0 => body1 se mueve hacia body2 (aproximacion).
    //   Eso es un impacto real; magnitud = closing speed.
    // - vrel.Dot(normal) < 0 => se alejan; no es impacto.
    const JPH::Vec3 normal = manifold.mWorldSpaceNormal;
    const f32 closingSpeed = vrel.Dot(normal);

    if (closingSpeed < owner->impactSpeedThreshold) {
        return;
    }

    // Impulse magnitude estilo arcade: la velocidad de cierre escalada
    // por el factor configurado. NO usamos masa porque ya estamos
    // tuneando con `impactImpulseFactor` para feel — el calculo fisico
    // exacto (vrel * massReducida) manda los cuerpos volando.
    const f32 impulseMag = closingSpeed * owner->impactImpulseFactor;
    const glm::vec3 normalG = toGlm(normal);
    // Body2 es la "victima" en el sentido geometrico: la normal apunta
    // hacia el, asi que el impacto lo empuja en direccion +normal. Body1
    // es el "atacante" y recibe un impulse de retroceso (-normal). El
    // drain del RagdollSystem decide cual de los 2 era ragdoll-able:
    // tipicamente body1 es el vehicle (no Animated) y body2 el NPC.
    const glm::vec3 impulse1 = -normalG * impulseMag;
    const glm::vec3 impulse2 =  normalG * impulseMag;

    const u32 id1 = body1.GetID().GetIndexAndSequenceNumber();
    const u32 id2 = body2.GetID().GetIndexAndSequenceNumber();

    {
        std::lock_guard<std::mutex> lock(owner->impactQueueMutex);
        owner->impactQueue.push_back({id1, impulse1, closingSpeed});
        owner->impactQueue.push_back({id2, impulse2, closingSpeed});
    }
}

} // namespace Mood::physics_internal

namespace Mood {

void PhysicsWorld::registerBodyEntity(u32 bodyId, u32 entityHandle) {
    if (!m_impl || bodyId == 0) return;
    m_impl->bodyToEntity[bodyId] = entityHandle;
}

void PhysicsWorld::unregisterBodyEntity(u32 bodyId) {
    if (!m_impl || bodyId == 0) return;
    m_impl->bodyToEntity.erase(bodyId);
}

u32 PhysicsWorld::entityOfBody(u32 bodyId) const {
    if (!m_impl || bodyId == 0) return 0;
    auto it = m_impl->bodyToEntity.find(bodyId);
    if (it == m_impl->bodyToEntity.end()) return 0;
    return it->second;
}

std::vector<PhysicsWorld::RagdollImpactEvent>
PhysicsWorld::drainImpactEvents() {
    std::vector<RagdollImpactEvent> out;
    if (!m_impl) return out;
    std::vector<Impl::ImpactEvent> local;
    {
        std::lock_guard<std::mutex> lock(m_impl->impactQueueMutex);
        std::swap(local, m_impl->impactQueue);
    }
    out.reserve(local.size());
    for (const auto& ev : local) {
        out.push_back({ev.victimBodyId, ev.impulseWorld, ev.impactSpeed});
    }
    return out;
}

void PhysicsWorld::setRagdollImpactSpeedThreshold(f32 metersPerSecond) {
    if (!m_impl) return;
    m_impl->impactSpeedThreshold = std::max(0.0f, metersPerSecond);
}

void PhysicsWorld::setRagdollImpactFactor(f32 factor) {
    if (!m_impl) return;
    m_impl->impactImpulseFactor = std::max(0.0f, factor);
}

std::vector<u32> PhysicsWorld::ragdollBodyIds(u32 ragdollId) const {
    std::vector<u32> out;
    if (!m_impl || ragdollId == 0) return out;
    auto it = m_impl->ragdolls.find(ragdollId);
    if (it == m_impl->ragdolls.end() || it->second == nullptr) return out;
    const JPH::Ragdoll* rag = it->second.GetPtr();
    const JPH::Array<JPH::BodyID>& ids = rag->GetBodyIDs();
    out.reserve(ids.size());
    for (const auto& id : ids) {
        out.push_back(id.GetIndexAndSequenceNumber());
    }
    return out;
}

} // namespace Mood
