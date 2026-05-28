#include "core/UserSettings.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace Mood::UserSettings {

namespace {

I18n::Language s_language = I18n::Language::Spanish;
std::string s_theme = "dark";  // F2H76: default dark
EditorSettings s_editor{};     // F3H7: editor prefs (default-constructed)
std::filesystem::path s_path;

std::filesystem::path computePath() {
    // Windows: %APPDATA% es la convencion (`C:\Users\<name>\AppData\Roaming`).
    // Si no existe la env var (caso raro), cae al cwd.
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr && *appdata != '\0') {
        return std::filesystem::path(appdata) / "MoodEngine" / "settings.json";
    }
    return std::filesystem::current_path() / "moodengine_settings.json";
}

} // namespace

void init() {
    s_path = computePath();
    s_language = I18n::Language::Spanish;  // default si no hay archivo
    s_theme = "dark";
    s_editor = EditorSettings{};

    std::ifstream in(s_path);
    if (!in.is_open()) {
        Log::engine()->info("[settings] no existe '{}' — usando defaults",
                              s_path.generic_string());
        return;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->warn("[settings] parse error en '{}': {} — usando defaults",
                              s_path.generic_string(), e.what());
        return;
    }

    if (j.contains("language") && j["language"].is_string()) {
        s_language = I18n::languageFromCode(j["language"].get<std::string>());
    }
    if (j.contains("theme") && j["theme"].is_string()) {
        s_theme = j["theme"].get<std::string>();
    }
    if (j.contains("editor")) {
        s_editor = editorSettingsFromJson(j.at("editor"));
    }
    Log::engine()->info("[settings] cargado '{}' (language={}, theme={})",
                         s_path.generic_string(),
                         I18n::languageCode(s_language), s_theme);
}

void shutdown() {
    // No-op por ahora; placeholder para simetria con init().
}

bool save() {
    if (s_path.empty()) s_path = computePath();
    std::error_code ec;
    std::filesystem::create_directories(s_path.parent_path(), ec);
    if (ec) {
        Log::engine()->warn("[settings] no se pudo crear dir '{}': {}",
                              s_path.parent_path().generic_string(), ec.message());
        return false;
    }
    std::ofstream out(s_path);
    if (!out.is_open()) {
        Log::engine()->warn("[settings] no se pudo abrir '{}' para escritura",
                              s_path.generic_string());
        return false;
    }
    nlohmann::json j;
    j["language"] = I18n::languageCode(s_language);
    j["theme"]    = s_theme;
    // F3H7: solo persistir "editor" si alguno de los fields difiere del
    // default (settings.json limpio cuando todo es default).
    const auto editorJson = editorSettingsToJson(s_editor);
    if (!editorJson.empty()) j["editor"] = editorJson;
    out << j.dump(2) << "\n";
    Log::engine()->info("[settings] guardado '{}' (language={}, theme={})",
                         s_path.generic_string(),
                         I18n::languageCode(s_language), s_theme);
    return true;
}

I18n::Language language() { return s_language; }

void setLanguage(I18n::Language lang) { s_language = lang; }

const std::string& theme() { return s_theme; }

void setTheme(const std::string& id) { s_theme = id; }

const EditorSettings& editor() { return s_editor; }

void setEditor(const EditorSettings& s) { s_editor = s; }

std::filesystem::path settingsPath() {
    if (s_path.empty()) return computePath();
    return s_path;
}

