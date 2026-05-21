// F2H75 Bloque B: wrapper sobre los soft bodies de Jolt para telas (cloth).
// Convierte un `cloth::ClothLayout` puro (sin Jolt) en un soft body vivo:
// N particulas (vertices con masa) + resortes (edges) + caras (para el
// solver). Las particulas ancladas van con invMass 0. El render lee las
// posiciones por frame con `readClothVertices`; el viento entra por
// `applyClothAcceleration`.

#include "engine/physics/world/PhysicsWorld_Internal.h"

#include "core/Log.h"
#include "engine/physics/cloth/ClothLayout.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

#include <glm/geometric.hpp>  // glm::dot
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>

namespace Mood {

u32 PhysicsWorld::createCloth(const cloth::ClothLayout& layout,
                               const glm::mat4& worldTransform,
                               f32 totalMass,
                               f32 compliance,
                               f32 linearDamping,
                               bool useGravity) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (layout.empty()) return 0;

    const usize n = layout.particles.size();

    // Masa por particula: repartimos `totalMass` entre las NO ancladas.
    // invMass de las ancladas = 0 (Jolt las trata como kinematicas: no caen).
    usize freeCount = 0;
    for (const auto& p : layout.particles) if (!p.pinned) ++freeCount;
    const f32 perMass = (freeCount > 0)
        ? std::max(totalMass, 0.01f) / static_cast<f32>(freeCount)
        : 1.0f;
    const f32 freeInvMass = (perMass > 0.0f) ? (1.0f / perMass) : 1.0f;

    JPH::Ref<JPH::SoftBodySharedSettings> shared = new JPH::SoftBodySharedSettings();
    shared->mVertices.reserve(n);
    for (const auto& p : layout.particles) {
        JPH::SoftBodySharedSettings::Vertex v;
        v.mPosition = JPH::Float3(p.localPos.x, p.localPos.y, p.localPos.z);
        v.mInvMass  = p.pinned ? 0.0f : freeInvMass;
        shared->mVertices.push_back(v);
    }

    // Edges (resortes). Rest length viene precalculado del layout; compliance
    // global (0 = rigido). El bend ya esta materializado como edge en el
    // layout, asi que NO usamos CreateConstraints (que regeneraria desde
    // faces) — cargamos los edges directo.
    shared->mEdgeConstraints.reserve(layout.edges.size());
    for (const auto& e : layout.edges) {
        JPH::SoftBodySharedSettings::Edge je(
            static_cast<JPH::uint32>(e.a), static_cast<JPH::uint32>(e.b),
            std::max(compliance, 0.0f));
        je.mRestLength = e.restLength;
        shared->mEdgeConstraints.push_back(je);
    }

    // Faces: dan superficie al solver (necesarias para que el soft body
    // sea valido + futuro aero/colision). Vienen de los triangulos del layout.
    const auto& tri = layout.triangleIndices;
    for (usize t = 0; t + 2 < tri.size(); t += 3) {
        JPH::SoftBodySharedSettings::Face f(tri[t], tri[t + 1], tri[t + 2]);
        if (!f.IsDegenerate()) shared->AddFace(f);
    }

    // Optimize reordena constraints para el solver paralelo. NO recalcula
    // rest lengths (ya estan seteados).
    shared->Optimize();

    // Pose inicial: descomponemos la worldMatrix en translation + rotation.
    const glm::vec3 pos(worldTransform[3]);
    const glm::quat rot = glm::quat_cast(glm::mat3(worldTransform));

    JPH::SoftBodyCreationSettings cs(
        shared,
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat(rot.x, rot.y, rot.z, rot.w),
        physics_internal::toJPHLayer(ObjectLayer::Moving));
    cs.mGravityFactor = useGravity ? 1.0f : 0.0f;
    cs.mLinearDamping = std::max(linearDamping, 0.0f);
    cs.mPressure      = 0.0f;       // tela plana, sin volumen interno
    cs.mUpdatePosition = false;     // el centro de masa NO se traslada solo:
                                    // las particulas ancladas fijan la tela
                                    // en su lugar; dejar que el body "siga"
                                    // su COM haria flotar el origen.

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    const JPH::BodyID id =
        bi.CreateAndAddSoftBody(cs, JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        Log::physics()->error(
            "PhysicsWorld::createCloth: CreateAndAddSoftBody fallo (probable "
            "out of bodies — tope global del motor).");
        return 0;
    }

