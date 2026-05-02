#include "engine/physics/PhysicsWorld.h"

#include "core/Log.h"

#include <glm/vec4.hpp>  // Hito 41 C: setBodyPositionRot quat

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
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>

#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

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

struct CharacterEntry {
    JPH::Ref<JPH::CharacterVirtual> character;
    glm::vec3 desiredVelocity{0.0f};
};

struct PhysicsWorld::Impl {
    BPLayerInterface       bpLayerInterface{};
    ObjectVsBPFilter       objectVsBpFilter{};
    ObjectLayerPairFilter  objectLayerPairFilter{};
    std::unique_ptr<JPH::TempAllocator> tempAllocator;
    std::unique_ptr<JPH::JobSystem> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    // Hito 30: characters CharacterVirtual indexados por handle estable.
    std::unordered_map<u32, CharacterEntry> characters;
    u32 nextCharId = 1;
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

    // Hito 30: characters van ANTES de Update — la nueva pose entra a
    // la simulacion fisica como Kinematic, asi los rigidbodies que el
    // character toca reciben colision este frame.
    //
    // Composicion de velocidad: el caller setea desired = (vx, vy_impulse, vz)
    // con desired.y NORMALMENTE 0 (excepto en frames de salto). Aca:
    //   - X/Z = directo del desired (input controla horizontal).
    //   - Y   = acumula gravedad si NO esta OnGround; si si, se resetea
    //           a max(0, current.y) para no penetrar el piso. Sumamos
    //           desired.y como impulse instantaneo (jump).
    if (!m_impl->characters.empty()) {
        const JPH::Vec3 gravity = m_impl->physicsSystem->GetGravity();
        const JPH::DefaultBroadPhaseLayerFilter bpFilter(
            m_impl->objectVsBpFilter, toJPHLayer(ObjectLayer::Moving));
        const JPH::DefaultObjectLayerFilter objFilter(
            m_impl->objectLayerPairFilter, toJPHLayer(ObjectLayer::Moving));
        const JPH::BodyFilter   bodyFilter;
        const JPH::ShapeFilter  shapeFilter;
        for (auto& [id, entry] : m_impl->characters) {
            if (entry.character == nullptr) continue;

            const bool onGround = entry.character->GetGroundState()
                == JPH::CharacterVirtual::EGroundState::OnGround;
            const JPH::Vec3 currentVel = entry.character->GetLinearVelocity();
            float vy = currentVel.GetY();
            if (onGround) {
                vy = (vy < 0.0f) ? 0.0f : vy;  // no atravesar el piso
            } else {
                vy += gravity.GetY() * dt;     // gravedad acumula en aire
            }
            // Sumar el impulse vertical del caller (jump) sobre la base.
            vy += entry.desiredVelocity.y;

            entry.character->SetLinearVelocity(JPH::Vec3(
                entry.desiredVelocity.x,
                vy,
                entry.desiredVelocity.z));

            // ExtendedUpdate con gravity=0: ya la integramos manualmente.
            JPH::CharacterVirtual::ExtendedUpdateSettings extSettings{};
            entry.character->ExtendedUpdate(dt, JPH::Vec3::sZero(),
                                              extSettings,
                                              bpFilter, objFilter,
                                              bodyFilter, shapeFilter,
                                              *m_impl->tempAllocator);
        }
    }

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
                              f32 mass,
                              f32 friction,
                              const glm::vec4& rotationQuat) {
    if (!m_impl) return 0;

    auto jphShape = createJPHShape(shape, halfExtents);
    if (jphShape == nullptr) {
        Log::physics()->warn("createBody: shape no soportado");
        return 0;
    }

    const ObjectLayer layer = (type == BodyType::Static)
        ? ObjectLayer::Static : ObjectLayer::Moving;

    // Hito 41 fix-up #2: rotation desde el caller (quat XYZW). Si el
    // quat es zero (caller no lo pasa explicito y olvida default),
    // caemos a identidad para no crashear Jolt con quat normalizado=0.
    JPH::Quat jphQuat(rotationQuat.x, rotationQuat.y, rotationQuat.z,
                       rotationQuat.w);
    if (jphQuat.LengthSq() < 1e-6f) {
        jphQuat = JPH::Quat::sIdentity();
    } else {
        jphQuat = jphQuat.Normalized();
    }

    JPH::BodyCreationSettings settings(
        jphShape,
        JPH::RVec3(position.x, position.y, position.z),
        jphQuat,
        toJPHMotion(type),
        toJPHLayer(layer));

    if (type == BodyType::Dynamic) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }
    // Hito 34 A: friction per-body. Antes (Hito 31 D) era 0.5 hardcoded
    // para evitar el resbaloso default 0.2 de Jolt. Ahora viene del
    // RigidBodyComponent — el caller decide. Aplica a Static + Dynamic
    // (en Static no afecta al body, si al contacto contra otros bodies).
    settings.mFriction = friction;

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

