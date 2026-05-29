// Tests headless de ProjectSettings (F3H1): roundtrip, back-compat,
// forward-compat, malformed input.

#include <doctest/doctest.h>

#include "engine/project/ProjectSettings.h"

#include <nlohmann/json.hpp>

using namespace Mood;

TEST_CASE("ProjectSettings defaults: toJson devuelve object vacio") {
    ProjectSettings s;
    const auto j = toJson(s);
    CHECK(j.is_object());
    CHECK(j.empty());  // ningun field difiere del default
}

TEST_CASE("ProjectSettings non-default: toJson solo escribe lo que cambio") {
    ProjectSettings s;
    s.targetFps = 120;
    const auto j = toJson(s);
    CHECK(j.contains("target_fps"));
    CHECK(j.at("target_fps") == 120);
}

TEST_CASE("ProjectSettings roundtrip preserva el campo") {
    ProjectSettings before;
    before.targetFps = 144;

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.targetFps == 144);
}

TEST_CASE("ProjectSettings fromJson de empty object devuelve defaults") {
    const auto s = projectSettingsFromJson(nlohmann::json::object());
    CHECK(s.targetFps == 60);
}

TEST_CASE("ProjectSettings fromJson de non-object devuelve defaults") {
    // null, string, array → no son object de settings
    const auto s1 = projectSettingsFromJson(nlohmann::json{});
    const auto s2 = projectSettingsFromJson(nlohmann::json("hello"));
    const auto s3 = projectSettingsFromJson(nlohmann::json::array({1, 2, 3}));
    CHECK(s1.targetFps == 60);
    CHECK(s2.targetFps == 60);
    CHECK(s3.targetFps == 60);
}

TEST_CASE("ProjectSettings fromJson forward-compat: keys desconocidas ignoradas") {
    // Simula un .moodproj escrito por una version futura del editor
    // que agrego nuevos campos.
    nlohmann::json j;
    j["target_fps"]      = 90;
    j["future_field_x"]  = 42;            // futuro: codigo viejo no la conoce
    j["future_field_y"]  = "anything";    // futuro: idem
    j["nested_future"]   = {{"a", 1}};    // futuro: subobject anidado

    // Debe cargar sin throw ni warning del field conocido.
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 90);
}

TEST_CASE("ProjectSettings fromJson tipo incorrecto: skip silencioso del field") {
    // target_fps esperado int pero llega string → field skipped, default.
    nlohmann::json j;
    j["target_fps"] = "no-soy-int";
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 60);            // default
}

// ============================================================
// F3H4: GameplaySettings (nested struct).
// ============================================================

TEST_CASE("Gameplay defaults: toJson no incluye subobjeto si todo default") {
    ProjectSettings s;  // gameplay todo default
    const auto j = toJson(s);
    CHECK(!j.contains("gameplay"));
}

TEST_CASE("Gameplay non-default: subobjeto incluido con solo los fields cambiados") {
    ProjectSettings s;
    s.gameplay.walkSpeed = 7.0f;
    const auto j = toJson(s);
    REQUIRE(j.contains("gameplay"));
    CHECK(j.at("gameplay").contains("walk_speed"));
    CHECK(j.at("gameplay").at("walk_speed").get<f32>() == doctest::Approx(7.0f));
    // crouch_speed quedo default → no se escribe
    CHECK(!j.at("gameplay").contains("crouch_speed"));
}

TEST_CASE("Gameplay roundtrip preserva los 5 fields") {
    ProjectSettings before;
    before.gameplay.walkSpeed        = 6.5f;
    before.gameplay.crouchSpeed      = 2.5f;
    before.gameplay.jumpVelocity     = 7.0f;
    before.gameplay.jumpCooldownSec  = 0.35f;
    before.gameplay.maxHealthDefault = 150.0f;  // F4H1

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.gameplay.walkSpeed        == doctest::Approx(6.5f));
    CHECK(after.gameplay.crouchSpeed      == doctest::Approx(2.5f));
    CHECK(after.gameplay.jumpVelocity     == doctest::Approx(7.0f));
    CHECK(after.gameplay.jumpCooldownSec  == doctest::Approx(0.35f));
    CHECK(after.gameplay.maxHealthDefault == doctest::Approx(150.0f));
}