    const u32 handle = m_impl->nextClothId++;
    PhysicsWorld::Impl::ClothEntry entry;
    entry.bodyId      = id;
    entry.shared      = shared;
    entry.vertexCount = static_cast<u32>(n);
    m_impl->cloths.emplace(handle, std::move(entry));
    Log::physics()->info(
        "PhysicsWorld::createCloth: id={} vertices={} edges={} faces={} "
        "mass={} gravity={}",
        handle, n, layout.edges.size(), tri.size() / 3, totalMass, useGravity);
    return handle;
}

void PhysicsWorld::destroyCloth(u32 clothId) {
    if (!m_impl || clothId == 0) return;
    auto it = m_impl->cloths.find(clothId);
    if (it == m_impl->cloths.end()) return;
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    if (!it->second.bodyId.IsInvalid()) {
        m_impl->bodyToEntity.erase(
            it->second.bodyId.GetIndexAndSequenceNumber());
        bi.RemoveBody(it->second.bodyId);
        bi.DestroyBody(it->second.bodyId);
    }
    m_impl->cloths.erase(it);
}

bool PhysicsWorld::readClothVertices(
    u32 clothId, std::vector<glm::vec3>& outWorldPositions) const {
    if (!m_impl || clothId == 0) return false;
    auto it = m_impl->cloths.find(clothId);
    if (it == m_impl->cloths.end() || it->second.bodyId.IsInvalid()) {
        return false;
    }

    const JPH::BodyLockInterface& bli =
        m_impl->physicsSystem->GetBodyLockInterface();
    JPH::BodyLockRead lock(bli, it->second.bodyId);
    if (!lock.Succeeded()) return false;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(
        body.GetMotionProperties());
    if (mp == nullptr) return false;

    // Las posiciones de los vertices estan relativas al centro de masa del
    // soft body. Las llevamos a world con la COM transform.
    const JPH::RMat44 com = body.GetCenterOfMassTransform();
    const auto& verts = mp->GetVertices();
    outWorldPositions.resize(verts.size());
    for (usize i = 0; i < verts.size(); ++i) {
        const JPH::RVec3 w = com * verts[i].mPosition;
        outWorldPositions[i] =
            glm::vec3(static_cast<f32>(w.GetX()),
                      static_cast<f32>(w.GetY()),
                      static_cast<f32>(w.GetZ()));
    }
    return true;
}

void PhysicsWorld::applyClothAcceleration(u32 clothId,
                                           const glm::vec3& accelWorld,
                                           f32 dt) {
    if (!m_impl || clothId == 0 || dt <= 0.0f) return;
    auto it = m_impl->cloths.find(clothId);
    if (it == m_impl->cloths.end() || it->second.bodyId.IsInvalid()) return;
    if (glm::dot(accelWorld, accelWorld) < 1e-8f) return;

    const JPH::BodyLockInterface& bli =
        m_impl->physicsSystem->GetBodyLockInterface();
    JPH::BodyLockWrite lock(bli, it->second.bodyId);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(
        body.GetMotionProperties());
    if (mp == nullptr) return;

    // Sumamos a la velocidad de las particulas NO ancladas (invMass > 0).
    // Tratamos `accelWorld` como aceleracion (mass-independent) integrando
    // por dt: dv = a * dt. Asi el viento mueve toda la tela parejo sin
    // importar la masa por particula. Las ancladas (invMass 0) no se tocan.
    const JPH::Vec3 dv(accelWorld.x * dt, accelWorld.y * dt, accelWorld.z * dt);
    auto& verts = mp->GetVertices();
    for (auto& v : verts) {
        if (v.mInvMass > 0.0f) v.mVelocity += dv;
    }
    // El soft body ya esta activo; tocar velocidades no requiere re-activar
    // explicitamente (el solver corre cada step mientras el body este awake).
}

u32 PhysicsWorld::clothCount() const {
    if (!m_impl) return 0;
    return static_cast<u32>(m_impl->cloths.size());
}

} // namespace Mood
