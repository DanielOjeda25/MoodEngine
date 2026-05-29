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

// ---------------------------------------------------------------------------
// F3H22 — inspectorActiveCategory (Inspector con icons laterales).
// ---------------------------------------------------------------------------

TEST_CASE("F3H22: default es 'object'") {
    const EditorSettings defaults;
    CHECK(defaults.inspectorActiveCategory == "object");
}

TEST_CASE("F3H22: roundtrip preserva category id") {
    EditorSettings before;
    before.inspectorActiveCategory = "physics";
    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);
    CHECK(after.inspectorActiveCategory == "physics");
}

TEST_CASE("F3H22: toJson omite category cuando es default 'object'") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK_FALSE(j.contains("inspector_active_category"));
}

TEST_CASE("F3H22: fromJson acepta los 7 IDs validos (sin 'all' tras feedback dev)") {
    const char* validIds[] = {
        "object", "render", "animation", "audio",
        "physics", "gameplay", "environment"
    };
    for (const char* id : validIds) {
        nlohmann::json j;
        j["inspector_active_category"] = id;
        CHECK(editorSettingsFromJson(j).inspectorActiveCategory == std::string(id));
    }
}

TEST_CASE("F3H22: 'all' del stub inicial migra silenciosamente a 'object'") {
    nlohmann::json j;
    j["inspector_active_category"] = "all";
    CHECK(editorSettingsFromJson(j).inspectorActiveCategory == "object");
}

TEST_CASE("F3H22: fromJson ID desconocido cae a default 'object'") {
    nlohmann::json j;
    j["inspector_active_category"] = "unknown_category";
    CHECK(editorSettingsFromJson(j).inspectorActiveCategory == "object");
}

TEST_CASE("F3H22: fromJson type incorrecto (int) cae a default 'object'") {
    nlohmann::json j;
    j["inspector_active_category"] = 42;  // no es string
    CHECK(editorSettingsFromJson(j).inspectorActiveCategory == "object");
}

// =====================================================================
// F3H23: stats overlay (7 bools) + profiler frame count
// =====================================================================

TEST_CASE("F3H23: StatsOverlay defaults — FPS/Draws/Tris ON, resto OFF") {
    EditorSettings s;
    CHECK(s.statsOverlay.showFps == true);
    CHECK(s.statsOverlay.showDrawcalls == true);
    CHECK(s.statsOverlay.showTris == true);
    CHECK(s.statsOverlay.showMemGpu == false);
    CHECK(s.statsOverlay.showMemCpu == false);
    CHECK(s.statsOverlay.showLights == false);
    CHECK(s.statsOverlay.showEntities == false);
    CHECK(s.statsOverlay.anyEnabled() == true);
    CHECK(s.profilerFrameCount == 240);
}

TEST_CASE("F3H23: toJson omite stats_overlay si todo igual al default") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK(j.empty());  // ningun field cambio
}

TEST_CASE("F3H23: toJson incluye stats_overlay solo con fields que difieren") {
    EditorSettings s;
    s.statsOverlay.showEntities = true;  // ON (default OFF)
    const auto j = editorSettingsToJson(s);
    REQUIRE(j.contains("stats_overlay"));
    const auto& so = j.at("stats_overlay");
    CHECK(so.size() == 1u);
    CHECK(so.at("show_entities") == true);
    // FPS/Draws/Tris siguen en default ON → NO se escriben.
    CHECK_FALSE(so.contains("show_fps"));
    CHECK_FALSE(so.contains("show_drawcalls"));
}

TEST_CASE("F3H23: StatsOverlay roundtrip de los 7 flags") {
    EditorSettings before;
    before.statsOverlay.showFps       = false;  // OFF (default ON)
    before.statsOverlay.showDrawcalls = false;
    before.statsOverlay.showTris      = false;
    before.statsOverlay.showMemGpu    = true;
    before.statsOverlay.showMemCpu    = true;
    before.statsOverlay.showLights    = true;
    before.statsOverlay.showEntities  = true;

    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);

    CHECK(after.statsOverlay.showFps       == false);
    CHECK(after.statsOverlay.showDrawcalls == false);
    CHECK(after.statsOverlay.showTris      == false);
    CHECK(after.statsOverlay.showMemGpu    == true);
    CHECK(after.statsOverlay.showMemCpu    == true);
    CHECK(after.statsOverlay.showLights    == true);
    CHECK(after.statsOverlay.showEntities  == true);
}

