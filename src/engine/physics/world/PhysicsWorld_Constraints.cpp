// F2H65: API de constraints (joints) — Hinge / Distance / Point.
// F2H66 Bloque 0: extraido de `PhysicsWorld.cpp` que excedio el hard cap
// de 800 LOC tras los joints. Comparte `PhysicsWorld::Impl` con el resto
// del modulo via `PhysicsWorld_Internal.h`.

#include "engine/physics/world/PhysicsWorld_Internal.h"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>

#include <glm/trigonometric.hpp>  // glm::radians

#include <cmath>

namespace Mood {

u32 PhysicsWorld::createHingeConstraint(u32 bodyA, u32 bodyB,
                                          const glm::vec3& pivotWorld,
                                          const glm::vec3& axisWorld,
                                          f32 limitMinDeg, f32 limitMaxDeg) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (bodyA == 0 || bodyB == 0) return 0;

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID idA{bodyA};
    const JPH::BodyID idB{bodyB};
    if (!bi.IsAdded(idA) || !bi.IsAdded(idB)) return 0;

    // Lock ambos bodies para tener referencias estables al crear el
    // constraint. BodyLockMultiWrite es la API correcta cuando necesitas
    // ambos al mismo tiempo (evita deadlocks por orden de locking).
    const JPH::BodyID ids[2] = {idA, idB};
    JPH::BodyLockMultiWrite lock(m_impl->physicsSystem->GetBodyLockInterface(),
                                  ids, 2);
    JPH::Body* a = lock.GetBody(0);
    JPH::Body* b = lock.GetBody(1);
    if (a == nullptr || b == nullptr) return 0;

    JPH::HingeConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
    settings.mPoint2 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
    // El hinge axis define el eje de rotacion. Normal axis define un
    // segundo eje perpendicular para medir el angulo actual. Usamos un
    // helper para construir una base ortonormal a partir del axis.
    const JPH::Vec3 hingeAxis(axisWorld.x, axisWorld.y, axisWorld.z);
    JPH::Vec3 hingeAxisNorm = hingeAxis.NormalizedOr(JPH::Vec3::sAxisY());
    // Normal axis: cualquier vector perpendicular al hinge axis. Tomamos
    // el eje X mundial; si el hinge es paralelo a X, caemos a Z.
    JPH::Vec3 normalAxis = JPH::Vec3::sAxisX();
    if (std::abs(hingeAxisNorm.Dot(normalAxis)) > 0.99f) {
        normalAxis = JPH::Vec3::sAxisZ();
    }
    normalAxis = (normalAxis - normalAxis.Dot(hingeAxisNorm) * hingeAxisNorm)
                     .Normalized();
    settings.mHingeAxis1 = hingeAxisNorm;
    settings.mHingeAxis2 = hingeAxisNorm;
    settings.mNormalAxis1 = normalAxis;
    settings.mNormalAxis2 = normalAxis;
    settings.mLimitsMin = glm::radians(limitMinDeg);
    settings.mLimitsMax = glm::radians(limitMaxDeg);

    JPH::Ref<JPH::Constraint> c = settings.Create(*a, *b);
    m_impl->physicsSystem->AddConstraint(c);

    const u32 handle = m_impl->nextConstraintId++;
    m_impl->constraints.emplace(handle, c);
    return handle;
}

u32 PhysicsWorld::createDistanceConstraint(u32 bodyA, u32 bodyB,
                                             const glm::vec3& pivotA_world,
                                             const glm::vec3& pivotB_world,
                                             f32 minDistance, f32 maxDistance) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (bodyA == 0 || bodyB == 0) return 0;

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID idA{bodyA};
    const JPH::BodyID idB{bodyB};
    if (!bi.IsAdded(idA) || !bi.IsAdded(idB)) return 0;

    const JPH::BodyID ids[2] = {idA, idB};
    JPH::BodyLockMultiWrite lock(m_impl->physicsSystem->GetBodyLockInterface(),
                                  ids, 2);
    JPH::Body* a = lock.GetBody(0);
    JPH::Body* b = lock.GetBody(1);
    if (a == nullptr || b == nullptr) return 0;

    JPH::DistanceConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = JPH::RVec3(pivotA_world.x, pivotA_world.y, pivotA_world.z);
    settings.mPoint2 = JPH::RVec3(pivotB_world.x, pivotB_world.y, pivotB_world.z);
    settings.mMinDistance = minDistance;
    settings.mMaxDistance = maxDistance;

    JPH::Ref<JPH::Constraint> c = settings.Create(*a, *b);
    m_impl->physicsSystem->AddConstraint(c);

    const u32 handle = m_impl->nextConstraintId++;
    m_impl->constraints.emplace(handle, c);
    return handle;
}

u32 PhysicsWorld::createPointConstraint(u32 bodyA, u32 bodyB,
                                          const glm::vec3& pivotWorld) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (bodyA == 0 || bodyB == 0) return 0;

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID idA{bodyA};
    const JPH::BodyID idB{bodyB};
    if (!bi.IsAdded(idA) || !bi.IsAdded(idB)) return 0;

    const JPH::BodyID ids[2] = {idA, idB};
    JPH::BodyLockMultiWrite lock(m_impl->physicsSystem->GetBodyLockInterface(),
                                  ids, 2);
    JPH::Body* a = lock.GetBody(0);
    JPH::Body* b = lock.GetBody(1);
    if (a == nullptr || b == nullptr) return 0;

    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
    settings.mPoint2 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);

    JPH::Ref<JPH::Constraint> c = settings.Create(*a, *b);
    m_impl->physicsSystem->AddConstraint(c);

    const u32 handle = m_impl->nextConstraintId++;
    m_impl->constraints.emplace(handle, c);
    return handle;
}

void PhysicsWorld::destroyConstraint(u32 constraintId) {
    if (!m_impl || !m_impl->physicsSystem) return;
    if (constraintId == 0) return;
    auto it = m_impl->constraints.find(constraintId);
    if (it == m_impl->constraints.end()) return;
    m_impl->physicsSystem->RemoveConstraint(it->second.GetPtr());
    m_impl->constraints.erase(it);
}

u32 PhysicsWorld::constraintCount() const {
    if (!m_impl) return 0;
    return static_cast<u32>(m_impl->constraints.size());
}

} // namespace Mood
