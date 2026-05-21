// F2H67 Polish 2: AssetManager — VehicleConfig (.moodvehicle) load/cache.
// F2H70 Bloque C: schema v2 axle-based estilo Source/Valve.
// Carga JSON declarativa con TODO el tuning del vehiculo y la convierte a
// `vehicle::VehicleConfig`. Slot 0 reservado para el config default SA
// (makeFallbackGenericSedan) que sirve cuando el path es invalido o el archivo no
// existe.
//
// Schema v1 (legacy, sigue funcionando):
//   Una entry por wheel + chassis flat + engine flat (ver parseV1 abajo).
//
// Schema v2 (preferido, estilo Source `scripts/vehicles/<car>.txt`):
//   body / axle_front / axle_rear / engine / brakes / steering. Units
//   amigables al dev (mm, kg, HP, Nm @ RPM) con conversion interna al SI.
//   Ver ejemplo en docs/asset_conventions.md o
//   assets/vehicles/delorean_dmc12/delorean_dmc12.moodvehicle.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/physics/vehicle/VehicleConfig.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Mood {

namespace {
constexpr const char* k_defaultVehiclePath = "__fallback_generic_sedan";

// Helper: lee un vec3 desde JSON array. Faltante o malformado -> fallback.
glm::vec3 readVec3(const nlohmann::json& j, const char* key,
                    const glm::vec3& fallback) {
    if (!j.contains(key) || !j.at(key).is_array() || j.at(key).size() < 3) {
        return fallback;
    }
    return glm::vec3(
        j.at(key)[0].get<f32>(),
        j.at(key)[1].get<f32>(),
        j.at(key)[2].get<f32>());
}

// --- Schema v2 (axle-based estilo Source) ---

// Presets de transmissions. Cuando el JSON declara `transmission: "<name>"`
// sin `gear_ratios` explicito, expandimos al preset. Si el JSON pone
// `gear_ratios` array, el override gana.
struct TransmissionPreset {
    std::vector<f32> forward;
    std::vector<f32> reverse;
};
TransmissionPreset presetTransmission(const std::string& name) {
    if (name == "5-speed-manual") return {{3.36f, 2.06f, 1.38f, 1.00f, 0.82f}, {3.10f}};
    if (name == "4-speed-auto")   return {{2.40f, 1.47f, 1.00f, 0.74f},        {2.20f}};
    if (name == "6-speed-manual") return {{3.45f, 2.05f, 1.40f, 1.10f, 0.85f, 0.65f}, {3.20f}};
    // Default SA (Banshee-like).
    return {{2.66f, 1.78f, 1.30f, 1.00f, 0.74f}, {2.90f}};
}

// Aplica un bloque `axle` (front o rear) a los 2 wheels del eje en cfg,
// expandiendo offset_z + track a posiciones FL/FR o RL/RR.
void applyAxleV2(const nlohmann::json& jaxle, vehicle::VehicleConfig& cfg,
                  int leftIdx, int rightIdx, bool isFront) {
    const f32 offset_z_m = jaxle.value("offset_z_mm",
                                         isFront ? 1500.0f : -1500.0f) / 1000.0f;
    const f32 track_m = jaxle.value("track_mm", 1700.0f) / 1000.0f;
    const f32 attach_y = jaxle.value("attach_y_mm", -300.0f) / 1000.0f;

    f32 radius_m = cfg.wheels[leftIdx].radius;
    f32 width_m  = cfg.wheels[leftIdx].width;
    f32 friction_long = cfg.wheels[leftIdx].longitudinalFriction;
    f32 friction_lat  = cfg.wheels[leftIdx].lateralFriction;
    if (jaxle.contains("wheel") && jaxle.at("wheel").is_object()) {
        const auto& jw = jaxle.at("wheel");
        radius_m = jw.value("radius_mm", radius_m * 1000.0f) / 1000.0f;
        width_m  = jw.value("width_mm",  width_m  * 1000.0f) / 1000.0f;
        friction_long = jw.value("friction_long", friction_long);
        friction_lat  = jw.value("friction_lat",  friction_lat);
    }

    f32 susp_freq  = cfg.wheels[leftIdx].suspensionFrequency;
    f32 susp_damp  = cfg.wheels[leftIdx].suspensionDamping;
    f32 susp_max_m = cfg.wheels[leftIdx].suspensionMaxLength;
    f32 susp_min_m = cfg.wheels[leftIdx].suspensionMinLength;
    if (jaxle.contains("suspension") && jaxle.at("suspension").is_object()) {
        const auto& js = jaxle.at("suspension");
        susp_freq  = js.value("frequency_hz",  susp_freq);
        susp_damp  = js.value("damping",       susp_damp);
        susp_max_m = js.value("max_length_mm", susp_max_m * 1000.0f) / 1000.0f;
        susp_min_m = js.value("min_length_mm", susp_min_m * 1000.0f) / 1000.0f;
    }

    // torque_factor 0 => no driven; > 0 => driven (Jolt reparte uniforme
    // entre las driven wheels; el factor exacto entre axles queda como
    // mejora futura cuando integremos differential proper).
    const f32 torque_factor = jaxle.value("torque_factor", 1.0f);
    const bool steered    = jaxle.value("steered", isFront);
    const bool handbraked = jaxle.value("handbraked", !isFront);

    for (int idx : {leftIdx, rightIdx}) {
        vehicle::WheelConfig& wc = cfg.wheels[idx];
        const f32 x_sign = (idx == leftIdx) ? -1.0f : 1.0f;
        wc.attachLocal = glm::vec3(x_sign * track_m * 0.5f, attach_y, offset_z_m);
        wc.radius = radius_m;
        wc.width  = width_m;
        wc.suspensionMaxLength  = susp_max_m;
        wc.suspensionMinLength  = susp_min_m;
        wc.suspensionFrequency  = susp_freq;
        wc.suspensionDamping    = susp_damp;
        wc.longitudinalFriction = friction_long;
        wc.lateralFriction      = friction_lat;
        wc.driven     = (torque_factor > 0.0f);
        wc.steered    = steered;
        wc.handbraked = handbraked;
    }
}

vehicle::VehicleConfig parseVehicleConfigJsonV2(const nlohmann::json& j) {
    vehicle::VehicleConfig cfg = vehicle::makeFallbackGenericSedan();

    // metadata.name -> displayName (nombre legible para tag/browser).
    if (j.contains("metadata") && j.at("metadata").is_object()) {
        const auto& jm = j.at("metadata");
        if (jm.contains("name") && jm.at("name").is_string()) {
            cfg.displayName = jm.at("name").get<std::string>();
        }
    }

    // body
    if (j.contains("body") && j.at("body").is_object()) {
        const auto& jb = j.at("body");
        if (jb.contains("dimensions_mm") && jb.at("dimensions_mm").is_array()
            && jb.at("dimensions_mm").size() >= 3) {
            const auto& d = jb.at("dimensions_mm");
            // dimensions_mm = [width, height, length] full extents
            cfg.chassisHalfExtents = glm::vec3(
                d[0].get<f32>() / 2000.0f,
                d[1].get<f32>() / 2000.0f,
                d[2].get<f32>() / 2000.0f);
        }
        cfg.chassisMass = jb.value("mass_kg", cfg.chassisMass);
        // F2H70.3 Bloque F: mesh visual self-contained. Path logico relativo
        // a assets/ — usado por el viewport drop para spawnear el entity con
        // su MeshRenderer ya cableado.
        cfg.meshPath = jb.value("mesh_path", cfg.meshPath);
        if (jb.contains("mass_center_override_mm")
            && jb.at("mass_center_override_mm").is_array()
            && jb.at("mass_center_override_mm").size() >= 3) {
            const auto& mc = jb.at("mass_center_override_mm");
            cfg.centerOfMassLocal = glm::vec3(
                mc[0].get<f32>() / 1000.0f,
                mc[1].get<f32>() / 1000.0f,
                mc[2].get<f32>() / 1000.0f);
        }
        // F2H70.2: damping del chassis. Opcional — si no aparece, el config
        // mantiene los defaults (0.5 / 0.5 arcade) del struct. Bajar a
        // 0.05-0.1 para feel sim (momentum largo, sensacion pesada);
        // subir a 0.7-1.0 para arcade snappy (el auto se detiene rapido).
        cfg.chassisLinearDamping  = jb.value("linear_damping",  cfg.chassisLinearDamping);
        cfg.chassisAngularDamping = jb.value("angular_damping", cfg.chassisAngularDamping);
        // F2H70.2 D5: mesh_yaw_offset_deg / mesh_forward_axis. El campo
        // canonical es `mesh_yaw_offset_deg` (numero en grados). Como azucar
        // para el dev, aceptamos tambien `mesh_forward_axis` con un string
        // ("+Z" | "-Z" | "+X" | "-X") y lo convertimos al yaw equivalente.
        // Convencion: axis "+Z" = 0° (default engine forward), "+X" = -90°,
        // "-Z" = 180°, "-X" = +90°. Si ambos campos aparecen, gana el
        // explicit numerico.
        if (jb.contains("mesh_yaw_offset_deg")) {
            cfg.meshYawOffsetDeg = jb.value("mesh_yaw_offset_deg",
                                              cfg.meshYawOffsetDeg);
        } else if (jb.contains("mesh_forward_axis")
                   && jb.at("mesh_forward_axis").is_string()) {
            const std::string ax = jb.at("mesh_forward_axis").get<std::string>();
            if      (ax == "+Z" || ax == "Z")  cfg.meshYawOffsetDeg = 0.0f;
            else if (ax == "-Z")               cfg.meshYawOffsetDeg = 180.0f;
            else if (ax == "+X" || ax == "X")  cfg.meshYawOffsetDeg = -90.0f;
            else if (ax == "-X")               cfg.meshYawOffsetDeg = 90.0f;
            else {
                Log::assets()->warn(
                    "AssetManager: mesh_forward_axis='{}' no reconocido "
                    "(esperaba +Z/-Z/+X/-X). Asumiendo +Z.", ax);
            }
        }
    }

    // axles → expand to 4 wheels
    if (j.contains("axle_front") && j.at("axle_front").is_object()) {
        applyAxleV2(j.at("axle_front"), cfg,
                     vehicle::WheelFL, vehicle::WheelFR, /*isFront*/true);
    }
    if (j.contains("axle_rear") && j.at("axle_rear").is_object()) {
        applyAxleV2(j.at("axle_rear"), cfg,
                     vehicle::WheelRL, vehicle::WheelRR, /*isFront*/false);
    }

    // engine
    if (j.contains("engine") && j.at("engine").is_object()) {
        const auto& je = j.at("engine");
        cfg.engine.maxTorque       = je.value("peak_torque_nm",  cfg.engine.maxTorque);
        cfg.engine.maxTorqueRPM    = je.value("peak_torque_rpm", cfg.engine.maxTorqueRPM);
        cfg.engine.maxRPM          = je.value("redline_rpm",     cfg.engine.maxRPM);
        cfg.engine.minRPM          = je.value("idle_rpm",        cfg.engine.minRPM);
        cfg.engine.finalDriveRatio = je.value("final_drive_ratio", cfg.engine.finalDriveRatio);
        cfg.engine.inertia         = je.value("inertia",         cfg.engine.inertia);
        cfg.engine.angularDamping  = je.value("angular_damping", cfg.engine.angularDamping);

        // gear ratios: explicit override gana; sino transmission preset.
        if (je.contains("gear_ratios") && je.at("gear_ratios").is_array()) {
            cfg.engine.gearRatios.clear();
            for (const auto& r : je.at("gear_ratios"))
                cfg.engine.gearRatios.push_back(r.get<f32>());
        } else if (je.contains("transmission") && je.at("transmission").is_string()) {
            TransmissionPreset p = presetTransmission(
                je.at("transmission").get<std::string>());
            cfg.engine.gearRatios        = std::move(p.forward);
            cfg.engine.reverseGearRatios = std::move(p.reverse);
        }
        if (je.contains("reverse_ratios") && je.at("reverse_ratios").is_array()) {
            cfg.engine.reverseGearRatios.clear();
            for (const auto& r : je.at("reverse_ratios"))
                cfg.engine.reverseGearRatios.push_back(r.get<f32>());
        }
    }

    // brakes: derivar brakeTorque desde deceleracion target + mass + radius.
    // Patron Source: el dev declara "quiero frenar a X m/s^2 desde Y km/h"
    // en lugar de Nm crudos.
    //   torque_por_wheel = mass * deceleration * radius / n_wheels_brake
    // Asumimos las 4 wheels frenan (cfg actual no expone brake_factor
    // distinto entre axles; queda para Bloque F+).
    if (j.contains("brakes") && j.at("brakes").is_object()) {
        const auto& jb = j.at("brakes");
        const f32 decel_target = jb.value("deceleration_target_mps2", 7.0f);
        const f32 wheel_r = cfg.wheels[vehicle::WheelFL].radius;
        cfg.engine.brakeTorque =
            (cfg.chassisMass * decel_target * wheel_r) / 4.0f;
        const f32 handbrake_ratio = jb.value("handbrake_ratio", 0.5f);
        cfg.engine.handbrakeTorque =
            cfg.engine.brakeTorque * (1.0f + handbrake_ratio * 2.0f);
    }

    // steering
    if (j.contains("steering") && j.at("steering").is_object()) {
        const auto& js = j.at("steering");
        // max_angle_deg_slow es el "angle a baja velocidad" en Source. v1
        // del engine usa un solo angulo — tomamos el slow (mas tipico, ~30°).
        cfg.maxSteerAngleDeg = js.value("max_angle_deg_slow",
            js.value("max_angle_deg", cfg.maxSteerAngleDeg));
        cfg.steerLerpSpeed = js.value("throttle_steering_rest_rate_slow",
            js.value("steer_lerp_speed", cfg.steerLerpSpeed));
    }

    return cfg;
}

// --- Schema v1 (legacy, key naming flat) ---

vehicle::VehicleConfig parseVehicleConfigJsonV1(const nlohmann::json& j) {
    vehicle::VehicleConfig cfg = vehicle::makeFallbackGenericSedan();

    if (j.contains("chassis") && j.at("chassis").is_object()) {
        const auto& jc = j.at("chassis");
        cfg.chassisHalfExtents =
            readVec3(jc, "halfExtents", cfg.chassisHalfExtents);
        cfg.chassisMass = jc.value("mass", cfg.chassisMass);
        cfg.centerOfMassLocal =
            readVec3(jc, "centerOfMassLocal", cfg.centerOfMassLocal);
    }
    if (j.contains("engine") && j.at("engine").is_object()) {
        const auto& je = j.at("engine");
        cfg.engine.maxTorque       = je.value("maxTorque",       cfg.engine.maxTorque);
        cfg.engine.maxTorqueRPM    = je.value("maxTorqueRPM",    cfg.engine.maxTorqueRPM);
        cfg.engine.maxRPM          = je.value("maxRPM",          cfg.engine.maxRPM);
        cfg.engine.minRPM          = je.value("minRPM",          cfg.engine.minRPM);
        cfg.engine.inertia         = je.value("inertia",         cfg.engine.inertia);
        cfg.engine.angularDamping  = je.value("angularDamping",  cfg.engine.angularDamping);
        cfg.engine.finalDriveRatio = je.value("finalDriveRatio", cfg.engine.finalDriveRatio);
        cfg.engine.brakeTorque     = je.value("brakeTorque",     cfg.engine.brakeTorque);
        cfg.engine.handbrakeTorque = je.value("handbrakeTorque", cfg.engine.handbrakeTorque);
        if (je.contains("gearRatios") && je.at("gearRatios").is_array()) {
            cfg.engine.gearRatios.clear();
            for (const auto& r : je.at("gearRatios")) {
                cfg.engine.gearRatios.push_back(r.get<f32>());
            }
        }
        if (je.contains("reverseGearRatios")
            && je.at("reverseGearRatios").is_array()) {
            cfg.engine.reverseGearRatios.clear();
            for (const auto& r : je.at("reverseGearRatios")) {
                cfg.engine.reverseGearRatios.push_back(r.get<f32>());
            }
        }
    }
    cfg.maxSteerAngleDeg = j.value("maxSteerAngleDeg", cfg.maxSteerAngleDeg);
    cfg.steerLerpSpeed   = j.value("steerLerpSpeed",   cfg.steerLerpSpeed);

    // Wheels: array de 4. Si trae menos, los faltantes mantienen los
    // defaults SA. Cada wheel acepta un subset de campos (todos opcionales).
    if (j.contains("wheels") && j.at("wheels").is_array()) {
        const auto& jw = j.at("wheels");
        const usize n = std::min<usize>(jw.size(), vehicle::WheelCount);
        for (usize i = 0; i < n; ++i) {
            const auto& w = jw[i];
            vehicle::WheelConfig& wc = cfg.wheels[i];
            wc.attachLocal = readVec3(w, "attachLocal", wc.attachLocal);
            wc.radius = w.value("radius", wc.radius);
            wc.width  = w.value("width",  wc.width);
            wc.suspensionMaxLength  = w.value("suspensionMaxLength",  wc.suspensionMaxLength);
            wc.suspensionMinLength  = w.value("suspensionMinLength",  wc.suspensionMinLength);
            wc.suspensionFrequency  = w.value("suspensionFrequency",  wc.suspensionFrequency);
            wc.suspensionDamping    = w.value("suspensionDamping",    wc.suspensionDamping);
            wc.longitudinalFriction = w.value("longitudinalFriction", wc.longitudinalFriction);
            wc.lateralFriction      = w.value("lateralFriction",      wc.lateralFriction);
            wc.driven     = w.value("driven",     wc.driven);
            wc.steered    = w.value("steered",    wc.steered);
            wc.handbraked = w.value("handbraked", wc.handbraked);
        }
    }
    return cfg;
}

// Dispatcher: lee `schemaVersion` y delega al parser correspondiente.
// Default v1 cuando el campo falta (back-compat con assets pre-F2H70).
vehicle::VehicleConfig parseVehicleConfigJson(const nlohmann::json& j) {
    const int version = j.value("schemaVersion", 1);
    if (version >= 2) return parseVehicleConfigJsonV2(j);
    return parseVehicleConfigJsonV1(j);
}

} // anonymous