TEST_CASE("F4H1: maxHealthDefault clamp [1, 10000] en fromJson") {
    // Defensivo: dev edita .moodproj a mano y mete valor invalido.
    nlohmann::json j;
    j["gameplay"]["max_health_default"] = 0.0f;  // invalido: <1
    auto s = projectSettingsFromJson(j);
    CHECK(s.gameplay.maxHealthDefault == doctest::Approx(1.0f));

    j["gameplay"]["max_health_default"] = 50000.0f;  // invalido: >10000
    s = projectSettingsFromJson(j);
    CHECK(s.gameplay.maxHealthDefault == doctest::Approx(10000.0f));

    j["gameplay"]["max_health_default"] = 250.0f;  // valido
    s = projectSettingsFromJson(j);
    CHECK(s.gameplay.maxHealthDefault == doctest::Approx(250.0f));
}

TEST_CASE("F4H1: maxHealthDefault default (no-key en JSON) = 100") {
    nlohmann::json j;
    j["gameplay"] = nlohmann::json::object();  // gameplay vacio
    const auto s = projectSettingsFromJson(j);
    CHECK(s.gameplay.maxHealthDefault == doctest::Approx(100.0f));
}

TEST_CASE("Gameplay back-compat: .moodproj pre-F3H4 (sin gameplay subkey) carga con defaults") {
    nlohmann::json j;
    j["target_fps"] = 120;  // solo el field viejo, sin "gameplay"
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps                 == 120);
    CHECK(s.gameplay.walkSpeed        == doctest::Approx(5.5f));  // default F3H4
    CHECK(s.gameplay.crouchSpeed      == doctest::Approx(3.0f));
    CHECK(s.gameplay.jumpVelocity     == doctest::Approx(5.5f));
    CHECK(s.gameplay.jumpCooldownSec  == doctest::Approx(0.2f));
    CHECK(s.gameplay.maxHealthDefault == doctest::Approx(100.0f));  // F4H1 default
}

TEST_CASE("Gameplay malformed: subkey no-object devuelve defaults silencioso") {
    nlohmann::json j;
    j["gameplay"] = "no-soy-objeto";  // futuro: bug o corrupcion
    const auto s = projectSettingsFromJson(j);
    CHECK(s.gameplay.walkSpeed == doctest::Approx(5.5f));  // default
}

// ============================================================
// F3H5: CharacterSettings (nested struct).
// ============================================================

TEST_CASE("Character defaults: toJson no incluye subobjeto si todo default") {
    ProjectSettings s;
    const auto j = toJson(s);
    CHECK(!j.contains("character"));
}

TEST_CASE("Character non-default: subobjeto incluido con solo los fields cambiados") {
    ProjectSettings s;
    s.character.radius = 0.6f;  // mas ancho
    const auto j = toJson(s);
    REQUIRE(j.contains("character"));
    CHECK(j.at("character").contains("radius"));
    CHECK(j.at("character").at("radius").get<f32>() == doctest::Approx(0.6f));
    CHECK(!j.at("character").contains("half_height_stand"));  // default → no escrito
}

TEST_CASE("Character roundtrip preserva los 7 fields") {
    ProjectSettings before;
    before.character.halfHeightStand   = 0.6f;
    before.character.halfHeightCrouch  = 0.15f;
    before.character.radius            = 0.5f;
    before.character.eyeHeightStand    = 0.8f;
    before.character.eyeHeightCrouch   = 0.4f;
    before.character.headbobFrequency  = 3.5f;
    before.character.headbobAmplitude  = 0.06f;

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.character.halfHeightStand   == doctest::Approx(0.6f));
    CHECK(after.character.halfHeightCrouch  == doctest::Approx(0.15f));
    CHECK(after.character.radius            == doctest::Approx(0.5f));
    CHECK(after.character.eyeHeightStand    == doctest::Approx(0.8f));
    CHECK(after.character.eyeHeightCrouch   == doctest::Approx(0.4f));
    CHECK(after.character.headbobFrequency  == doctest::Approx(3.5f));
    CHECK(after.character.headbobAmplitude  == doctest::Approx(0.06f));
}

