// F2H67 Polish 2: AssetManager — VehicleConfig (.moodvehicle) load/cache.
// Carga JSON declarativa con TODO el tuning del vehiculo y la convierte a
// `vehicle::VehicleConfig`. Slot 0 reservado para el config default SA
// (makeDefaultSA) que sirve cuando el path es invalido o el archivo no
// existe.
//
// Formato `.moodvehicle` (JSON aditivo — campos faltantes caen al default
// SA correspondiente):
//
// {
//   "name": "Banshee_SA",
//   "chassis": {
//     "halfExtents": [0.9, 0.5, 2.0],
//     "mass": 1500.0,
//     "centerOfMassLocal": [0, -0.2, 0]
//   },
//   "engine": {
//     "maxTorque": 500.0,
//     "maxTorqueRPM": 4000.0,
//     "maxRPM": 6000.0,
//     "minRPM": 1000.0,
//     "inertia": 0.5,
//     "angularDamping": 0.2,
//     "gearRatios": [2.66, 1.78, 1.30, 1.00, 0.74],
//     "reverseGearRatios": [2.90],
//     "finalDriveRatio": 3.42,
//     "brakeTorque": 1500.0,
//     "handbrakeTorque": 4000.0
//   },
//   "maxSteerAngleDeg": 35.0,
//   "steerLerpSpeed": 4.0,
//   "wheels": [
//     {"name":"FL","attachLocal":[-0.85,-0.3,1.5],"steered":true},
//     {"name":"FR","attachLocal":[ 0.85,-0.3,1.5],"steered":true},
//     {"name":"RL","attachLocal":[-0.85,-0.3,-1.5],"handbraked":true},
//     {"name":"RR","attachLocal":[ 0.85,-0.3,-1.5],"handbraked":true}
//   ]
// }

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/physics/vehicle/VehicleConfig.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Mood {

namespace {
constexpr const char* k_defaultVehiclePath = "__default_vehicle_sa";

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

// Parse del JSON crudo a VehicleConfig. Defaults SA para campos faltantes.
vehicle::VehicleConfig parseVehicleConfigJson(const nlohmann::json& j) {
    vehicle::VehicleConfig cfg = vehicle::makeDefaultSA();

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

} // anonymous

VehicleConfigAssetId AssetManager::loadVehicleConfig(
    std::string_view logicalPath) {
    // Lazy-init del slot 0 (default SA). El AssetManager constructor no
    // crea este slot por ahora porque agregaria una dependencia mas; lo
    // generamos en la primera llamada.
    if (m_vehicleConfigs.empty()) {
        auto def = std::make_unique<vehicle::VehicleConfig>(
            vehicle::makeDefaultSA());
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