VehicleConfigAssetId AssetManager::loadVehicleConfig(
    std::string_view logicalPath) {
    // Lazy-init del slot 0 (default SA). El AssetManager constructor no
    // crea este slot por ahora porque agregaria una dependencia mas; lo
    // generamos en la primera llamada.
    if (m_vehicleConfigs.empty()) {
        auto def = std::make_unique<vehicle::VehicleConfig>(
            vehicle::makeFallbackGenericSedan());
        m_vehicleConfigs.push_back(std::move(def));
        m_vehicleConfigPaths.emplace_back(k_defaultVehiclePath);
    }

    const std::string key(logicalPath);
    if (key.empty()) return missingVehicleConfigId();

    if (auto it = m_vehicleConfigCache.find(key);
        it != m_vehicleConfigCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: vehicle path '{}' rechazado por VFS. Fallback al default SA.",
            logicalPath);
        m_vehicleConfigCache.emplace(key, missingVehicleConfigId());
        return missingVehicleConfigId();
    }
    std::ifstream in(fs);
    if (!in.good()) {
        Log::assets()->warn(
            "AssetManager: no se pudo abrir vehicle '{}'. Fallback al default SA.",
            fs.generic_string());
        m_vehicleConfigCache.emplace(key, missingVehicleConfigId());
        return missingVehicleConfigId();
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "AssetManager: vehicle '{}' no es JSON valido ({}). Fallback al default SA.",
            fs.generic_string(), e.what());
        m_vehicleConfigCache.emplace(key, missingVehicleConfigId());
        return missingVehicleConfigId();
    }

    auto cfg = std::make_unique<vehicle::VehicleConfig>(parseVehicleConfigJson(j));
    if (!vehicle::isValid(*cfg)) {
        Log::assets()->warn(
            "AssetManager: vehicle '{}' parseado pero config invalido. Fallback al default SA.",
            fs.generic_string());
        m_vehicleConfigCache.emplace(key, missingVehicleConfigId());
        return missingVehicleConfigId();
    }
    const VehicleConfigAssetId id =
        static_cast<VehicleConfigAssetId>(m_vehicleConfigs.size());
    m_vehicleConfigs.push_back(std::move(cfg));
    m_vehicleConfigPaths.push_back(key);
    m_vehicleConfigCache.emplace(key, id);
    Log::assets()->info(
        "AssetManager: vehicle '{}' cargado en slot {}.", key, id);
    return id;
}

const vehicle::VehicleConfig* AssetManager::getVehicleConfig(
    VehicleConfigAssetId id) const {
    if (m_vehicleConfigs.empty()) return nullptr;
    if (id >= m_vehicleConfigs.size()) {
        return m_vehicleConfigs[0].get();  // fallback slot 0
    }
    return m_vehicleConfigs[id].get();
}

std::string AssetManager::vehicleConfigPathOf(
    VehicleConfigAssetId id) const {
    if (id >= m_vehicleConfigPaths.size()) return std::string{};
    return m_vehicleConfigPaths[id];
}

usize AssetManager::vehicleConfigCount() const {
    return m_vehicleConfigs.size();
}

} // namespace Mood
