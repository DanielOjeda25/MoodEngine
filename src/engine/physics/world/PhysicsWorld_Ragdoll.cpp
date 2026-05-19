// F2H66 Bloque C: implementacion del wrapper sobre `JPH::Ragdoll`.
// Convierte un `ragdoll::RagdollLayout` puro (sin Jolt) en un Ragdoll
// vivo en el physics system: N bodies (capsules) + (N-1) constraints
// (Hinge o SwingTwist) + GroupFilterTable que deshabilita self-collision
// entre partes del mismo ragdoll.

#include "engine/physics/world/PhysicsWorld_Internal.h"

#include "core/Log.h"
#include "engine/physics/ragdoll/RagdollLayout.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>  // JPH::Skeleton para Stabilize()

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace Mood {

namespace {

// Helpers de conversion mat4 <-> JPH::RVec3/Quat.
//
// Asumimos `worldMatrix` con basis ortonormal (sin scale). Las rotation
// derivada via glm::quat_cast funciona porque RagdollLayout no inyecta
// scale en partWorldTransforms — el caller (RagdollSystem) las construye
// desde bind globals * entityWorldMatrix con scale ya neutralizado.
JPH::Vec3 vec3ToJph(const glm::vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}
JPH::Quat matToJphQuat(const glm::mat4& m) {
    const glm::quat q = glm::quat_cast(m);
    return JPH::Quat(q.x, q.y, q.z, q.w);
}
glm::vec3 jphToVec3(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}
glm::mat4 jphToMat4(const JPH::RMat44& m) {
    glm::mat4 out;
    // JPH usa column-major matching glm. Copia cell-by-cell defensiva.
    for (int c = 0; c < 4; ++c) {
        const JPH::Vec4 col = m.GetColumn4(c);
        out[c] = glm::vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
    }
    return out;
}

// Calcula el twist axis local del bone: direccion hacia el child principal
// (donde el ragdoll lo conecta), expresada en local de la rotacion del
// transform del bone. Para Hinge usamos el axis perpendicular a la
// gravedad (eje Z local). Para SwingTwist usamos +Y como twist (axis del
// hueso) y +X como plane.
//
// Defaults sensatos: el caller raramente personaliza esto.
struct BoneAxes {
    JPH::Vec3 twistAxis = JPH::Vec3::sAxisY();
    JPH::Vec3 planeAxis = JPH::Vec3::sAxisX();
    JPH::Vec3 hingeAxis = JPH::Vec3::sAxisZ();
};
BoneAxes defaultBoneAxes() { return BoneAxes{}; }

} // anonymous

u32 PhysicsWorld::createRagdoll(const ragdoll::RagdollLayout& layout,
                                  const std::vector<glm::mat4>& partWorldTransforms,
                                  bool useGravity) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (layout.bones.empty()) return 0;
    if (partWorldTransforms.size() != layout.bones.size()) {
        Log::physics()->error(
            "PhysicsWorld::createRagdoll: partWorldTransforms.size() ({}) != "
            "layout.bones.size() ({})",
            partWorldTransforms.size(), layout.bones.size());
        return 0;
    }

    JPH::Ref<JPH::RagdollSettings> settings = new JPH::RagdollSettings();
    settings->mParts.resize(layout.bones.size());

    // Jolt requiere un `Skeleton` interno con joints + parent index para
    // que `Stabilize()` y la creacion del Ragdoll sepan la jerarquia.
    // Lo armamos espejando el orden de mParts.
    JPH::Ref<JPH::Skeleton> jphSkeleton = new JPH::Skeleton();
    jphSkeleton->GetJoints().reserve(layout.bones.size());
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const auto& b = layout.bones[i];
        const std::string parentName = (b.parentRagdollIndex >= 0)
            ? std::string(layout.bones[b.parentRagdollIndex].boneName)
            : std::string{};
        jphSkeleton->GetJoints().emplace_back(
            std::string(b.boneName), parentName, b.parentRagdollIndex);
    }
    settings->mSkeleton = jphSkeleton;

    // 1) Body per part. Capsule shape orientada al twist axis del bone
    //    (default +Y). El caller pasa la rotacion bone-world via
    //    partWorldTransforms[i], que ya respeta la orientacion del bone.
    const auto motionMoving = physics_internal::toJPHLayer(ObjectLayer::Moving);
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const auto& b = layout.bones[i];
        auto& part = settings->mParts[i];

        // CapsuleShape: el "alto" en Jolt es 2*halfHeightOfCylinder (sin
        // contar los hemisferios). Nuestro layout guarda halfHeight ya
        // descontando el radio.
        const f32 capHalfH = std::max(b.capsuleHalfHeight, 0.01f);
        const f32 capRad   = std::max(b.capsuleRadius, 0.01f);
        JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(capHalfH, capRad);
        part.SetShape(capsule);

        const glm::mat4& wt = partWorldTransforms[i];
        const glm::vec3 pos(wt[3]);
        part.mPosition       = JPH::RVec3(pos.x, pos.y, pos.z);
        part.mRotation       = matToJphQuat(wt);
        part.mMotionType     = JPH::EMotionType::Dynamic;
        part.mObjectLayer    = motionMoving;
        part.mGravityFactor  = useGravity ? 1.0f : 0.0f;
        part.mAllowSleeping  = true;
        // Mass override (Jolt computa inercia desde el shape; el override
        // de mass mantiene la inercia ratio pero escala el peso).
        part.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        part.mMassPropertiesOverride.mMass = std::max(b.mass, 0.1f);
    }

    // 2) Constraints. Para cada bone con parent ragdolleado, generamos un
    //    Hinge o SwingTwist segun el kind del layout. Pivot: en world
    //    coords, lo ponemos en la POSICION del child body (= articulacion).
    //    Asi el codo pivota en el codo, no en el medio del hueso.
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const auto& b = layout.bones[i];
        if (b.parentRagdollIndex < 0) continue;  // root, sin constraint
        if (b.constraint == ragdoll::ConstraintKind::None) continue;

        const glm::vec3 pivotWorld(partWorldTransforms[i][3]);
        const BoneAxes axes = defaultBoneAxes();

        if (b.constraint == ragdoll::ConstraintKind::Hinge) {
            auto* hs = new JPH::HingeConstraintSettings();
            hs->mSpace      = JPH::EConstraintSpace::WorldSpace;
            hs->mPoint1     = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
            hs->mPoint2     = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
            hs->mHingeAxis1 = axes.hingeAxis;
            hs->mHingeAxis2 = axes.hingeAxis;
            // Normal axis perpendicular al hinge para medir angulo.
            hs->mNormalAxis1 = JPH::Vec3::sAxisX();
            hs->mNormalAxis2 = JPH::Vec3::sAxisX();
            hs->mLimitsMin  = glm::radians(b.limits.hingeMinDeg);
            hs->mLimitsMax  = glm::radians(b.limits.hingeMaxDeg);
            settings->mParts[i].mToParent = hs;
        } else /* SwingTwist */ {
            auto* sts = new JPH::SwingTwistConstraintSettings();
            sts->mSpace = JPH::EConstraintSpace::WorldSpace;
            sts->mPosition1 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
            sts->mPosition2 = JPH::RVec3(pivotWorld.x, pivotWorld.y, pivotWorld.z);
            sts->mTwistAxis1 = axes.twistAxis;
            sts->mTwistAxis2 = axes.twistAxis;
            sts->mPlaneAxis1 = axes.planeAxis;
            sts->mPlaneAxis2 = axes.planeAxis;
            sts->mNormalHalfConeAngle = glm::radians(b.limits.swingYDeg);
            sts->mPlaneHalfConeAngle  = glm::radians(b.limits.swingZDeg);
            sts->mTwistMinAngle = glm::radians(b.limits.twistMinDeg);
            sts->mTwistMaxAngle = glm::radians(b.limits.twistMaxDeg);
            settings->mParts[i].mToParent = sts;
        }
    }

    // NOTA: Jolt infiere los parent indices del orden de `mParts`. Nuestro
    // `buildMixamoLayout` garantiza orden topologico (Hips antes que Spine,
    // etc.) — los constraints conectan via el orden de Parts + el campo
    // `mToParent` del child apuntando al parent.

    // 3) Stabilize (Jolt valida + ajusta). Si falla, abortamos.
    if (!settings->Stabilize()) {
        Log::physics()->error(
            "PhysicsWorld::createRagdoll: Stabilize() fallo. Revisa los "
            "indices de parent en el layout.");
        return 0;
    }

    // 4) Group filter para deshabilitar self-collision entre partes.
    settings->DisableParentChildCollisions();

    // 5) Create + add al physics system. GroupID unico por ragdoll para
    //    que multiples ragdolls no se choquen entre si por accidente.
    const JPH::CollisionGroup::GroupID groupId =
        static_cast<JPH::CollisionGroup::GroupID>(m_impl->nextRagdollId);
    JPH::Ref<JPH::Ragdoll> rag = settings->CreateRagdoll(
        groupId, /*userData*/ 0, m_impl->physicsSystem.get());
    if (rag == nullptr) {
        Log::physics()->error(
            "PhysicsWorld::createRagdoll: CreateRagdoll devolvio nullptr "
            "(probable out of bodies — el motor tiene tope global).");
        return 0;
    }
    rag->AddToPhysicsSystem(JPH::EActivation::Activate);

    const u32 handle = m_impl->nextRagdollId++;
    m_impl->ragdolls.emplace(handle, rag);
    Log::physics()->info(
        "PhysicsWorld::createRagdoll: id={} parts={} gravity={}",
        handle, layout.bones.size(), useGravity);
    return handle;
}

