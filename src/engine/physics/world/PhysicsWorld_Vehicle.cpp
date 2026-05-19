// F2H67 Bloque C: wrapper sobre `JPH::VehicleConstraint` +
// `WheeledVehicleController`. Convierte un `vehicle::VehicleConfig` puro
// en un vehiculo "vivo" en el physics system: 1 chassis body (Box
// Dynamic) + N=4 wheels gestionadas por el VehicleConstraint (sin bodies
// propios, son raycasts) + 1 controller con engine/transmission/diffs.
//
// Conexiones internas con el resto del modulo:
//   - chassisBodyId vive en `m_impl->vehicles[id].chassisBodyId`. Cleanup
//     en `destroyVehicle` remueve constraint + body atomicamente.
//   - El controller maneja el automatico (auto-shift por RPM). En v1 NO
//     exponemos clutch manual.
//   - VehicleCollisionTester usa el layer Moving del cast (las wheels
//     "ven" tanto Static como Moving del world).
//
// IMPORTANT: el VehicleConstraint se debe registrar como `StepListener`
// del physics system al crearse. Sin eso, las wheels no se actualizan.

#include "engine/physics/world/PhysicsWorld_Internal.h"

#include "core/Log.h"
#include "engine/physics/vehicle/VehicleConfig.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace Mood {

