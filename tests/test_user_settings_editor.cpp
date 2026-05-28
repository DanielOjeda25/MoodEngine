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

// ---------------------------------------------------------------------------
// F3H21 — viewportRenderMode + smoothViewEnabled + smoothViewDurationMs.
// ---------------------------------------------------------------------------

TEST_CASE("F3H21: defaults son MaterialPreview + lerp ON 200ms") {
    const EditorSettings defaults;
    CHECK(defaults.viewportRenderMode == UserSettings::ViewportRenderMode::MaterialPreview);
    CHECK(defaults.smoothViewEnabled == true);
    CHECK(defaults.smoothViewDurationMs == 200);
}

TEST_CASE("F3H21: roundtrip preserva los 3 fields nuevos") {
    EditorSettings before;
    before.viewportRenderMode    = UserSettings::ViewportRenderMode::Wireframe;
    before.smoothViewEnabled     = false;
    before.smoothViewDurationMs  = 350;

    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);

    CHECK(after.viewportRenderMode   == UserSettings::ViewportRenderMode::Wireframe);
    CHECK(after.smoothViewEnabled    == false);
    CHECK(after.smoothViewDurationMs == 350);
}

TEST_CASE("F3H21: toJson omite los 3 fields cuando son default") {
    EditorSettings s;  // todos defaults
    const auto j = editorSettingsToJson(s);
    CHECK_FALSE(j.contains("viewport_render_mode"));
    CHECK_FALSE(j.contains("smooth_view_enabled"));
    CHECK_FALSE(j.contains("smooth_view_duration_ms"));
}

TEST_CASE("F3H21: fromJson valor invalido del enum (negativo) cae a default") {
    nlohmann::json j;
    j["viewport_render_mode"] = -1;  // fuera del rango 0-3
    const auto s = editorSettingsFromJson(j);
    CHECK(s.viewportRenderMode == UserSettings::ViewportRenderMode::MaterialPreview);
}

TEST_CASE("F3H21: fromJson valor invalido del enum (>3) cae a default") {
    nlohmann::json j;
    j["viewport_render_mode"] = 99;
    const auto s = editorSettingsFromJson(j);
    CHECK(s.viewportRenderMode == UserSettings::ViewportRenderMode::MaterialPreview);
}

TEST_CASE("F3H21: fromJson clampea smooth_view_duration_ms fuera de rango") {
    nlohmann::json j1, j2;
    j1["smooth_view_duration_ms"] = -50;
    j2["smooth_view_duration_ms"] = 5000;
    CHECK(editorSettingsFromJson(j1).smoothViewDurationMs == 0);
    CHECK(editorSettingsFromJson(j2).smoothViewDurationMs == 1000);
}

TEST_CASE("F3H21: fromJson acepta los 4 valores validos del enum") {
    nlohmann::json j;
    j["viewport_render_mode"] = 0;
    CHECK(editorSettingsFromJson(j).viewportRenderMode == UserSettings::ViewportRenderMode::Wireframe);
    j["viewport_render_mode"] = 1;
    CHECK(editorSettingsFromJson(j).viewportRenderMode == UserSettings::ViewportRenderMode::Solid);
    j["viewport_render_mode"] = 2;
    CHECK(editorSettingsFromJson(j).viewportRenderMode == UserSettings::ViewportRenderMode::MaterialPreview);
    j["viewport_render_mode"] = 3;
    CHECK(editorSettingsFromJson(j).viewportRenderMode == UserSettings::ViewportRenderMode::Rendered);
}