glm::vec3 PhysicsWorld::bodyLinearVelocity(u32 bodyId) const {
    if (!m_impl || bodyId == 0) return glm::vec3(0.0f);
    JPH::BodyID id(bodyId);
    const JPH::BodyInterface& bi =
        m_impl->physicsSystem->GetBodyInterfaceNoLock();
    const JPH::Vec3 v = bi.GetLinearVelocity(id);
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

glm::vec3 PhysicsWorld::bodyAngularVelocity(u32 bodyId) const {
    if (!m_impl || bodyId == 0) return glm::vec3(0.0f);
    JPH::BodyID id(bodyId);
    const JPH::BodyInterface& bi =
        m_impl->physicsSystem->GetBodyInterfaceNoLock();
    const JPH::Vec3 w = bi.GetAngularVelocity(id);
    return glm::vec3(w.GetX(), w.GetY(), w.GetZ());
}

void PhysicsWorld::setBodyLinearVelocity(u32 bodyId, const glm::vec3& v) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.SetLinearVelocity(id, JPH::Vec3(v.x, v.y, v.z));
    bi.ActivateBody(id);
}

void PhysicsWorld::setBodyAngularVelocity(u32 bodyId, const glm::vec3& w) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.SetAngularVelocity(id, JPH::Vec3(w.x, w.y, w.z));
    bi.ActivateBody(id);
}