namespace {

JPH::Vec3 vec3ToJph(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
glm::vec3 jphToVec3(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}
JPH::Quat matToJphQuat(const glm::mat4& m) {
    const glm::quat q = glm::quat_cast(m);
    return JPH::Quat(q.x, q.y, q.z, q.w);
}
glm::mat4 jphRMatToMat4(const JPH::RMat44& m) {
    glm::mat4 out;
    for (int c = 0; c < 4; ++c) {
        const JPH::Vec4 col = m.GetColumn4(c);
        out[c] = glm::vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
    }
    return out;
}
glm::mat4 jphMatToMat4(const JPH::Mat44& m) {
    glm::mat4 out;
    for (int c = 0; c < 4; ++c) {
        const JPH::Vec4 col = m.GetColumn4(c);
        out[c] = glm::vec4(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
    }
    return out;
}

} // anonymous

u32 PhysicsWorld::createVehicle(const vehicle::VehicleConfig& cfg,
                                  const glm::mat4& initialWorldTransform) {
    if (!m_impl || !m_impl->physicsSystem) return 0;
    if (!vehicle::isValid(cfg)) {
        Log::physics()->error(
            "PhysicsWorld::createVehicle: VehicleConfig invalido (mass/wheels/"
            "engine fuera de rango). Abortando.");
        return 0;
    }

    // 1) Construir el chassis body. Box con half-extents del config + CoM
    //    offset via OffsetCenterOfMassShape (Jolt expone esto como un
    //    shape wrapper -- el visual mesh queda centrado pero la masa
    //    cuelga abajo, lo que estabiliza el vehiculo).
    JPH::Ref<JPH::Shape> boxShape = new JPH::BoxShape(
        vec3ToJph(cfg.chassisHalfExtents));
    JPH::Ref<JPH::Shape> chassisShape = boxShape;
    if (glm::length(cfg.centerOfMassLocal) > 1e-4f) {
        chassisShape = new JPH::OffsetCenterOfMassShape(
            boxShape, vec3ToJph(cfg.centerOfMassLocal));
    }

    const glm::vec3 initialPos(initialWorldTransform[3]);
    const JPH::Quat initialRot = matToJphQuat(initialWorldTransform);

    JPH::BodyCreationSettings chassisSettings(
        chassisShape, JPH::RVec3(initialPos.x, initialPos.y, initialPos.z),
        initialRot, JPH::EMotionType::Dynamic,
        physics_internal::toJPHLayer(ObjectLayer::Moving));
    chassisSettings.mOverrideMassProperties =
        JPH::EOverrideMassProperties::CalculateInertia;
    chassisSettings.mMassPropertiesOverride.mMass = cfg.chassisMass;
    // Reduce bouncing al apoyar el vehiculo en el suelo (alta penetration
    // recovery por defecto seria contraproducente con suspension blanda).
    chassisSettings.mLinearDamping  = 0.05f;
    chassisSettings.mAngularDamping = 0.05f;
    // El chasis NO duerme mientras el dev no haga release del input. Jolt
    // lo despierta solo, pero pre-arrancar awake reduce un frame de
    // arranque "muerto".
    chassisSettings.mAllowSleeping = true;

    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    JPH::Body* chassisBody = bi.CreateBody(chassisSettings);
    if (chassisBody == nullptr) {
        Log::physics()->error(
            "PhysicsWorld::createVehicle: CreateBody chassis devolvio "
            "nullptr (probable out of bodies).");
        return 0;
    }
    const JPH::BodyID chassisId = chassisBody->GetID();
    bi.AddBody(chassisId, JPH::EActivation::Activate);

    // 2) VehicleConstraintSettings: up/forward + 4 wheels + controller.
    JPH::VehicleConstraintSettings vcSettings;
    vcSettings.mDrawConstraintSize = 0.1f;
    vcSettings.mUp      = JPH::Vec3(0.0f, 1.0f, 0.0f);
    vcSettings.mForward = JPH::Vec3(0.0f, 0.0f, 1.0f);
    // El cono de pitch/roll alrededor del up axis limita cuanto se puede
    // volcar antes de que las wheels dejen de aplicar friccion. SA-style:
    // bastante permisivo pero no full 360.
    vcSettings.mMaxPitchRollAngle = glm::radians(60.0f);

    // 4 wheels: orden FL, FR, RL, RR (matchea WheelIndex del config).
    vcSettings.mWheels.resize(vehicle::WheelCount);
    for (int i = 0; i < vehicle::WheelCount; ++i) {
        const vehicle::WheelConfig& w = cfg.wheels[i];
        JPH::Ref<JPH::WheelSettingsWV> ws = new JPH::WheelSettingsWV();
        ws->mPosition = vec3ToJph(w.attachLocal);
        ws->mSuspensionDirection = JPH::Vec3(0.0f, -1.0f, 0.0f);  // hacia abajo
        ws->mSteeringAxis        = JPH::Vec3(0.0f,  1.0f, 0.0f);  // up
        ws->mWheelUp             = JPH::Vec3(0.0f,  1.0f, 0.0f);
        ws->mWheelForward        = JPH::Vec3(0.0f,  0.0f, 1.0f);
        ws->mRadius = w.radius;
        ws->mWidth  = w.width;
        ws->mSuspensionMinLength = w.suspensionMinLength;
        ws->mSuspensionMaxLength = w.suspensionMaxLength;
        ws->mSuspensionSpring = JPH::SpringSettings(
            JPH::ESpringMode::FrequencyAndDamping,
            w.suspensionFrequency, w.suspensionDamping);
        // Steering: solo wheels con `steered=true`. Las traseras NO giran
        // (max angle 0).
        ws->mMaxSteerAngle = w.steered
            ? glm::radians(cfg.maxSteerAngleDeg)
            : 0.0f;
        // Brake torque: aplica si la rueda no es 'handbraked-only'. En SA
        // todas las wheels tienen brake regular; el handbrake adicional
        // solo lo aplica el WheeledVehicleController a las marcadas como
        // handbraked.
        ws->mMaxBrakeTorque     = cfg.engine.brakeTorque;
        ws->mMaxHandBrakeTorque = w.handbraked ? cfg.engine.handbrakeTorque : 0.0f;
        // Friccion: SA-style alta, baseline + un segundo punto que
        // mantiene la friction casi flat (no decae con slip).
        ws->mLongitudinalFriction.Clear();
        ws->mLongitudinalFriction.AddPoint(0.0f, 0.0f);
        ws->mLongitudinalFriction.AddPoint(0.06f, w.longitudinalFriction);
        ws->mLongitudinalFriction.AddPoint(0.20f, w.longitudinalFriction * 0.9f);
        ws->mLongitudinalFriction.AddPoint(1.0f,  w.longitudinalFriction * 0.7f);
        ws->mLateralFriction.Clear();
        ws->mLateralFriction.AddPoint(0.0f,  0.0f);
        ws->mLateralFriction.AddPoint(3.0f,  w.lateralFriction * 0.7f);
        ws->mLateralFriction.AddPoint(20.0f, w.lateralFriction);
        ws->mLateralFriction.AddPoint(90.0f, w.lateralFriction * 0.5f);
        vcSettings.mWheels[i] = ws;
    }

    // 3) Controller settings: engine + transmission + diferenciales.
    JPH::WheeledVehicleControllerSettings* wvSettings =
        new JPH::WheeledVehicleControllerSettings();
    wvSettings->mEngine.mMaxTorque = cfg.engine.maxTorque;
    wvSettings->mEngine.mMinRPM    = cfg.engine.minRPM;
    wvSettings->mEngine.mMaxRPM    = cfg.engine.maxRPM;
    wvSettings->mEngine.mInertia       = cfg.engine.inertia;
    wvSettings->mEngine.mAngularDamping = cfg.engine.angularDamping;
    // Torque curve estilo SA: pico de torque en ~maxTorqueRPM, decae
    // suavemente. Curva simple flat-ish para arcade feel.
    const f32 peakFrac = std::clamp(
        cfg.engine.maxTorqueRPM / cfg.engine.maxRPM, 0.1f, 0.9f);
    wvSettings->mEngine.mNormalizedTorque.Clear();
    wvSettings->mEngine.mNormalizedTorque.AddPoint(0.0f,     0.6f);
    wvSettings->mEngine.mNormalizedTorque.AddPoint(peakFrac, 1.0f);
    wvSettings->mEngine.mNormalizedTorque.AddPoint(1.0f,     0.7f);

    wvSettings->mTransmission.mMode = JPH::ETransmissionMode::Auto;
    wvSettings->mTransmission.mGearRatios.clear();
    for (f32 r : cfg.engine.gearRatios) {
        wvSettings->mTransmission.mGearRatios.push_back(r);
    }
    wvSettings->mTransmission.mReverseGearRatios.clear();
    for (f32 r : cfg.engine.reverseGearRatios) {
        // Reverse gears en Jolt son negativos (signo invertido respecto al
        // config nuestro que los guarda positivos para legibilidad).
        wvSettings->mTransmission.mReverseGearRatios.push_back(-r);
    }
    wvSettings->mTransmission.mShiftUpRPM   = cfg.engine.maxRPM * 0.75f;
    wvSettings->mTransmission.mShiftDownRPM = cfg.engine.maxRPM * 0.30f;

    // Diferenciales: 2 (front + rear), torque split 50/50 entre ambos
    // (4WD por default; si en el config algun wheel.driven=false, no
    // aplicamos torque ahi). Jolt no expone "esta wheel es driven" como
    // flag — el diferencial define qué wheels recibe el split. Si solo
    // FL+FR estan driven => 1 diferencial front; si solo RL+RR => rear.
    const bool frontDriven = cfg.wheels[vehicle::WheelFL].driven ||
                              cfg.wheels[vehicle::WheelFR].driven;
    const bool rearDriven  = cfg.wheels[vehicle::WheelRL].driven ||
                              cfg.wheels[vehicle::WheelRR].driven;
    const int activeDiffs = (frontDriven ? 1 : 0) + (rearDriven ? 1 : 0);
    if (activeDiffs == 0) {
        Log::physics()->error(
            "PhysicsWorld::createVehicle: ningun wheel marcado como driven. "
            "El vehiculo no recibira torque. Abortando.");
        bi.RemoveBody(chassisId);
        bi.DestroyBody(chassisId);
        delete wvSettings;
        return 0;
    }
    const f32 torquePerDiff = 1.0f / static_cast<f32>(activeDiffs);
    if (frontDriven) {
        JPH::VehicleDifferentialSettings d;
        d.mLeftWheel  = cfg.wheels[vehicle::WheelFL].driven ? vehicle::WheelFL : -1;
        d.mRightWheel = cfg.wheels[vehicle::WheelFR].driven ? vehicle::WheelFR : -1;
        d.mDifferentialRatio = cfg.engine.finalDriveRatio;
        d.mEngineTorqueRatio = torquePerDiff;
        wvSettings->mDifferentials.push_back(d);
    }
    if (rearDriven) {
        JPH::VehicleDifferentialSettings d;
        d.mLeftWheel  = cfg.wheels[vehicle::WheelRL].driven ? vehicle::WheelRL : -1;
        d.mRightWheel = cfg.wheels[vehicle::WheelRR].driven ? vehicle::WheelRR : -1;
        d.mDifferentialRatio = cfg.engine.finalDriveRatio;
        d.mEngineTorqueRatio = torquePerDiff;
        wvSettings->mDifferentials.push_back(d);
    }

    vcSettings.mController = wvSettings;  // ownership transferida via Ref

    // 4) Crear el constraint, registrar como step listener, agregar al
    //    physics system. VehicleConstraint constructor toma referencia al
    //    body — lo agarramos via lock-write y construimos in-place.
    JPH::Ref<JPH::VehicleConstraint> vc;
    JPH::Ref<JPH::VehicleCollisionTester> tester;
    {
        JPH::BodyLockWrite lock(
            m_impl->physicsSystem->GetBodyLockInterface(), chassisId);
        if (!lock.Succeeded()) {
            Log::physics()->error(
                "PhysicsWorld::createVehicle: no se pudo lock-write el "
                "chassis body para construir el constraint.");
            bi.RemoveBody(chassisId);
            bi.DestroyBody(chassisId);
            return 0;
        }
        JPH::Body& chassisRef = lock.GetBody();
        vc = new JPH::VehicleConstraint(chassisRef, vcSettings);
        // Collision tester por raycast (mas estable que shape cast para
        // arcade SA + barato). Layer Moving para que las wheels "vean"
        // tanto Static (mapa) como Moving (otros bodies).
        tester = new JPH::VehicleCollisionTesterRay(
            physics_internal::toJPHLayer(ObjectLayer::Moving));
        vc->SetVehicleCollisionTester(tester);
    }
    // AddConstraint/AddStepListener fuera del scope del lock (ellos toman
    // sus propios locks internamente).
    m_impl->physicsSystem->AddConstraint(vc);
    m_impl->physicsSystem->AddStepListener(vc);

    const u32 handle = m_impl->nextVehicleId++;
    PhysicsWorld::Impl::VehicleEntry entry{};
    entry.constraint      = vc;
    entry.collisionTester = tester;
    entry.chassisBodyId   = chassisId;
    m_impl->vehicles.emplace(handle, std::move(entry));
    Log::physics()->info(
        "PhysicsWorld::createVehicle: id={} mass={:.0f}kg wheels=4",
        handle, cfg.chassisMass);
    return handle;
}

void PhysicsWorld::destroyVehicle(u32 vehicleId) {
    if (!m_impl || vehicleId == 0) return;
    auto it = m_impl->vehicles.find(vehicleId);
    if (it == m_impl->vehicles.end()) return;

    if (it->second.constraint != nullptr) {
        m_impl->physicsSystem->RemoveStepListener(it->second.constraint);
        m_impl->physicsSystem->RemoveConstraint(it->second.constraint);
    }
    if (!it->second.chassisBodyId.IsInvalid()) {
        JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
        bi.RemoveBody(it->second.chassisBodyId);
        bi.DestroyBody(it->second.chassisBodyId);
    }
    m_impl->vehicles.erase(it);
}

void PhysicsWorld::setVehicleInput(u32 vehicleId, f32 throttle, f32 brake,
                                     f32 steer, f32 handbrake) {
    if (!m_impl || vehicleId == 0) return;
    auto it = m_impl->vehicles.find(vehicleId);
    if (it == m_impl->vehicles.end() || it->second.constraint == nullptr) return;

    // Clamps defensivos (Lua puede mandar fuera de rango).
    throttle  = std::clamp(throttle,  0.0f, 1.0f);
    brake     = std::clamp(brake,     0.0f, 1.0f);
    steer     = std::clamp(steer,    -1.0f, 1.0f);
    handbrake = std::clamp(handbrake, 0.0f, 1.0f);

    // Convencion Jolt: `mForward` = throttle pero firmado (auto trans
    // mete reverse cuando es negativo Y el vehiculo esta detenido). Para
    // arcade SA mas simple: `forward = throttle` salvo que steer hint sea
    // de reversa via brake-press-on-stop, que ya maneja el auto.
    JPH::WheeledVehicleController* ctrl =
        static_cast<JPH::WheeledVehicleController*>(
            it->second.constraint->GetController());
    if (ctrl == nullptr) return;
    ctrl->SetDriverInput(throttle, steer, brake, handbrake);

    // Despertar el chassis si esta dormido (sino el input se traga hasta
    // que algo lo despierte por colision).
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    if (!it->second.chassisBodyId.IsInvalid()) {
        if (throttle > 0.01f || brake > 0.01f || std::abs(steer) > 0.01f
            || handbrake > 0.01f) {
            bi.ActivateBody(it->second.chassisBodyId);
        }
    }
}

bool PhysicsWorld::readVehicleState(u32 vehicleId, VehicleState& out) const {
    if (!m_impl || vehicleId == 0) return false;
    auto it = m_impl->vehicles.find(vehicleId);
    if (it == m_impl->vehicles.end() || it->second.constraint == nullptr) {
        return false;
    }
    const JPH::BodyID chassisId = it->second.chassisBodyId;
    if (chassisId.IsInvalid()) return false;

    const JPH::BodyLockInterface& bli =
        m_impl->physicsSystem->GetBodyLockInterface();
    JPH::BodyLockRead lock(bli, chassisId);
    if (!lock.Succeeded()) return false;
    const JPH::Body& body = lock.GetBody();

    out.chassisWorld = jphRMatToMat4(body.GetWorldTransform());
    const JPH::Vec3 v = body.GetLinearVelocity();
    // Forward speed = proyeccion de la velocidad sobre el +Z local del
    // chasis (la convencion VehicleConstraintSettings::mForward).
    const JPH::Quat rot = body.GetRotation();
    const JPH::Vec3 fwdLocal(0, 0, 1);
    const JPH::Vec3 fwdWorld = rot * fwdLocal;
    out.forwardSpeed = v.Dot(fwdWorld);

    // Wheels: leer suspension state + composer transform por wheel.
    bool anyGrounded = false;
    const JPH::VehicleConstraint& vc = *it->second.constraint;
    for (int i = 0; i < vehicle::WheelCount; ++i) {
        const JPH::Wheel* wheel = vc.GetWheel(static_cast<JPH::uint>(i));
        if (wheel == nullptr) {
            out.wheelWorlds[i] = out.chassisWorld;
            continue;
        }
        // Local transform de la wheel (composed con suspension actual +
        // steering + rotation). `GetWheelLocalTransform` requiere los
        // ejes locales right/up de la rueda.
        const JPH::Mat44 localM = vc.GetWheelLocalTransform(
            static_cast<JPH::uint>(i),
            JPH::Vec3(1.0f, 0.0f, 0.0f),  // wheel right (rotacion sobre X)
            JPH::Vec3(0.0f, 1.0f, 0.0f)); // wheel up
        out.wheelWorlds[i] = out.chassisWorld * jphMatToMat4(localM);
        if (wheel->HasContact()) anyGrounded = true;
    }
    out.grounded = anyGrounded;

    const JPH::WheeledVehicleController* ctrl =
        static_cast<const JPH::WheeledVehicleController*>(vc.GetController());
    if (ctrl != nullptr) {
        out.currentGear = ctrl->GetTransmission().GetCurrentGear();
        out.engineRPM   = ctrl->GetEngine().GetCurrentRPM();
    }
    return true;
}

void PhysicsWorld::applyVehicleImpulse(u32 vehicleId,
                                         const glm::vec3& impulseWorld) {
    if (!m_impl || vehicleId == 0) return;
    auto it = m_impl->vehicles.find(vehicleId);
    if (it == m_impl->vehicles.end()) return;
    if (it->second.chassisBodyId.IsInvalid()) return;
    JPH::BodyInterface& bi = m_impl->physicsSystem->GetBodyInterface();
    bi.AddImpulse(it->second.chassisBodyId, vec3ToJph(impulseWorld));
}

u32 PhysicsWorld::vehicleChassisBodyId(u32 vehicleId) const {
    if (!m_impl || vehicleId == 0) return 0;
    auto it = m_impl->vehicles.find(vehicleId);
    if (it == m_impl->vehicles.end()) return 0;
    if (it->second.chassisBodyId.IsInvalid()) return 0;
    return it->second.chassisBodyId.GetIndexAndSequenceNumber();
}

u32 PhysicsWorld::vehicleCount() const {
    if (!m_impl) return 0;
    return static_cast<u32>(m_impl->vehicles.size());
}

} // namespace Mood