TEST_CASE("Character back-compat: .moodproj pre-F3H5 (sin character) carga con defaults") {
    nlohmann::json j;
    j["target_fps"] = 120;
    j["gameplay"]   = {{"walk_speed", 6.0f}};  // pre-F3H5 podia tener gameplay (F3H4)
    const auto s = projectSettingsFromJson(j);
    CHECK(s.character.halfHeightStand   == doctest::Approx(0.5f));   // default F3H5
    CHECK(s.character.radius            == doctest::Approx(0.4f));
    CHECK(s.character.headbobFrequency  == doctest::Approx(3.5f));   // F2H41 tuning
    CHECK(s.character.headbobAmplitude  == doctest::Approx(0.05f));  // F2H41 tuning
    // gameplay tambien se carga correctamente (no interferencia entre buckets)
    CHECK(s.gameplay.walkSpeed == doctest::Approx(6.0f));
}

TEST_CASE("Character malformed: subkey no-object devuelve defaults silencioso") {
    nlohmann::json j;
    j["character"] = 42;  // futuro: corrupcion / version-mismatch
    const auto s = projectSettingsFromJson(j);
    CHECK(s.character.radius == doctest::Approx(0.4f));  // default
}

// ============================================================
// F3H6: SnapSettings (nested struct con std::vector<int>).
// ============================================================

TEST_CASE("Snap defaults: toJson no incluye subobjeto si todo default") {
    ProjectSettings s;
    const auto j = toJson(s);
    CHECK(!j.contains("snap"));
}

TEST_CASE("Snap non-default: subobjeto incluido con steps custom") {
    ProjectSettings s;
    s.snap.stepsAvailable = {1, 5, 10, 25, 50, 100};
    s.snap.defaultStepIndex = 2;  // 10
    const auto j = toJson(s);
    REQUIRE(j.contains("snap"));
    CHECK(j.at("snap").contains("steps_available"));
    CHECK(j.at("snap").contains("default_step_index"));
    CHECK(j.at("snap").at("default_step_index").get<int>() == 2);
}

TEST_CASE("Snap roundtrip preserva los 4 fields") {
    ProjectSettings before;
    before.snap.stepsAvailable = {2, 4, 8, 16};
    before.snap.defaultStepIndex = 1;
    before.snap.snapToVertexThresholdNdc = 0.05f;
    before.snap.snapBroadphaseMinWorld = 32.0f;

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.snap.stepsAvailable == std::vector<int>{2, 4, 8, 16});
    CHECK(after.snap.defaultStepIndex == 1);
    CHECK(after.snap.snapToVertexThresholdNdc == doctest::Approx(0.05f));
    CHECK(after.snap.snapBroadphaseMinWorld == doctest::Approx(32.0f));
}

TEST_CASE("Snap fromJson sanitize: array con mezcla de tipos/valores invalidos") {
    nlohmann::json j;
    nlohmann::json snap;
    // Mezcla: ints validos, negativo, cero, string, duplicado, fuera de orden.
    snap["steps_available"] = nlohmann::json::array({8, -3, 0, "garbage", 4, 4, 2, 8});
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    // Esperamos: filtrado (>0 ints), dedupe, sort ascendente.
    CHECK(s.snap.stepsAvailable == std::vector<int>{2, 4, 8});
}

TEST_CASE("Snap fromJson: array completamente invalido cae a defaults") {
    nlohmann::json j;
    nlohmann::json snap;
    snap["steps_available"] = nlohmann::json::array({-1, "x", 0});
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    // Sanitize quedo vacio → defaults completos.
    CHECK(s.snap.stepsAvailable == std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128});
    CHECK(s.snap.defaultStepIndex == 4);
}

TEST_CASE("Snap fromJson: default_step_index fuera de rango clampea a 0") {
    nlohmann::json j;
    nlohmann::json snap;
    snap["steps_available"]    = nlohmann::json::array({1, 2, 4});
    snap["default_step_index"] = 99;  // fuera de rango
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.stepsAvailable == std::vector<int>{1, 2, 4});
    CHECK(s.snap.defaultStepIndex == 0);  // clamped
}

