// F2H82 Bloque C: implementación del writer .moodvehicle.

#include "engine/physics/vehicle/VehicleConfigWriter.h"

#include "core/Log.h"
#include "engine/physics/world/PhysicsWorld.h"  // F3H3: kEarthGravityMagnitude

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace Mood::vehicle {

namespace {

// Compresión de la suspensión en reposo: x = g / (2π·f)². Para precargar
// `attach_y` de modo que el centro de la rueda en reposo caiga a la altura
// real del modelo. Mismo cálculo que vehicle::wheelRestCompression.
// F3H3: g desde la constante canonica de physics/ (compartida con el
// mundo Jolt + la implementacion de wheelRestCompression).
f32 restCompression(f32 freqHz) {
    constexpr f32 kTwoPi = 2.0f * 3.14159265358979323846f;
    const f32 omega = kTwoPi * freqHz;
    if (omega <= 1e-6f) return 0.0f;
    return physics::kEarthGravityMagnitude / (omega * omega);
}

// Promedio de un campo de las 2 ruedas de un eje, usando solo las detectadas.
// `getter` recibe la rueda y devuelve el valor. Si ninguna está, devuelve
// `fallback`.
template <typename Fn>
f32 axleAvg(const VehicleAnalysis& a, WheelRole l, WheelRole r, Fn getter,
            f32 fallback) {
    f32 sum = 0.0f; int n = 0;
    if (a.wheels[static_cast<int>(l)].role != WheelRole::Unknown) {
        sum += getter(a.wheels[static_cast<int>(l)]); ++n;
    }
    if (a.wheels[static_cast<int>(r)].role != WheelRole::Unknown) {
        sum += getter(a.wheels[static_cast<int>(r)]); ++n;
    }
    return n > 0 ? sum / static_cast<f32>(n) : fallback;
}

nlohmann::json vec3mm(const glm::vec3& v) {
    return nlohmann::json::array(
        {v.x * 1000.0f, v.y * 1000.0f, v.z * 1000.0f});
}

} // namespace

std::string buildVehicleConfigJson(const VehicleAnalysis& aRaw,
                                   const VehiclePhysicsPreset& p,
                                   const VehicleImportMeta& meta,
                                   bool pretty) {
    using nlohmann::json;
    json j;
    j["schemaVersion"] = 2;
    j["metadata"] = {{"name", meta.displayName},
                     {"generated_by", "MoodEngine vehicle importer (F2H82)"}};

    // F2H82 polish: aplicar `meta.meshScale` a las medidas FISICAS del analisis
    // (chassis, ruedas) — el modelo viene en cm/mm y el dev pidio convertir a
    // metros. NO escalamos `mesh_wheels.hub_offset_mm` (queda en unidades crudas
    // del mesh, lo aplica el render multiplicando por TransformComponent.scale).
    const f32 ms = (meta.meshScale > 0.0f) ? meta.meshScale : 1.0f;
    VehicleAnalysis a = aRaw;
    if (std::fabs(ms - 1.0f) > 1e-6f) {
        a.chassisAabbMin     *= ms;
        a.chassisAabbMax     *= ms;
        a.chassisCenter      *= ms;
        a.overallAabbMin     *= ms;
        a.overallAabbMax     *= ms;
        a.chassisHalfExtents *= ms;
        a.centerOfMassLocal  *= ms;
        a.trackFront         *= ms;
        a.trackRear          *= ms;
        a.wheelbase          *= ms;
        a.wheelRadius        *= ms;
        a.wheelWidth         *= ms;
        for (auto& w : a.wheels) {
            w.part.center *= ms;
            w.part.aabbMin *= ms;
            w.part.aabbMax *= ms;
            w.radius *= ms;
            w.width  *= ms;
        }
    }

    // --- frame mesh→física ---
    // El análisis está en MODEL space. La física usa +Z forward / +X right.
    // El analyzer sugiere yaw 0 o 180 (auto que mira -Z). Bajo 180, los signos
    // de X y Z se invierten pero las MAGNITUDES (extents, track, wheelbase) no.
    const f32 yaw = a.suggestedYawOffsetDeg;
    const f32 sgn = (std::abs(yaw - 180.0f) < 1.0f) ? -1.0f : 1.0f;

    // --- body ---
    // F2H82 Bloque B (caja al centro del cuerpo): si el modelo tiene el origen
    // fuera del centro del cuerpo (p.ej. origen en la base/atras), emitimos
    // `box_offset_mm` para que el motor envuelva la caja fisica en un
    // RotatedTranslatedShape. Y la `mass_center_override_mm` la expresamos
    // relativa al origen del modelo (= chassisCenter + CoM_local_del_chasis)
    // para que el CoM termine un poco debajo del centro del cuerpo, NO debajo
    // del piso.
    const glm::vec3 comFromOrigin = a.chassisCenter + a.centerOfMassLocal;
    j["body"] = {
        {"mesh_path", meta.meshPath},
        // dimensions = [width(X), height(Y), length(Z)] full extents.
        {"dimensions_mm", vec3mm(a.chassisHalfExtents * 2.0f)},
        {"mass_kg", p.massKg},
        {"mass_center_override_mm", vec3mm(comFromOrigin)},
        {"box_offset_mm", vec3mm(a.chassisCenter)},
        {"mesh_yaw_offset_deg", yaw},
        {"linear_damping", p.chassisLinearDamping},
        {"angular_damping", p.chassisAngularDamping},
    };

    // --- ejes ---
    // offset_z (física) del eje = (Z del eje en model − Z centro chasis) · sgn.
    // Front debería quedar +, rear −. attach_y precarga la altura para que el
    // centro de la rueda en reposo caiga a la altura real del modelo.
    const f32 chassisCz = a.chassisCenter.z;
    const f32 chassisCy = a.chassisCenter.y;
    const f32 restLen = std::max(p.suspMinLenMm / 1000.0f,
                                 p.suspMaxLenMm / 1000.0f
                                     - restCompression(p.suspFrequencyHz));

    auto wheelZ = [](const DetectedWheel& w) { return w.part.center.z; };
    auto wheelY = [](const DetectedWheel& w) { return w.part.center.y; };

    const f32 frontZ = axleAvg(a, WheelRole::FL, WheelRole::FR, wheelZ,
                               chassisCz + a.chassisHalfExtents.z * 0.6f);
    const f32 rearZ  = axleAvg(a, WheelRole::RL, WheelRole::RR, wheelZ,
                               chassisCz - a.chassisHalfExtents.z * 0.6f);
    const f32 frontY = axleAvg(a, WheelRole::FL, WheelRole::FR, wheelY,
                               chassisCy - a.chassisHalfExtents.y * 0.5f);
    const f32 rearY  = axleAvg(a, WheelRole::RL, WheelRole::RR, wheelY,
                               chassisCy - a.chassisHalfExtents.y * 0.5f);

    const f32 trackF = (a.trackFront > 0.01f) ? a.trackFront
                                              : a.chassisHalfExtents.x * 1.7f;
    const f32 trackR = (a.trackRear > 0.01f) ? a.trackRear : trackF;

    // torque por eje según tren motriz.
    const bool frontDriven = (p.drivetrain == Drivetrain::FWD
                              || p.drivetrain == Drivetrain::AWD);
    const bool rearDriven  = (p.drivetrain == Drivetrain::RWD
                              || p.drivetrain == Drivetrain::AWD);

    auto makeAxle = [&](f32 zModel, f32 yModel, f32 track, bool driven,
                        bool steered, bool handbraked) {
        // IMPORTANTE: offset_z y attach_y van relativos al ORIGEN del modelo,
        // no al centro del chasis. El motor coloca las ruedas (attachLocal)
        // relativas al origen del entity = origen del modelo, y el cuerpo se
        // renderiza con su geometría en model space (base al piso vía
        // chassisRenderYOffset). Si los expresáramos relativos al centro del
        // chasis, en modelos con origen descentrado (origen abajo/atrás, no en
        // el centro del cuerpo) las ruedas quedan despegadas del cuerpo.
        // El `sgn` reconcilia el yaw (auto que mira -Z → física +Z).
        return json{
            {"offset_z_mm", zModel * sgn * 1000.0f},
            {"track_mm", track * 1000.0f},
            {"attach_y_mm", (yModel + restLen) * 1000.0f},
            {"wheel", {
                {"radius_mm", a.wheelRadius * 1000.0f},
                {"width_mm", a.wheelWidth * 1000.0f},
                {"friction_long", p.frictionLong},
                {"friction_lat", p.frictionLat},
            }},
            {"suspension", {
                {"frequency_hz", p.suspFrequencyHz},
                {"damping", p.suspDamping},
                {"max_length_mm", p.suspMaxLenMm},
                {"min_length_mm", p.suspMinLenMm},
            }},
            {"torque_factor", driven ? 1.0f : 0.0f},
            {"steered", steered},
            {"handbraked", handbraked},
        };
    };

    j["axle_front"] = makeAxle(frontZ, frontY, trackF, frontDriven,
                               /*steered*/true, /*handbraked*/false);
    j["axle_rear"]  = makeAxle(rearZ, rearY, trackR, rearDriven,
                               /*steered*/false, /*handbraked*/true);

    // --- motor / frenos / dirección ---
    j["engine"] = {
        {"peak_torque_nm", p.peakTorqueNm},
        {"peak_torque_rpm", p.peakTorqueRpm},
        {"redline_rpm", p.redlineRpm},
        {"idle_rpm", p.idleRpm},
        {"final_drive_ratio", p.finalDrive},
        {"transmission", p.transmission},
        {"inertia", 0.5f},
        {"angular_damping", 0.2f},
    };
    j["brakes"] = {
        {"deceleration_target_mps2", p.decelTargetMps2},
        {"handbrake_ratio", p.handbrakeRatio},
    };
    j["steering"] = {
        {"max_angle_deg", p.maxSteerDeg},
        {"steer_lerp_speed", p.steerLerp},
    };

    // --- mesh_wheels (Bloque B): binding visual por rol detectado ---
    // OJO: el hub_offset_mm se guarda en unidades CRUDAS del mesh (NO se
    // multiplica por meta.meshScale). El render aplica TransformComponent.scale
    // al chassis y a la wheel-entity, que ya incluye meshScale — escalar aca
    // duplicaria.
    json mw = json::object();
    static const std::array<std::pair<const char*, WheelRole>, 4> kRoles = {{
        {"FL", WheelRole::FL}, {"FR", WheelRole::FR},
        {"RL", WheelRole::RL}, {"RR", WheelRole::RR}}};
    for (const auto& [key, role] : kRoles) {
        const DetectedWheel& wRaw = aRaw.wheels[static_cast<int>(role)];
        if (wRaw.role == WheelRole::Unknown) continue;
        mw[key] = {
            {"submesh", wRaw.part.nodeName},
            {"hub_offset_mm", vec3mm(wRaw.part.center)},
        };
    }
    if (!mw.empty()) j["mesh_wheels"] = std::move(mw);

    // F2H82 polish: NO escribimos `mesh_scale` en el JSON nuevo: la escala se
    // bakeó en el nodo raíz del .glb al copiarlo. El reader sigue soportando
    // `mesh_scale` para back-compat con .moodvehicle viejos.

    return pretty ? j.dump(2) : j.dump();
}

bool writeVehicleConfigFile(const VehicleAnalysis& a,
                            const VehiclePhysicsPreset& p,
                            const VehicleImportMeta& meta,
                            const std::string& outFsPath,
                            std::string& err) {
    if (!a.ok) {
        err = "análisis inválido (a.ok == false)";
        return false;
    }
    const std::string text = buildVehicleConfigJson(a, p, meta, /*pretty*/true);

    std::error_code ec;
    const std::filesystem::path path(outFsPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        // ec acá no es fatal: si el dir ya existe create_directories no falla,
        // y si falla de verdad lo detecta el ofstream abajo.
    }
    std::ofstream out(outFsPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        err = "no se pudo abrir para escritura: " + outFsPath;
        return false;
    }
    out << text;
    if (!out) {
        err = "error escribiendo: " + outFsPath;
        return false;
    }
    Log::assets()->info("VehicleConfigWriter: escrito '{}' ({} bytes)",
                        outFsPath, text.size());
    return true;
}

} // namespace Mood::vehicle