TEST_CASE("F3H23: anyEnabled() devuelve false con todos los toggles OFF") {
    EditorSettings s;
    s.statsOverlay.showFps = false;
    s.statsOverlay.showDrawcalls = false;
    s.statsOverlay.showTris = false;
    CHECK(s.statsOverlay.anyEnabled() == false);
}

TEST_CASE("F3H23: profilerFrameCount toJson solo si difiere del default") {
    EditorSettings s;
    s.profilerFrameCount = 600;
    const auto j = editorSettingsToJson(s);
    REQUIRE(j.contains("profiler_frame_count"));
    CHECK(j.at("profiler_frame_count") == 600);
}

TEST_CASE("F3H23: profilerFrameCount sanitize clamp [60, 1200]") {
    {
        nlohmann::json j;
        j["profiler_frame_count"] = 30;  // por debajo
        CHECK(editorSettingsFromJson(j).profilerFrameCount == 60);
    }
    {
        nlohmann::json j;
        j["profiler_frame_count"] = 5000;  // por encima
        CHECK(editorSettingsFromJson(j).profilerFrameCount == 1200);
    }
    {
        nlohmann::json j;
        j["profiler_frame_count"] = 600;  // valido
        CHECK(editorSettingsFromJson(j).profilerFrameCount == 600);
    }
}

TEST_CASE("F3H23: stats_overlay fromJson ignora claves de tipo equivocado") {
    nlohmann::json j;
    j["stats_overlay"]["show_fps"] = "true";  // string, no bool
    const auto s = editorSettingsFromJson(j);
    // Debe quedar en default (true) porque la clave es invalida.
    CHECK(s.statsOverlay.showFps == true);
}

// =====================================================================
// F3H24: toasts settings (enabled + lifetime ms)
// =====================================================================

TEST_CASE("F3H24: Toasts defaults — enabled ON, lifetime 3000 ms") {
    EditorSettings s;
    CHECK(s.toastsEnabled == true);
    CHECK(s.toastsLifetimeMs == 3000);
}

TEST_CASE("F3H24: toJson omite toasts si todo igual al default") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK_FALSE(j.contains("toasts_enabled"));
    CHECK_FALSE(j.contains("toasts_lifetime_ms"));
}

TEST_CASE("F3H24: toJson incluye solo los toasts fields que difieren") {
    EditorSettings s;
    s.toastsEnabled = false;
    const auto j = editorSettingsToJson(s);
    REQUIRE(j.contains("toasts_enabled"));
    CHECK(j.at("toasts_enabled") == false);
    CHECK_FALSE(j.contains("toasts_lifetime_ms"));
}

TEST_CASE("F3H24: Toasts roundtrip enabled + lifetime") {
    EditorSettings before;
    before.toastsEnabled    = false;
    before.toastsLifetimeMs = 5000;
    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);
    CHECK(after.toastsEnabled    == false);
    CHECK(after.toastsLifetimeMs == 5000);
}

TEST_CASE("F3H24: toastsLifetimeMs sanitize clamp [500, 10000]") {
    {
        nlohmann::json j;
        j["toasts_lifetime_ms"] = 100;  // debajo
        CHECK(editorSettingsFromJson(j).toastsLifetimeMs == 500);
    }
    {
        nlohmann::json j;
        j["toasts_lifetime_ms"] = 50000;  // encima
        CHECK(editorSettingsFromJson(j).toastsLifetimeMs == 10000);
    }
    {
        nlohmann::json j;
        j["toasts_lifetime_ms"] = 2500;  // valido
        CHECK(editorSettingsFromJson(j).toastsLifetimeMs == 2500);
    }
}

TEST_CASE("F3H24: toasts_enabled fromJson ignora tipo no-bool") {
    nlohmann::json j;
    j["toasts_enabled"] = "true";  // string
    CHECK(editorSettingsFromJson(j).toastsEnabled == true);  // default
}

// =====================================================================
// F3H25: autosave settings (enabled + interval min)
// =====================================================================