void PhysicsWorld::destroyRagdoll(u32 ragdollId) {
    if (!m_impl || ragdollId == 0) return;
    auto it = m_impl->ragdolls.find(ragdollId);
    if (it == m_impl->ragdolls.end()) return;
    if (it->second != nullptr) {
        it->second->RemoveFromPhysicsSystem();
    }
    m_impl->ragdolls.erase(it);
}

bool PhysicsWorld::readRagdollPose(u32 ragdollId,
                                     std::vector<glm::mat4>& outTransforms) const {
    if (!m_impl || ragdollId == 0) return false;
    auto it = m_impl->ragdolls.find(ragdollId);
    if (it == m_impl->ragdolls.end() || it->second == nullptr) return false;

    const JPH::Array<JPH::BodyID>& ids = it->second->GetBodyIDs();
    outTransforms.resize(ids.size());

    // Lock todos los bodies del ragdoll a la vez para leer transforms.
    // Read-only (no mutamos).
    const JPH::BodyLockInterface& bli = m_impl->physicsSystem->GetBodyLockInterface();
    for (usize i = 0; i < ids.size(); ++i) {
        JPH::BodyLockRead lock(bli, ids[i]);
        if (!lock.Succeeded()) {
            outTransforms[i] = glm::mat4(1.0f);
            continue;
        }
        const JPH::Body& body = lock.GetBody();
        outTransforms[i] = jphToMat4(body.GetWorldTransform());
    }
    return true;
}

void PhysicsWorld::applyRagdollImpulse(u32 ragdollId, int partIndex,
                                         const glm::vec3& impulseWorld) {
    if (!m_impl || ragdollId == 0 || partIndex < 0) return;
    auto it = m_impl->ragdolls.find(ragdollId);
    if (it == m_impl->ragdolls.end() || it->second == nullptr) return;
    const JPH::Array<JPH::BodyID>& ids = it->second->GetBodyIDs();
    if (partIndex >= static_cast<int>(ids.size())) return;

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.AddImpulse(ids[partIndex], vec3ToJph(impulseWorld));
}

u32 PhysicsWorld::ragdollCount() const {
    if (!m_impl) return 0;
    return static_cast<u32>(m_impl->ragdolls.size());
}

} // namespace Mood
