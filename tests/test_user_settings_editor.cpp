// Tests headless del struct UserSettings::EditorSettings (F3H7).
//
// Cubrimos las funciones libres editorSettingsToJson / editorSettingsFromJson
// SIN tocar el filesystem — el init/save real escribe a APPDATA y
// contaminaria el state del dev. La persistencia end-to-end se valida
// en vivo (cerrar + reabrir el editor preserva los values).

#include <doctest/doctest.h>

#include "core/UserSettings.h"

#include <nlohmann/json.hpp>

using namespace Mood;
using EditorSettings = UserSettings::EditorSettings;
using UserSettings::editorSettingsToJson;
using UserSettings::editorSettingsFromJson;

TEST_CASE("EditorSettings defaults: toJson devuelve object vacio") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK(j.is_object());
    CHECK(j.empty());  // ningun field difiere del default
}

TEST_CASE("EditorSettings non-default: toJson solo escribe lo que cambio") {
    EditorSettings s;
    s.gizmoArmLengthPx = 100.0f;
    const auto j = editorSettingsToJson(s);
    CHECK(j.size() == 1u);
    CHECK(j.contains("gizmo_arm_length_px"));
    CHECK(j.at("gizmo_arm_length_px") == doctest::Approx(100.0f));
}

TEST_CASE("EditorSettings roundtrip preserva los 5 fields") {
    EditorSettings before;
    before.orthoInitialZoom      = 48.0f;
    before.orthoZoomFactor       = 1.25f;
    before.gizmoArmLengthPx      = 90.0f;
    before.gizmoRotateRingPx     = 100.0f;
    before.clickDragThresholdPx  = 8;

    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);

    CHECK(after.orthoInitialZoom     == doctest::Approx(48.0f));
    CHECK(after.orthoZoomFactor      == doctest::Approx(1.25f));
    CHECK(after.gizmoArmLengthPx     == doctest::Approx(90.0f));
    CHECK(after.gizmoRotateRingPx    == doctest::Approx(100.0f));
    CHECK(after.clickDragThresholdPx == 8);
}

TEST_CASE("EditorSettings fromJson de empty object devuelve defaults") {
    const auto s = editorSettingsFromJson(nlohmann::json::object());
    const EditorSettings defaults;
    CHECK(s.orthoInitialZoom     == doctest::Approx(defaults.orthoInitialZoom));
    CHECK(s.orthoZoomFactor      == doctest::Approx(defaults.orthoZoomFactor));
    CHECK(s.gizmoArmLengthPx     == doctest::Approx(defaults.gizmoArmLengthPx));
    CHECK(s.gizmoRotateRingPx    == doctest::Approx(defaults.gizmoRotateRingPx));
    CHECK(s.clickDragThresholdPx == defaults.clickDragThresholdPx);
}

TEST_CASE("EditorSettings fromJson de non-object devuelve defaults") {
    const auto s1 = editorSettingsFromJson(nlohmann::json{});
    const auto s2 = editorSettingsFromJson(nlohmann::json("hello"));
    const auto s3 = editorSettingsFromJson(nlohmann::json::array({1, 2, 3}));
    const EditorSettings defaults;
    CHECK(s1.gizmoArmLengthPx == doctest::Approx(defaults.gizmoArmLengthPx));
    CHECK(s2.gizmoArmLengthPx == doctest::Approx(defaults.gizmoArmLengthPx));
    CHECK(s3.gizmoArmLengthPx == doctest::Approx(defaults.gizmoArmLengthPx));
}

TEST_CASE("EditorSettings fromJson clampea ortho_zoom_factor <= 1.0 a 1.05") {
    nlohmann::json j;
    j["ortho_zoom_factor"] = 0.5f;  // wheel mas grueso al alejar — sin sentido
    const auto s = editorSettingsFromJson(j);
    CHECK(s.orthoZoomFactor == doctest::Approx(1.05f));
}

TEST_CASE("EditorSettings fromJson clampea click_drag_threshold_px < 1 a 1") {
    nlohmann::json j;
    j["click_drag_threshold_px"] = 0;
    const auto s = editorSettingsFromJson(j);
    CHECK(s.clickDragThresholdPx == 1);
}

TEST_CASE("EditorSettings fromJson rechaza valores negativos en sizes") {
    nlohmann::json j;
    j["ortho_initial_zoom"]    = -10.0f;  // invalido
    j["gizmo_arm_length_px"]   = 0.0f;    // invalido
    j["gizmo_rotate_ring_px"]  = -5.0f;   // invalido
    const auto s = editorSettingsFromJson(j);
    const EditorSettings defaults;
    CHECK(s.orthoInitialZoom    == doctest::Approx(defaults.orthoInitialZoom));
    CHECK(s.gizmoArmLengthPx    == doctest::Approx(defaults.gizmoArmLengthPx));
    CHECK(s.gizmoRotateRingPx   == doctest::Approx(defaults.gizmoRotateRingPx));
}

TEST_CASE("EditorSettings fromJson back-compat: faltan keys = defaults silencioso") {
    nlohmann::json j;
    j["gizmo_arm_length_px"] = 75.0f;  // solo este field
    // los otros 4 deben quedar en defaults
    const auto s = editorSettingsFromJson(j);
    const EditorSettings defaults;
    CHECK(s.gizmoArmLengthPx     == doctest::Approx(75.0f));
    CHECK(s.orthoInitialZoom     == doctest::Approx(defaults.orthoInitialZoom));
    CHECK(s.orthoZoomFactor      == doctest::Approx(defaults.orthoZoomFactor));
    CHECK(s.gizmoRotateRingPx    == doctest::Approx(defaults.gizmoRotateRingPx));
    CHECK(s.clickDragThresholdPx == defaults.clickDragThresholdPx);
}

TEST_CASE("EditorSettings fromJson ignora keys que el codigo no conoce") {
    nlohmann::json j;
    j["ortho_initial_zoom"] = 48.0f;
    j["unknown_key"]        = "garbage";
    j["future_field"]       = 42;
    const auto s = editorSettingsFromJson(j);
    CHECK(s.orthoInitialZoom == doctest::Approx(48.0f));
    // sin crash, sin warn — forward-compat.
}
