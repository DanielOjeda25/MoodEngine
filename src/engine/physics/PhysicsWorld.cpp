#include "engine/physics/PhysicsWorld.h"

#include "core/Log.h"

// Jolt headers — incluir en un orden especifico. El `Jolt.h` maestro define
// macros que el resto necesita.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

// Jolt requiere un trace callback global. Lo atamos al canal `physics` de
// spdlog. Variadic via sprintf porque Jolt pasa un format string estilo C.
static void joltTraceCallback(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Mood::Log::physics()->trace("[Jolt] {}", buf);
}

#ifdef JPH_ENABLE_ASSERTS
static bool joltAssertFailed(const char* expression, const char* message,
                              const char* file, JPH::uint line) {
    Mood::Log::physics()->error("[Jolt assert] {}:{} {} ({})",
                                 file, line, expression,
                                 message != nullptr ? message : "");
    return true; // break into debugger
}
#endif

namespace Mood {

namespace {

constexpr u32 k_maxBodies            = 1024;
constexpr u32 k_numBodyMutexes       = 0;    // default
constexpr u32 k_maxBodyPairs         = 1024;
constexpr u32 k_maxContactConstraints = 1024;
constexpr u32 k_tempAllocatorMB      = 10;

// Broadphase layer ids (propios de Jolt, 8-bit).
namespace BPLayer {
    constexpr JPH::BroadPhaseLayer NonMoving(0);
    constexpr JPH::BroadPhaseLayer Moving(1);
    constexpr u32 Count = 2;
}

// ObjectLayer ids — compatibles con nuestro enum `Mood::ObjectLayer`.
constexpr JPH::ObjectLayer toJPHLayer(ObjectLayer l) {
    return static_cast<JPH::ObjectLayer>(l);
}

// Mapa ObjectLayer -> BroadPhaseLayer. Clase requerida por Jolt.
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
        // Moving choca con cualquiera; Static solo con Moving (no consigo mismo).
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
        // Nada colisiona Static-Static.
        if (oa == ObjectLayer::Static && ob == ObjectLayer::Static) return false;
        return true;
    }
};

} // namespace

struct PhysicsWorld::Impl {
    BPLayerInterface       bpLayerInterface{};
    ObjectVsBPFilter       objectVsBpFilter{};
    ObjectLayerPairFilter  objectLayerPairFilter{};
    std::unique_ptr<JPH::TempAllocator> tempAllocator;
    std::unique_ptr<JPH::JobSystem> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;
};

PhysicsWorld::PhysicsWorld() : m_impl(std::make_unique<Impl>()) {
    // Jolt init global: 1ra corrida crea la Factory y registra los tipos.
    // Si ya hay una Factory (ej. un test corriendo antes), el `if` evita
    // el double-init — Jolt NO es idempotente en esto.
    if (JPH::Factory::sInstance == nullptr) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = joltTraceCallback;
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = joltAssertFailed;
#endif
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }

    m_impl->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(
        k_tempAllocatorMB * 1024 * 1024);
    // 0 workers = usa todos los hilos disponibles menos uno (main).
    m_impl->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        /*numThreads*/ -1);

    m_impl->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_impl->physicsSystem->Init(
        k_maxBodies,
        k_numBodyMutexes,
        k_maxBodyPairs,
        k_maxContactConstraints,
        m_impl->bpLayerInterface,
        m_impl->objectVsBpFilter,
        m_impl->objectLayerPairFilter);

    // Gravedad default SI: 9.81 m/s^2 hacia -Y.
    m_impl->physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    Log::physics()->info(
        "Jolt inicializado (max_bodies={}, max_pairs={}, gravity=-9.81)",
        k_maxBodies, k_maxBodyPairs);
}

PhysicsWorld::~PhysicsWorld() {
    if (!m_impl) return;
    m_impl->physicsSystem.reset();
    m_impl->jobSystem.reset();
    m_impl->tempAllocator.reset();
    // No destruimos Factory aqui — si otro PhysicsWorld se instancia en la
    // misma proceso (tests multiples), Jolt explota si la Factory falta.
    // Leak benigno: el OS la limpia al exit. La alternativa seria
    // refcounting global, overengineering.
}

void PhysicsWorld::step(f32 dt, int collisionSteps) {
    if (!m_impl || !m_impl->physicsSystem) return;
    m_impl->physicsSystem->Update(dt, collisionSteps,
                                    m_impl->tempAllocator.get(),
                                    m_impl->jobSystem.get());
}

namespace {

JPH::RefConst<JPH::Shape> createJPHShape(CollisionShape shape,
                                          const glm::vec3& halfExtents) {
    switch (shape) {
        case CollisionShape::Box:
            return new JPH::BoxShape(
                JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
        case CollisionShape::Sphere:
            return new JPH::SphereShape(halfExtents.x);
        case CollisionShape::Capsule:
            return new JPH::CapsuleShape(halfExtents.x, halfExtents.y);
    }
    return nullptr;
}

JPH::EMotionType toJPHMotion(BodyType t) {
    switch (t) {
        case BodyType::Static:    return JPH::EMotionType::Static;
        case BodyType::Kinematic: return JPH::EMotionType::Kinematic;
        case BodyType::Dynamic:   return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Static;
}

} // namespace

u32 PhysicsWorld::createBody(const glm::vec3& position,
                              CollisionShape shape,
                              const glm::vec3& halfExtents,
                              BodyType type,
                              f32 mass) {
    if (!m_impl) return 0;

    auto jphShape = createJPHShape(shape, halfExtents);
    if (jphShape == nullptr) {
        Log::physics()->warn("createBody: shape no soportado");
        return 0;
    }

    const ObjectLayer layer = (type == BodyType::Static)
        ? ObjectLayer::Static : ObjectLayer::Moving;

    JPH::BodyCreationSettings settings(
        jphShape,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        toJPHMotion(type),
        toJPHLayer(layer));

    if (type == BodyType::Dynamic) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    JPH::BodyID id = bi.CreateAndAddBody(settings,
        type == BodyType::Static
            ? JPH::EActivation::DontActivate
            : JPH::EActivation::Activate);
    if (id.IsInvalid()) {
        Log::physics()->warn("createBody: Jolt devolvio BodyID invalido");
        return 0;
    }
    return id.GetIndexAndSequenceNumber();
}

void PhysicsWorld::destroyBody(u32 bodyId) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.RemoveBody(id);
    bi.DestroyBody(id);
}

glm::vec3 PhysicsWorld::bodyPosition(u32 bodyId) const {
    if (!m_impl || bodyId == 0) return glm::vec3(0.0f);
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    JPH::RVec3 pos = bi.GetPosition(id);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

void PhysicsWorld::setBodyPosition(u32 bodyId, const glm::vec3& position) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.SetPosition(id, JPH::RVec3(position.x, position.y, position.z),
                    JPH::EActivation::Activate);
}

void PhysicsWorld::addForce(u32 bodyId, const glm::vec3& force) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.AddForce(id, JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsWorld::addImpulse(u32 bodyId, const glm::vec3& impulse) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

u32 PhysicsWorld::bodyCount() const {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    return m_impl->physicsSystem->GetNumBodies();
}

} // namespace Mood