TEST_CASE("F3H25: Autosave defaults — enabled ON, interval 5 min") {
    EditorSettings s;
    CHECK(s.autosaveEnabled == true);
    CHECK(s.autosaveIntervalMin == 5);
}

TEST_CASE("F3H25: toJson omite autosave si todo igual al default") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK_FALSE(j.contains("autosave_enabled"));
    CHECK_FALSE(j.contains("autosave_interval_min"));
}

TEST_CASE("F3H25: Autosave roundtrip enabled + interval") {
    EditorSettings before;
    before.autosaveEnabled     = false;
    before.autosaveIntervalMin = 15;
    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);
    CHECK(after.autosaveEnabled     == false);
    CHECK(after.autosaveIntervalMin == 15);
}

TEST_CASE("F3H25: autosaveIntervalMin sanitize clamp [1, 60]") {
    {
        nlohmann::json j;
        j["autosave_interval_min"] = 0;  // debajo
        CHECK(editorSettingsFromJson(j).autosaveIntervalMin == 1);
    }
    {
        nlohmann::json j;
        j["autosave_interval_min"] = 999;  // encima
        CHECK(editorSettingsFromJson(j).autosaveIntervalMin == 60);
    }
    {
        nlohmann::json j;
        j["autosave_interval_min"] = 10;  // valido
        CHECK(editorSettingsFromJson(j).autosaveIntervalMin == 10);
    }
}

// =====================================================================
// F3H29: camera limits (far plane + max orbit radius)
// =====================================================================

TEST_CASE("F3H29: Camera limits defaults — far 1000, max orbit 500") {
    EditorSettings s;
    CHECK(s.editorCameraFarPlane == doctest::Approx(1000.0f));
    CHECK(s.editorCameraMaxOrbitRadius == doctest::Approx(500.0f));
}

TEST_CASE("F3H29: toJson omite camera limits si todo igual al default") {
    EditorSettings s;
    const auto j = editorSettingsToJson(s);
    CHECK_FALSE(j.contains("editor_camera_far_plane"));
    CHECK_FALSE(j.contains("editor_camera_max_orbit_radius"));
}

TEST_CASE("F3H29: Camera limits roundtrip") {
    EditorSettings before;
    before.editorCameraFarPlane = 5000.0f;
    before.editorCameraMaxOrbitRadius = 2500.0f;
    const auto j = editorSettingsToJson(before);
    const auto after = editorSettingsFromJson(j);
    CHECK(after.editorCameraFarPlane == doctest::Approx(5000.0f));
    CHECK(after.editorCameraMaxOrbitRadius == doctest::Approx(2500.0f));
}

TEST_CASE("F3H29: editorCameraFarPlane sanitize clamp [100, 100000]") {
    {
        nlohmann::json j;
        j["editor_camera_far_plane"] = 10.0f;  // muy bajo
        CHECK(editorSettingsFromJson(j).editorCameraFarPlane == doctest::Approx(100.0f));
    }
    {
        nlohmann::json j;
        j["editor_camera_far_plane"] = 1000000.0f;  // muy alto
        CHECK(editorSettingsFromJson(j).editorCameraFarPlane == doctest::Approx(100000.0f));
    }
    {
        nlohmann::json j;
        j["editor_camera_far_plane"] = 3000.0f;  // valido
        CHECK(editorSettingsFromJson(j).editorCameraFarPlane == doctest::Approx(3000.0f));
    }
}

TEST_CASE("F3H29: editorCameraMaxOrbitRadius sanitize clamp [10, 50000]") {
    {
        nlohmann::json j;
        j["editor_camera_max_orbit_radius"] = 1.0f;  // muy bajo
        CHECK(editorSettingsFromJson(j).editorCameraMaxOrbitRadius == doctest::Approx(10.0f));
    }
    {
        nlohmann::json j;
        j["editor_camera_max_orbit_radius"] = 999999.0f;  // muy alto
        CHECK(editorSettingsFromJson(j).editorCameraMaxOrbitRadius == doctest::Approx(50000.0f));
    }
    {
        nlohmann::json j;
        j["editor_camera_max_orbit_radius"] = 2000.0f;  // valido
        CHECK(editorSettingsFromJson(j).editorCameraMaxOrbitRadius == doctest::Approx(2000.0f));
    }
}