glm::vec3 PhysicsWorld::bodyPositionRot(u32 bodyId, glm::vec4& outQuatXYZW) const {
    outQuatXYZW = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    if (!m_impl || bodyId == 0) return glm::vec3(0.0f);
    JPH::BodyID id(bodyId);
    // GetBodyInterfaceNoLock es const + no-locking — apto para const-method.
    const JPH::BodyInterface& bi =
        m_impl->physicsSystem->GetBodyInterfaceNoLock();
    JPH::RVec3 pos;
    JPH::Quat q;
    bi.GetPositionAndRotation(id, pos, q);
    outQuatXYZW = glm::vec4(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

void PhysicsWorld::setBodyPositionRot(u32 bodyId, const glm::vec3& pos,
                                        const glm::vec4& quatXYZW) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.SetPositionAndRotation(id,
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat(quatXYZW.x, quatXYZW.y, quatXYZW.z, quatXYZW.w),
        JPH::EActivation::Activate);
}

void PhysicsWorld::addForce(u32 bodyId, const glm::vec3& force) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.AddForce(id, JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsWorld::setBodyFriction(u32 bodyId, f32 friction) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.SetFriction(id, friction);
    // Reactivar el body para que los contactos vigentes recalculen
    // friccion en el proximo step. Sin esto un dynamic en reposo
    // mantiene el coef anterior hasta que algo lo despierte.
    bi.ActivateBody(id);
}

void PhysicsWorld::setBodyMass(u32 bodyId, f32 mass) {
    // Hito 40 D: stub. La API limpia de Jolt para cambiar mass de un
    // body activo en runtime requiere `Body::SetMassProperties` con
    // shape recomputation, que no esta expuesta facilmente desde
    // `BodyInterface` en la version incluida. Implementaciones via
    // `BodyLockWrite + MotionProperties::SetInverseMass` crashean con
    // SEH en al menos algunas versiones. Aceptado: el caller debe usar
    // `setBodyHalfExtents` (que recrea el shape con `inUpdateMassProperties=true`)
    // si necesita cambio de mass por ahora, o re-Play del editor.
    // Para Fase 2: revivir esta API con la version de Jolt que la
    // soporte sin crash.
    (void)bodyId;
    (void)mass;
    if (m_impl != nullptr && bodyId != 0 && mass > 0.0f) {
        Log::physics()->debug(
            "setBodyMass({}, {}) es stub en v1.0 — usar setBodyHalfExtents "
            "si requiere cambio de mass. Pendiente Fase 2.",
            bodyId, mass);
    }
}

void PhysicsWorld::setBodyHalfExtents(u32 bodyId, CollisionShape shape,
                                        const glm::vec3& halfExtents) {
    if (!m_impl || bodyId == 0) return;
    JPH::BodyID id(bodyId);
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    auto newShape = createJPHShape(shape, halfExtents);
    if (newShape == nullptr) return;
    // SetShape: recrea collider + invalida contactos. El body keeps
    // su pose (position + rotation + velocity). El bool inUpdateMass
    // recalcula inercia con el nuevo shape (true).
    bi.SetShape(id, newShape, /*inUpdateMassProperties=*/true,
                JPH::EActivation::Activate);
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

// --- Hito 30: Character Controller ---

u32 PhysicsWorld::createCharacter(const glm::vec3& initialPos,
                                    f32 cylinderHalfHeight,
                                    f32 radius) {
    if (!m_impl || !m_impl->physicsSystem) return 0;

    JPH::RefConst<JPH::Shape> capsule =
        new JPH::CapsuleShape(cylinderHalfHeight, radius);

    JPH::Ref<JPH::CharacterVirtualSettings> settings =
        new JPH::CharacterVirtualSettings();
    settings->mShape = capsule;
    // Plano de soporte: aceptamos contactos cuyo normal Y >= cos(50°).
    // Da estabilidad en escalones sin que el char se "pegue" a paredes.
    settings->mSupportingVolume = JPH::Plane(
        JPH::Vec3::sAxisY(), -radius);
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);

    JPH::Ref<JPH::CharacterVirtual> ch = new JPH::CharacterVirtual(
        settings,
        JPH::RVec3(initialPos.x, initialPos.y, initialPos.z),
        JPH::Quat::sIdentity(),
        /*userData*/ 0,
        m_impl->physicsSystem.get());

    const u32 id = m_impl->nextCharId++;
    m_impl->characters[id] = CharacterEntry{ch, glm::vec3(0.0f)};
    return id;
}

void PhysicsWorld::destroyCharacter(u32 charId) {
    if (!m_impl || charId == 0) return;
    m_impl->characters.erase(charId);
}

void PhysicsWorld::setCharacterMovement(u32 charId,
                                         const glm::vec3& desiredVelocity) {
    if (!m_impl) return;
    auto it = m_impl->characters.find(charId);
    if (it == m_impl->characters.end()) return;
    it->second.desiredVelocity = desiredVelocity;
}

glm::vec3 PhysicsWorld::characterPosition(u32 charId) const {
    if (!m_impl) return glm::vec3(0.0f);
    auto it = m_impl->characters.find(charId);
    if (it == m_impl->characters.end() || it->second.character == nullptr) {
        return glm::vec3(0.0f);
    }
    JPH::RVec3 p = it->second.character->GetPosition();
    return glm::vec3(p.GetX(), p.GetY(), p.GetZ());
}

void PhysicsWorld::setCharacterPosition(u32 charId, const glm::vec3& position) {
    if (!m_impl) return;
    auto it = m_impl->characters.find(charId);
    if (it == m_impl->characters.end() || it->second.character == nullptr) return;
    it->second.character->SetPosition(JPH::RVec3(position.x, position.y, position.z));
    it->second.character->SetLinearVelocity(JPH::Vec3::sZero());
    it->second.desiredVelocity = glm::vec3(0.0f);
}

bool PhysicsWorld::isCharacterOnGround(u32 charId) const {
    if (!m_impl) return false;
    auto it = m_impl->characters.find(charId);
    if (it == m_impl->characters.end() || it->second.character == nullptr) {
        return false;
    }
    return it->second.character->GetGroundState()
           == JPH::CharacterVirtual::EGroundState::OnGround;
}

u32 PhysicsWorld::characterCount() const {
    if (!m_impl) return 0;
    return static_cast<u32>(m_impl->characters.size());
}

bool PhysicsWorld::setCharacterShape(u32 charId,
                                      f32 cylinderHalfHeight,
                                      f32 radius) {
    if (!m_impl) return false;
    auto it = m_impl->characters.find(charId);
    if (it == m_impl->characters.end() || it->second.character == nullptr) {
        return false;
    }
    JPH::RefConst<JPH::Shape> newShape =
        new JPH::CapsuleShape(cylinderHalfHeight, radius);

    // Filtros para el penetration check: Jolt nos dice si el shape
    // nuevo se solapa con la geometria; si si, rechazamos el cambio.
    const JPH::DefaultBroadPhaseLayerFilter bpFilter(
        m_impl->objectVsBpFilter, toJPHLayer(ObjectLayer::Moving));
    const JPH::DefaultObjectLayerFilter objFilter(
        m_impl->objectLayerPairFilter, toJPHLayer(ObjectLayer::Moving));
    const JPH::BodyFilter   bodyFilter;
    const JPH::ShapeFilter  shapeFilter;

    return it->second.character->SetShape(
        newShape,
        /*maxPenetrationDepth*/ 0.01f,
        bpFilter, objFilter, bodyFilter, shapeFilter,
        *m_impl->tempAllocator);
}

// Hito 34 B: filtro que descarta un body especifico durante un cast.
// Heredado de JPH::BodyFilter — el default `ShouldCollide` devuelve true,
// nosotros lo overrideamos para excluir `m_ignored`. Acotado a archivo.
// Si `m_ignoredRaw == 0`, no excluimos nada (Hito 39 C combo con layer
// filter — el caller puede querer solo layer sin ignored body).
namespace {
class IgnoreOneBodyFilter : public JPH::BodyFilter {
public:
    explicit IgnoreOneBodyFilter(u32 ignoredRaw) : m_ignoredRaw(ignoredRaw) {}
    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        if (m_ignoredRaw == 0u) return true;
        return inBodyID.GetIndexAndSequenceNumber() != m_ignoredRaw;
    }
private:
    u32 m_ignoredRaw;
};

// Hito 39 C: filtro por layer mask aplicado al ObjectLayer del body.
// Bit 0 = ObjectLayer::Static, bit 1 = ObjectLayer::Moving. Permite
// "solo paredes" (1u), "solo dinamicos" (2u), o ambos (3u/default).
class LayerMaskFilter : public JPH::ObjectLayerFilter {
public:
    explicit LayerMaskFilter(u32 mask) : m_mask(mask) {}
    bool ShouldCollide(JPH::ObjectLayer layer) const override {
        const u32 bit = (1u << static_cast<u32>(layer));
        return (m_mask & bit) != 0u;
    }
private:
    u32 m_mask;
};
} // namespace