// F3H7: solo escribe fields que difieren del default — mantiene
// settings.json minimal para devs que no tocan el editor.
nlohmann::json editorSettingsToJson(const EditorSettings& s) {
    nlohmann::json j = nlohmann::json::object();
    const EditorSettings defaults;
    if (s.orthoInitialZoom        != defaults.orthoInitialZoom)        j["ortho_initial_zoom"]        = s.orthoInitialZoom;
    if (s.orthoZoomFactor         != defaults.orthoZoomFactor)         j["ortho_zoom_factor"]         = s.orthoZoomFactor;
    if (s.gizmoArmLengthPx        != defaults.gizmoArmLengthPx)        j["gizmo_arm_length_px"]       = s.gizmoArmLengthPx;
    if (s.gizmoRotateRingPx       != defaults.gizmoRotateRingPx)       j["gizmo_rotate_ring_px"]      = s.gizmoRotateRingPx;
    if (s.clickDragThresholdPx    != defaults.clickDragThresholdPx)    j["click_drag_threshold_px"]   = s.clickDragThresholdPx;
    if (s.thumbnailResolution     != defaults.thumbnailResolution)     j["thumbnail_resolution"]      = s.thumbnailResolution;
    if (s.hoverPreviewDelayMs     != defaults.hoverPreviewDelayMs)     j["hover_preview_delay_ms"]    = s.hoverPreviewDelayMs;
    if (s.viewportRenderMode      != defaults.viewportRenderMode)      j["viewport_render_mode"]      = static_cast<int>(s.viewportRenderMode);
    if (s.smoothViewEnabled       != defaults.smoothViewEnabled)       j["smooth_view_enabled"]       = s.smoothViewEnabled;
    if (s.smoothViewDurationMs    != defaults.smoothViewDurationMs)    j["smooth_view_duration_ms"]   = s.smoothViewDurationMs;
    if (s.inspectorActiveCategory != defaults.inspectorActiveCategory) j["inspector_active_category"] = s.inspectorActiveCategory;

    // F3H23: stats overlay (subobject opcional — solo si difiere del default).
    const auto& so = s.statsOverlay;
    const auto& soDef = defaults.statsOverlay;
    nlohmann::json soJson = nlohmann::json::object();
    if (so.showFps       != soDef.showFps)       soJson["show_fps"]       = so.showFps;
    if (so.showDrawcalls != soDef.showDrawcalls) soJson["show_drawcalls"] = so.showDrawcalls;
    if (so.showTris      != soDef.showTris)      soJson["show_tris"]      = so.showTris;
    if (so.showMemGpu    != soDef.showMemGpu)    soJson["show_mem_gpu"]   = so.showMemGpu;
    if (so.showMemCpu    != soDef.showMemCpu)    soJson["show_mem_cpu"]   = so.showMemCpu;
    if (so.showLights    != soDef.showLights)    soJson["show_lights"]    = so.showLights;
    if (so.showEntities  != soDef.showEntities)  soJson["show_entities"]  = so.showEntities;
    if (!soJson.empty()) j["stats_overlay"] = std::move(soJson);

    if (s.profilerFrameCount != defaults.profilerFrameCount) j["profiler_frame_count"] = s.profilerFrameCount;

    // F3H24
    if (s.toastsEnabled    != defaults.toastsEnabled)    j["toasts_enabled"]     = s.toastsEnabled;
    if (s.toastsLifetimeMs != defaults.toastsLifetimeMs) j["toasts_lifetime_ms"] = s.toastsLifetimeMs;

    // F3H25
    if (s.autosaveEnabled     != defaults.autosaveEnabled)     j["autosave_enabled"]      = s.autosaveEnabled;
    if (s.autosaveIntervalMin != defaults.autosaveIntervalMin) j["autosave_interval_min"] = s.autosaveIntervalMin;
    return j;
}

