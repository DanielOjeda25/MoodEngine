// F2H82 Bloque C: tests del writer .moodvehicle + presets. Round-trip real:
//   analysis sintético + preset → buildVehicleConfigJson → parseVehicleConfigJson
//   → VehicleConfig válido con la geometría/feel esperados. Esto ejercita
//   también el bloque `mesh_wheels` del parser (Bloque B).

#include <doctest/doctest.h>

#include "engine/assets/manager/VehicleConfigParse.h"
#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/physics/vehicle/VehicleConfigWriter.h"
#include "engine/physics/vehicle/VehiclePresets.h"

#include <nlohmann/json.hpp>

using namespace Mood;
using namespace Mood::vehicle;

namespace {

// Construye un análisis sintético de un auto "normal" (mira +Z, yaw 0) con las
// 4 ruedas detectadas por nombre y centroides NO centrados en el hub.
VehicleAnalysis makeSyntheticAnalysis(float yawDeg = 0.0f) {
    VehicleAnalysis a;
    a.ok = true;
    a.chassisHalfExtents = glm::vec3(0.9f, 0.6f, 2.1f);
    a.chassisCenter = glm::vec3(0.0f, 0.6f, 0.0f);  // base en y=0
    a.centerOfMassLocal = glm::vec3(0.0f, -0.12f, 0.0f);
    a.wheelRadius = 0.34f;
    a.wheelWidth = 0.22f;
    a.trackFront = 1.6f;
    a.trackRear = 1.62f;
    a.wheelbase = 2.6f;
    a.wheelsFound = 4;
    a.wheelsByName = true;
    a.suggestedYawOffsetDeg = yawDeg;

    // Frente en +Z si yaw 0; en -Z si yaw 180 (auto mira -Z, como el DeLorean).
    const float fz = (yawDeg > 90.0f) ? -1.3f : 1.3f;
    const float rz = -fz;
    struct WInit { WheelRole role; glm::vec3 c; const char* name; };
    const WInit inits[4] = {
        {WheelRole::FL, {-0.8f, 0.34f, fz}, "wheel_FL"},
        {WheelRole::FR, { 0.8f, 0.34f, fz}, "wheel_FR"},
        {WheelRole::RL, {-0.8f, 0.34f, rz}, "wheel_RL"},
        {WheelRole::RR, { 0.8f, 0.34f, rz}, "wheel_RR"},
    };
    for (const auto& wi : inits) {
        DetectedWheel w;
        w.role = wi.role;
        w.byName = true;
        w.radius = 0.34f;
        w.width = 0.22f;
        w.part.nodeName = wi.name;
        w.part.center = wi.c;
        w.part.aabbMin = wi.c - glm::vec3(0.11f, 0.34f, 0.34f);
        w.part.aabbMax = wi.c + glm::vec3(0.11f, 0.34f, 0.34f);
        a.wheels[static_cast<int>(wi.role)] = w;
    }
    return a;
}

VehicleImportMeta makeMeta() {
    return {"Test Car", "vehicles/test/test.glb"};
}

} // namespace

TEST_CASE("VehiclePresets F2H82 C: cada clase produce números físicos sanos") {
    for (int i = 0; i < static_cast<int>(VehicleClass::Count); ++i) {
        const VehiclePhysicsPreset p = presetFor(static_cast<VehicleClass>(i));
        CAPTURE(i);
        CHECK(p.massKg > 0.0f);
        CHECK(p.peakTorqueNm > 0.0f);
        CHECK(p.idleRpm < p.peakTorqueRpm);
        CHECK(p.peakTorqueRpm < p.redlineRpm);
        CHECK(p.finalDrive > 0.0f);
        CHECK(p.suspMinLenMm < p.suspMaxLenMm);
        CHECK(p.maxSteerDeg > 0.0f);
        CHECK(p.maxSteerDeg <= 90.0f);
    }
    // El blindado es mucho más pesado que el deportivo.
    CHECK(presetFor(VehicleClass::Blindado).massKg
          > presetFor(VehicleClass::Deportivo).massKg);
}

TEST_CASE("VehicleConfigWriter F2H82 C: round-trip writer→parser da config válido") {
    const VehicleAnalysis a = makeSyntheticAnalysis();
    const VehiclePhysicsPreset p = presetFor(VehicleClass::Deportivo);
    const std::string text = buildVehicleConfigJson(a, p, makeMeta());

    const nlohmann::json j = nlohmann::json::parse(text);
    const VehicleConfig cfg = parseVehicleConfigJson(j);

    CHECK(isValid(cfg));
    CHECK(cfg.chassisMass == doctest::Approx(p.massKg));
    CHECK(cfg.displayName == "Test Car");
    CHECK(cfg.meshPath == "vehicles/test/test.glb");
    // Deportivo = RWD: traseras driven, delanteras no.
    CHECK(cfg.wheels[WheelFL].driven == false);
    CHECK(cfg.wheels[WheelRL].driven == true);
    CHECK(cfg.wheels[WheelFL].steered == true);
    CHECK(cfg.wheels[WheelRL].handbraked == true);
}

TEST_CASE("VehicleConfigWriter F2H82 C: mesh_wheels lleva nombre real + hub offset") {
    const VehicleAnalysis a = makeSyntheticAnalysis();
    const VehiclePhysicsPreset p = presetFor(VehicleClass::Sedan);
    const nlohmann::json j =
        nlohmann::json::parse(buildVehicleConfigJson(a, p, makeMeta()));

    REQUIRE(j.contains("mesh_wheels"));
    CHECK(j["mesh_wheels"]["FL"]["submesh"] == "wheel_FL");
    CHECK(j["mesh_wheels"]["RR"]["submesh"] == "wheel_RR");

    // El parser convierte mm→m: hub_offset_mm [800,340,1300] → meshHubOffset.
    const VehicleConfig cfg = parseVehicleConfigJson(j);
    CHECK(cfg.wheels[WheelFL].meshSubName == "wheel_FL");
    CHECK(cfg.wheels[WheelFL].meshHubOffset.x == doctest::Approx(-0.8f));
    CHECK(cfg.wheels[WheelFL].meshHubOffset.z == doctest::Approx(1.3f));
    CHECK(cfg.wheels[WheelRR].meshHubOffset.z == doctest::Approx(-1.3f));
}

TEST_CASE("VehicleConfigWriter F2H82 C: front axle queda en +Z físico (yaw 0)") {
    const VehicleAnalysis a = makeSyntheticAnalysis(0.0f);
    const nlohmann::json j = nlohmann::json::parse(
        buildVehicleConfigJson(a, presetFor(VehicleClass::Sedan), makeMeta()));
    CHECK(j["axle_front"]["offset_z_mm"].get<float>() > 0.0f);
    CHECK(j["axle_rear"]["offset_z_mm"].get<float>() < 0.0f);
}

TEST_CASE("VehicleConfigWriter F2H82 C: yaw 180 remapea front (-Z model) a +Z físico") {
    // Auto que mira -Z (como el DeLorean): el frente está en -Z model pero
    // debe quedar +Z en física. El writer aplica el sgn del yaw sugerido.
    const VehicleAnalysis a = makeSyntheticAnalysis(180.0f);
    const nlohmann::json j = nlohmann::json::parse(
        buildVehicleConfigJson(a, presetFor(VehicleClass::Sedan), makeMeta()));
    CHECK(j["body"]["mesh_yaw_offset_deg"].get<float>() == doctest::Approx(180.0f));
    CHECK(j["axle_front"]["offset_z_mm"].get<float>() > 0.0f);
    CHECK(j["axle_rear"]["offset_z_mm"].get<float>() < 0.0f);
}