TEST_CASE("Snap back-compat: .moodproj pre-F3H6 (sin snap) carga con defaults") {
    nlohmann::json j;
    j["target_fps"] = 120;  // solo field viejo, sin "snap"
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.stepsAvailable == std::vector<int>{1, 2, 4, 8, 16, 32, 64, 128});
    CHECK(s.snap.defaultStepIndex == 4);
    CHECK(s.snap.snapToVertexThresholdNdc == doctest::Approx(0.02f));
    CHECK(s.snap.snapBroadphaseMinWorld == doctest::Approx(16.0f));
}

// ============================================================
// F3H20: SnapSettings toggles + steps (vertex / grid / angle).
// ============================================================

TEST_CASE("Snap F3H20 defaults: todos los toggles arrancan off") {
    ProjectSettings s;
    CHECK(s.snap.snapToVertexEnabled == false);
    CHECK(s.snap.snapGridEnabled     == false);
    CHECK(s.snap.snapAngleEnabled    == false);
    CHECK(s.snap.snapGridStep        == doctest::Approx(0.5f));
    CHECK(s.snap.snapAngleDegrees    == doctest::Approx(15.0f));
}

TEST_CASE("Snap F3H20 roundtrip preserva toggles + steps") {
    ProjectSettings before;
    before.snap.snapToVertexEnabled = true;
    before.snap.snapGridEnabled     = true;
    before.snap.snapGridStep        = 0.25f;
    before.snap.snapAngleEnabled    = true;
    before.snap.snapAngleDegrees    = 45.0f;

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.snap.snapToVertexEnabled == true);
    CHECK(after.snap.snapGridEnabled     == true);
    CHECK(after.snap.snapGridStep        == doctest::Approx(0.25f));
    CHECK(after.snap.snapAngleEnabled    == true);
    CHECK(after.snap.snapAngleDegrees    == doctest::Approx(45.0f));
}

TEST_CASE("Snap F3H20 sanitize: grid_step <= 0 cae al default") {
    nlohmann::json j;
    nlohmann::json snap;
    snap["grid_enabled"] = true;
    snap["grid_step"]    = -1.0f;
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.snapGridEnabled == true);
    CHECK(s.snap.snapGridStep    == doctest::Approx(0.5f));  // default
}

TEST_CASE("Snap F3H20 sanitize: angle_degrees fuera de rango cae al default") {
    nlohmann::json j;
    nlohmann::json snap;
    snap["angle_enabled"] = true;
    snap["angle_degrees"] = 720.0f;  // > 360
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.snapAngleEnabled  == true);
    CHECK(s.snap.snapAngleDegrees  == doctest::Approx(15.0f));  // default
}

TEST_CASE("Snap F3H20 back-compat: pre-F3H20 sin toggles -> defaults off") {
    nlohmann::json j;
    nlohmann::json snap;
    snap["steps_available"]    = nlohmann::json::array({1, 2, 4});
    snap["default_step_index"] = 0;
    // sin vertex_enabled / grid_enabled / angle_enabled / etc
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.snapToVertexEnabled == false);
    CHECK(s.snap.snapGridEnabled     == false);
    CHECK(s.snap.snapAngleEnabled    == false);
    CHECK(s.snap.snapGridStep        == doctest::Approx(0.5f));
}

TEST_CASE("Snap F3H20 ignora keys de scale snap (feature removida)") {
    // Forward-compat: si un .moodproj viejo trae scale_enabled / scale_increment
    // (eran fields de F3H20 pre-iter5), deben ser ignorados sin crash y sin
    // afectar el resto.
    nlohmann::json j;
    nlohmann::json snap;
    snap["scale_enabled"]   = true;   // ignorado
    snap["scale_increment"] = 0.3f;   // ignorado
    snap["grid_enabled"]    = true;
    j["snap"] = snap;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.snap.snapGridEnabled == true);  // grid sí se aplica
    // El struct ya no tiene snapScale* — el test pasa si no rompe el load.
}
