#pragma once

// Header privado de PhysicsWorld. Define el PIMPL `PhysicsWorld::Impl` +
// las clases de configuracion Jolt (BPLayer/ObjectVs/PairFilter) +
// `CharacterEntry`. Incluido SOLO por los archivos parciales del modulo
// (`PhysicsWorld.cpp`, `PhysicsWorld_Constraints.cpp`, `PhysicsWorld_Ragdoll.cpp`).
//
// F2H66 Bloque 0: split del `PhysicsWorld.cpp` que excedio el hard cap de
// 800 LOC tras los joints de F2H65. Antes la `Impl` vivia inline en el
// .cpp, ahora se comparte entre los partials. Mismo patron PIMPL-split
// que usan multiples motores cuando un modulo de back-end crece.

#include "engine/physics/world/PhysicsWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>  // F2H66
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>  // F2H67
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>  // F2H67

#include <glm/vec3.hpp>

#include <memory>
#include <unordered_map>

namespace Mood::physics_internal {

// --- Broadphase layer ids (privados al modulo) ---
namespace BPLayer {
    constexpr JPH::BroadPhaseLayer NonMoving{0};
    constexpr JPH::BroadPhaseLayer Moving{1};
    constexpr JPH::uint Count = 2;
}

constexpr JPH::ObjectLayer toJPHLayer(ObjectLayer l) {
    return static_cast<JPH::ObjectLayer>(l);
}

// Mapa ObjectLayer -> BroadPhaseLayer.
class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterface() {
        m_objectToBroadphase[static_cast<JPH::ObjectLayer>(ObjectLayer::Static)] = BPLayer::NonMoving;
        m_objectToBroadphase[static_cast<JPH::ObjectLayer>(ObjectLayer::Moving)] = BPLayer::Moving;
    }

    JPH::uint GetNumBroadPhaseLayers() const override { return BPLayer::Count; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return m_objectToBroadphase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch (layer.GetValue()) {
            case 0: return "NonMoving";
            case 1: return "Moving";
            default: return "?";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_objectToBroadphase[static_cast<JPH::uint>(ObjectLayer::Count)];
};

class ObjectVsBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        const auto ol = static_cast<ObjectLayer>(layer);
        if (ol == ObjectLayer::Moving) return true;
        if (ol == ObjectLayer::Static) return bp == BPLayer::Moving;
        return false;
    }
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        const auto oa = static_cast<ObjectLayer>(a);
        const auto ob = static_cast<ObjectLayer>(b);
        if (oa == ObjectLayer::Static && ob == ObjectLayer::Static) return false;
        return true;
    }
};

struct CharacterEntry {
    JPH::Ref<JPH::CharacterVirtual> character;
    glm::vec3 desiredVelocity{0.0f};
};

} // namespace Mood::physics_internal

namespace Mood {

struct PhysicsWorld::Impl {
    physics_internal::BPLayerInterface       bpLayerInterface{};
    physics_internal::ObjectVsBPFilter       objectVsBpFilter{};
    physics_internal::ObjectLayerPairFilter  objectLayerPairFilter{};
    std::unique_ptr<JPH::TempAllocator> tempAllocator;
    std::unique_ptr<JPH::JobSystem> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    // Hito 30: characters CharacterVirtual indexados por handle estable.
    std::unordered_map<u32, physics_internal::CharacterEntry> characters;
    u32 nextCharId = 1;

    // F2H65: constraints (joints) indexados por handle estable.
    // JPH::Ref<JPH::Constraint> mantiene el constraint vivo mientras este
    // map lo tiene -- Jolt agrega su propia referencia al physicsSystem via
    // AddConstraint, asi que el Ref aqui es por simetria al lifecycle.
    std::unordered_map<u32, JPH::Ref<JPH::Constraint>> constraints;
    u32 nextConstraintId = 1;

    // F2H66: ragdolls indexados por handle. Mismo patron de JPH::Ref que los
    // constraints. Cada Ragdoll posee N bodies + (N-1) constraints internos.
    std::unordered_map<u32, JPH::Ref<JPH::Ragdoll>> ragdolls;
    u32 nextRagdollId = 1;

    // F2H67: vehicles indexados por handle. Cada entry mantiene la
    // VehicleConstraint (que es JPH::Ref-counted) + el chassis BodyID (que
    // NO es JPH::Ref — el lifecycle del body lo controlamos en createVehicle/
    // destroyVehicle como con cualquier otro body). El collision tester
    // debe sobrevivir mientras el constraint exista porque el constraint
    // tiene un puntero raw a el.
    struct VehicleEntry {
        JPH::Ref<JPH::VehicleConstraint> constraint;
        JPH::Ref<JPH::VehicleCollisionTester> collisionTester;
        JPH::BodyID chassisBodyId;
    };
    std::unordered_map<u32, VehicleEntry> vehicles;
    u32 nextVehicleId = 1;
};

} // namespace Mood