PhysicsWorld::RaycastHit PhysicsWorld::raycast(const glm::vec3& origin,
                                                const glm::vec3& direction,
                                                f32 maxDistance,
                                                u32 ignoredBodyId,
                                                u32 layerMask) const {
    RaycastHit out{};
    if (!m_impl || maxDistance <= 0.0f) return out;

    // Direction normalizada * maxDistance: convencion Jolt — la magnitud
    // del segundo arg es el "alcance" del rayo. Si el caller pasa una
    // direccion no-unitaria, normalizamos defensivamente para que
    // maxDistance siga siendo la distancia maxima en metros.
    const JPH::Vec3 jphDir(direction.x, direction.y, direction.z);
    const f32 lenSq = jphDir.LengthSq();
    if (lenSq < 1e-10f) return out;  // direccion nula -> miss
    const JPH::Vec3 unit = jphDir / std::sqrt(lenSq);
    const JPH::Vec3 scaled = unit * maxDistance;

    const JPH::RRayCast ray(
        JPH::RVec3(origin.x, origin.y, origin.z),
        scaled);
    JPH::RayCastResult result;

    // Hito 34 B + 39 C: filtros opcionales. Default args (0 / 0xFFFFFFFF)
    // arman filtros que aceptan todo — equivalente a no pasar nada.
    // Cuando alguno es restrictivo, instanciamos los filtros custom.
    bool hit;
    const bool useFilters = (ignoredBodyId != 0) || (layerMask != 0xFFFFFFFFu);
    if (useFilters) {
        const IgnoreOneBodyFilter bodyFilter{ignoredBodyId};
        const LayerMaskFilter     layerFilter{layerMask};
        hit = m_impl->physicsSystem->GetNarrowPhaseQuery().CastRay(
            ray, result, {}, layerFilter, bodyFilter);
    } else {
        hit = m_impl->physicsSystem->GetNarrowPhaseQuery().CastRay(ray, result);
    }
    if (!hit) return out;

    out.hit = true;
    out.distance = maxDistance * result.mFraction;
    const JPH::Vec3 hitPoint = ray.mOrigin + scaled * result.mFraction;
    out.point = glm::vec3(hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ());
    out.bodyId = result.mBodyID.GetIndexAndSequenceNumber();

    // Normal: Jolt la calcula a partir del body + sub-shape + punto.
    // Lock para leer-only — barato comparado al CastRay (acabamos de hacer).
    const JPH::BodyLockRead lock(
        m_impl->physicsSystem->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        const JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(
            result.mSubShapeID2, hitPoint);
        out.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
    }

    return out;
}

} // namespace Mood