// F3H7: lee + sanitize. Cualquier campo malformado/fuera de rango
// cae al default (sin log — el dev edito a mano el settings.json y
// quedo invalido, mostrarle un editor crasheado no ayuda).
EditorSettings editorSettingsFromJson(const nlohmann::json& j) {
    EditorSettings s;
    if (!j.is_object()) return s;

    if (j.contains("ortho_initial_zoom") && j.at("ortho_initial_zoom").is_number()) {
        const f32 v = j.at("ortho_initial_zoom").get<f32>();
        if (v > 0.0f) s.orthoInitialZoom = v;
    }
    if (j.contains("ortho_zoom_factor") && j.at("ortho_zoom_factor").is_number()) {
        const f32 v = j.at("ortho_zoom_factor").get<f32>();
        // factor <= 1.0 hace que el zoom no cambie o vaya al reves; clamp a 1.05.
        s.orthoZoomFactor = std::max(v, 1.05f);
    }
    if (j.contains("gizmo_arm_length_px") && j.at("gizmo_arm_length_px").is_number()) {
        const f32 v = j.at("gizmo_arm_length_px").get<f32>();
        if (v > 0.0f) s.gizmoArmLengthPx = v;
    }
    if (j.contains("gizmo_rotate_ring_px") && j.at("gizmo_rotate_ring_px").is_number()) {
        const f32 v = j.at("gizmo_rotate_ring_px").get<f32>();
        if (v > 0.0f) s.gizmoRotateRingPx = v;
    }
    if (j.contains("click_drag_threshold_px") && j.at("click_drag_threshold_px").is_number_integer()) {
        const int v = j.at("click_drag_threshold_px").get<int>();
        s.clickDragThresholdPx = std::max(v, 1);
    }
    if (j.contains("thumbnail_resolution") && j.at("thumbnail_resolution").is_number_integer()) {
        const int v = j.at("thumbnail_resolution").get<int>();
        s.thumbnailResolution = std::clamp(v, 64, 512);
    }
    if (j.contains("hover_preview_delay_ms") && j.at("hover_preview_delay_ms").is_number_integer()) {
        const int v = j.at("hover_preview_delay_ms").get<int>();
        s.hoverPreviewDelayMs = std::clamp(v, 0, 3000);
    }
    if (j.contains("viewport_render_mode") && j.at("viewport_render_mode").is_number_integer()) {
        const int v = j.at("viewport_render_mode").get<int>();
        // Clamp al rango del enum (0..3). Valor invalido cae a default
        // (MaterialPreview) sin log — mismo patron que el resto.
        if (v >= 0 && v <= 3) {
            s.viewportRenderMode = static_cast<ViewportRenderMode>(v);
        }
    }
    if (j.contains("smooth_view_enabled") && j.at("smooth_view_enabled").is_boolean()) {
        s.smoothViewEnabled = j.at("smooth_view_enabled").get<bool>();
    }
    if (j.contains("smooth_view_duration_ms") && j.at("smooth_view_duration_ms").is_number_integer()) {
        const int v = j.at("smooth_view_duration_ms").get<int>();
        s.smoothViewDurationMs = std::clamp(v, 0, 1000);
    }
    if (j.contains("inspector_active_category") && j.at("inspector_active_category").is_string()) {
        const std::string v = j.at("inspector_active_category").get<std::string>();
        // Sanitize: 7 IDs validos (categorias unicas). El "all" del stub
        // inicial fue eliminado por feedback del dev — el modo legacy
        // confundia. Si el settings.json del dev tiene "all" persistido,
        // migra silenciosamente a "object".
        if (v == "object" || v == "render" || v == "animation" ||
            v == "audio" || v == "physics" || v == "gameplay" ||
            v == "environment") {
            s.inspectorActiveCategory = v;
        }
    }

    // F3H23: stats overlay subobject. Cada bool independiente — flag
    // ausente queda en su default (no all-or-nothing).
    if (j.contains("stats_overlay") && j.at("stats_overlay").is_object()) {
        const auto& so = j.at("stats_overlay");
        auto readBool = [&](const char* key, bool& out) {
            if (so.contains(key) && so.at(key).is_boolean()) out = so.at(key).get<bool>();
        };
        readBool("show_fps",       s.statsOverlay.showFps);
        readBool("show_drawcalls", s.statsOverlay.showDrawcalls);
        readBool("show_tris",      s.statsOverlay.showTris);
        readBool("show_mem_gpu",   s.statsOverlay.showMemGpu);
        readBool("show_mem_cpu",   s.statsOverlay.showMemCpu);
        readBool("show_lights",    s.statsOverlay.showLights);
        readBool("show_entities",  s.statsOverlay.showEntities);
    }

    if (j.contains("profiler_frame_count") && j.at("profiler_frame_count").is_number_integer()) {
        const int v = j.at("profiler_frame_count").get<int>();
        s.profilerFrameCount = std::clamp(v, 60, 1200);
    }

    // F3H24
    if (j.contains("toasts_enabled") && j.at("toasts_enabled").is_boolean()) {
        s.toastsEnabled = j.at("toasts_enabled").get<bool>();
    }
    if (j.contains("toasts_lifetime_ms") && j.at("toasts_lifetime_ms").is_number_integer()) {
        const int v = j.at("toasts_lifetime_ms").get<int>();
        s.toastsLifetimeMs = std::clamp(v, 500, 10000);
    }

    // F3H25
    if (j.contains("autosave_enabled") && j.at("autosave_enabled").is_boolean()) {
        s.autosaveEnabled = j.at("autosave_enabled").get<bool>();
    }
    if (j.contains("autosave_interval_min") && j.at("autosave_interval_min").is_number_integer()) {
        const int v = j.at("autosave_interval_min").get<int>();
        s.autosaveIntervalMin = std::clamp(v, 1, 60);
    }
    return s;
}

} // namespace Mood::UserSettings
