#include "editor/panels/project/UserPreferencesPanel.h"

#include "core/Toasts.h"  // F3H24: toast al cerrar con cambios
#include "core/UserSettings.h"
#include "core/i18n/I18n.h"
#include "editor/ui/EditorThemes.h"
#include "editor/ui/IconsFontAwesome6.h"  // F3H7: ICON_FA_ROTATE_LEFT

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Mismo layout estilo Unity que ProjectSettingsPanel — label izquierda con
// ancho fijo, control derecha.
// F3H23: subido de 160 a 240 — labels largos del editor en español
// ("Tamaño gizmo (mover/escalar)", "Retraso preview al pasar el cursor")
// pisaban la columna del slider con el valor anterior.
constexpr float kLabelColumnWidth = 240.0f;
constexpr float kControlWidth     = 200.0f;
// F3H26: ancho de la sidebar de categorías. Calibrado para los labels
// más largos en español ("Notificaciones") sin overflow.
constexpr float kSidebarWidth     = 150.0f;

// F3H7: mismo helper que ProjectSettingsPanel — boton ↺ chiquito a la
// derecha del control que solo aparece si el valor difiere del default.
// Reusa la key i18n `editor.common.reset_default`.
template <typename T>
bool resetButton(const char* widgetIdSuffix, T& value, T defaultValue) {
    if (value == defaultValue) return false;

    ImGui::SameLine();
    const std::string btnLabel =
        std::string(ICON_FA_ROTATE_LEFT) + "##reset_" + widgetIdSuffix;
    bool clicked = false;
    if (ImGui::SmallButton(btnLabel.c_str())) {
        value = defaultValue;
        clicked = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.common.reset_default").c_str());
    }
    return clicked;
}

} // namespace

void UserPreferencesPanel::onImGuiRender() {
    // F3H24: detectar transición visible: true→false (panel cerrado).
    // Si hubo cambios entre apertura y cierre, emitir 1 toast resumen.
    if (m_wasVisibleLastFrame && !visible && m_changedSinceOpen) {
        Toasts::pushSuccess(I18n::T("editor.toast.preferences_saved"));
    }
    m_wasVisibleLastFrame = visible;
    if (!visible) {
        m_changedSinceOpen = false;  // reset al estar cerrado
        return;
    }

    // F3H26: ventana más grande para acomodar sidebar + content. Antes
    // 540x360 con TabBar horizontal; ahora 720x480 con split vertical.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720.0f, 480.0f), ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking;

    if (!ImGui::Begin(name(), &visible, kFlags)) {
        ImGui::End();
        return;
    }

    // F3H26: layout split = sidebar (categorías) + content (vista activa).
    // Estilo Blender Preferences. La sidebar es BeginChild con border;
    // el content es BeginChild sin border + scroll para que cualquier
    // sección que exceda el alto se navegue con la rueda.
    ImGui::BeginChild("##user_pref_sidebar",
                      ImVec2(kSidebarWidth, 0.0f),
                      /*border=*/true);
    drawSidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##user_pref_content",
                      ImVec2(0.0f, 0.0f),
                      /*border=*/false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::Indent();
    switch (m_activeCategory) {
        case Category::General:
            drawGeneral();
            break;
        case Category::Viewport:
        case Category::Assets:
        case Category::Performance:
        case Category::Notifications: {
            UserSettings::EditorSettings cfg = UserSettings::editor();
            const UserSettings::EditorSettings defaults;
            bool dirty   = false;
            bool saveNow = false;
            if (m_activeCategory == Category::Viewport)      drawViewport(cfg, defaults, dirty, saveNow);
            if (m_activeCategory == Category::Assets)        drawAssets(cfg, defaults, dirty, saveNow);
            if (m_activeCategory == Category::Performance)   drawPerformance(cfg, defaults, dirty, saveNow);
            if (m_activeCategory == Category::Notifications) drawNotifications(cfg, defaults, dirty, saveNow);
            if (dirty)   UserSettings::setEditor(cfg);
            if (saveNow) UserSettings::save();
            if (saveNow) m_changedSinceOpen = true;
            break;
        }
    }
    ImGui::Unindent();
    ImGui::EndChild();

    ImGui::End();
}

void UserPreferencesPanel::drawSidebar() {
    struct Entry { Category cat; const char* labelKey; };
    static const Entry kEntries[] = {
        {Category::General,       "editor.user_preferences.category.general"},
        {Category::Viewport,      "editor.user_preferences.category.viewport"},
        {Category::Assets,        "editor.user_preferences.category.assets"},
        {Category::Performance,   "editor.user_preferences.category.performance"},
        {Category::Notifications, "editor.user_preferences.category.notifications"},
    };
    ImGui::Spacing();
    for (const auto& e : kEntries) {
        const std::string label = I18n::T(e.labelKey);
        const bool selected = (m_activeCategory == e.cat);
        if (ImGui::Selectable(label.c_str(), selected,
                              ImGuiSelectableFlags_None,
                              ImVec2(0.0f, 24.0f))) {
            m_activeCategory = e.cat;
        }
    }
}

void UserPreferencesPanel::drawGeneral() {
    // === Tema ===
    const auto& themes = EditorThemes::available();
    const std::string& curThemeId = UserSettings::theme();
    int curThemeIdx = 0;
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        if (themes[i].id == curThemeId) { curThemeIdx = i; break; }
    }
    const std::string curThemeLabel = I18n::T(themes[curThemeIdx].i18nKey);

    ImGui::TextUnformatted(I18n::T("editor.user_preferences.theme").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::BeginCombo("##user_pref_theme", curThemeLabel.c_str())) {
        for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
            const bool sel = (i == curThemeIdx);
            if (ImGui::Selectable(I18n::T(themes[i].i18nKey).c_str(), sel)) {
                UserSettings::setTheme(themes[i].id);
                EditorThemes::apply(themes[i].id);  // preview live
                UserSettings::save();
                m_changedSinceOpen = true;  // F3H24
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // === Idioma ===
    const auto curLang = I18n::currentLanguage();
    const std::string curLangLabel = I18n::T(
        curLang == I18n::Language::English
            ? "editor.user_preferences.language.english"
            : "editor.user_preferences.language.spanish");

    ImGui::TextUnformatted(I18n::T("editor.user_preferences.language").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::BeginCombo("##user_pref_lang", curLangLabel.c_str())) {
        const bool isEs = (curLang == I18n::Language::Spanish);
        if (ImGui::Selectable(
                I18n::T("editor.user_preferences.language.spanish").c_str(), isEs)) {
            if (I18n::setLanguage(I18n::Language::Spanish)) {
                UserSettings::setLanguage(I18n::Language::Spanish);
                UserSettings::save();
                m_changedSinceOpen = true;  // F3H24
            }
        }
        if (isEs) ImGui::SetItemDefaultFocus();
        const bool isEn = (curLang == I18n::Language::English);
        if (ImGui::Selectable(
                I18n::T("editor.user_preferences.language.english").c_str(), isEn)) {
            if (I18n::setLanguage(I18n::Language::English)) {
                UserSettings::setLanguage(I18n::Language::English);
                UserSettings::save();
                m_changedSinceOpen = true;  // F3H24
            }
        }
        if (isEn) ImGui::SetItemDefaultFocus();
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.user_preferences.live_apply_hint").c_str());
}

void UserPreferencesPanel::drawViewport(UserSettings::EditorSettings& cfg,
                                         const UserSettings::EditorSettings& defaults,
                                         bool& dirty, bool& saveNow) {
    auto drawSliderF = [&](const char* labelKey, const char* hintKey,
                            const char* idSuffix,
                            f32& field, f32 fieldDefault,
                            f32 minV, f32 maxV, const char* fmt) {
        ImGui::TextUnformatted(I18n::T(labelKey).c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(hintKey).c_str());
        }
        ImGui::SameLine(kLabelColumnWidth);
        ImGui::SetNextItemWidth(kControlWidth);
        const std::string widgetId = std::string("##user_pref_") + idSuffix;
        if (ImGui::SliderFloat(widgetId.c_str(), &field, minV, maxV, fmt)) {
            dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
        if (resetButton(idSuffix, field, fieldDefault)) {
            dirty = true;
            saveNow = true;
        }
    };

    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.ortho_cam").c_str());
    drawSliderF("editor.user_preferences.editor.ortho_initial_zoom",
                "editor.user_preferences.editor.ortho_initial_zoom_hint",
                "ortho_initial_zoom",
                cfg.orthoInitialZoom, defaults.orthoInitialZoom,
                4.0f, 256.0f, "%.0f");
    drawSliderF("editor.user_preferences.editor.ortho_zoom_factor",
                "editor.user_preferences.editor.ortho_zoom_factor_hint",
                "ortho_zoom_factor",
                cfg.orthoZoomFactor, defaults.orthoZoomFactor,
                1.05f, 1.5f, "%.2fx");

    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.gizmos").c_str());
    drawSliderF("editor.user_preferences.editor.gizmo_arm_length",
                "editor.user_preferences.editor.gizmo_arm_length_hint",
                "gizmo_arm_length",
                cfg.gizmoArmLengthPx, defaults.gizmoArmLengthPx,
                30.0f, 120.0f, "%.0f px");
    drawSliderF("editor.user_preferences.editor.gizmo_rotate_ring",
                "editor.user_preferences.editor.gizmo_rotate_ring_hint",
                "gizmo_rotate_ring",
                cfg.gizmoRotateRingPx, defaults.gizmoRotateRingPx,
                30.0f, 140.0f, "%.0f px");

    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.interaction").c_str());
    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.click_drag_threshold").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.click_drag_threshold_hint").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_click_drag_threshold",
                          &cfg.clickDragThresholdPx, 1, 32, "%d px")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("click_drag_threshold",
                     cfg.clickDragThresholdPx,
                     defaults.clickDragThresholdPx)) {
        dirty = true;
        saveNow = true;
    }

    // F3H29: section "Mundo" — far plane + max orbit radius. Logarithmic
    // porque el rango [100, 100000] / [10, 50000] no se navega bien lineal
    // (el dev quiere granularidad fina en 100-2000 y poca en 10000+).
    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.world").c_str());

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.camera_far_plane").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.camera_far_plane.tooltip").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderFloat("##user_pref_camera_far_plane",
                            &cfg.editorCameraFarPlane, 100.0f, 100000.0f,
                            "%.0f m", ImGuiSliderFlags_Logarithmic)) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("camera_far_plane",
                     cfg.editorCameraFarPlane,
                     defaults.editorCameraFarPlane)) {
        dirty = true;
        saveNow = true;
    }

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.camera_max_orbit_radius").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.camera_max_orbit_radius.tooltip").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderFloat("##user_pref_camera_max_orbit",
                            &cfg.editorCameraMaxOrbitRadius, 10.0f, 50000.0f,
                            "%.0f m", ImGuiSliderFlags_Logarithmic)) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("camera_max_orbit",
                     cfg.editorCameraMaxOrbitRadius,
                     defaults.editorCameraMaxOrbitRadius)) {
        dirty = true;
        saveNow = true;
    }

    // F3H29 polish: hint "Los cambios se aplican al soltar el slider..."
    // eliminado de cada sub-sección (era 4× redundante). El
    // comportamiento es el estándar de cualquier Preferences pane.
}

void UserPreferencesPanel::drawAssets(UserSettings::EditorSettings& cfg,
                                       const UserSettings::EditorSettings& defaults,
                                       bool& dirty, bool& saveNow) {
    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.asset_browser").c_str());

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.thumbnail_resolution").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.thumbnail_resolution_hint").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_thumbnail_resolution",
                          &cfg.thumbnailResolution, 64, 512, "%d px")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("thumbnail_resolution",
                     cfg.thumbnailResolution,
                     defaults.thumbnailResolution)) {
        dirty = true;
        saveNow = true;
    }

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.hover_preview_delay").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.hover_preview_delay_hint").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_hover_preview_delay",
                          &cfg.hoverPreviewDelayMs, 0, 3000, "%d ms")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("hover_preview_delay",
                     cfg.hoverPreviewDelayMs,
                     defaults.hoverPreviewDelayMs)) {
        dirty = true;
        saveNow = true;
    }

    // F3H29 polish: hint "Los cambios se aplican al soltar el slider..."
    // eliminado de cada sub-sección (era 4× redundante). El
    // comportamiento es el estándar de cualquier Preferences pane.
}

void UserPreferencesPanel::drawPerformance(UserSettings::EditorSettings& cfg,
                                            const UserSettings::EditorSettings& defaults,
                                            bool& dirty, bool& saveNow) {
    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.editor.stats_overlay.section").c_str());

    auto statsCheckbox = [&](const char* idSuffix, const char* labelKey,
                              const char* tooltipKey, bool& value, bool defaultVal) {
        const std::string id = std::string("##user_pref_stats_") + idSuffix;
        if (ImGui::Checkbox((I18n::T(labelKey) + id).c_str(), &value)) {
            dirty = true;
            saveNow = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(tooltipKey).c_str());
        }
        if (value != defaultVal) {
            ImGui::SameLine();
            if (resetButton((std::string("stats_") + idSuffix).c_str(),
                              value, defaultVal)) {
                dirty = true;
                saveNow = true;
            }
        }
    };

    statsCheckbox("fps", "editor.user_preferences.editor.stats_overlay.fps",
                   "editor.user_preferences.editor.stats_overlay.fps.tooltip",
                   cfg.statsOverlay.showFps, defaults.statsOverlay.showFps);
    statsCheckbox("drawcalls", "editor.user_preferences.editor.stats_overlay.drawcalls",
                   "editor.user_preferences.editor.stats_overlay.drawcalls.tooltip",
                   cfg.statsOverlay.showDrawcalls, defaults.statsOverlay.showDrawcalls);
    statsCheckbox("tris", "editor.user_preferences.editor.stats_overlay.tris",
                   "editor.user_preferences.editor.stats_overlay.tris.tooltip",
                   cfg.statsOverlay.showTris, defaults.statsOverlay.showTris);
    statsCheckbox("mem_gpu", "editor.user_preferences.editor.stats_overlay.mem_gpu",
                   "editor.user_preferences.editor.stats_overlay.mem_gpu.tooltip",
                   cfg.statsOverlay.showMemGpu, defaults.statsOverlay.showMemGpu);
    statsCheckbox("mem_cpu", "editor.user_preferences.editor.stats_overlay.mem_cpu",
                   "editor.user_preferences.editor.stats_overlay.mem_cpu.tooltip",
                   cfg.statsOverlay.showMemCpu, defaults.statsOverlay.showMemCpu);
    statsCheckbox("lights", "editor.user_preferences.editor.stats_overlay.lights",
                   "editor.user_preferences.editor.stats_overlay.lights.tooltip",
                   cfg.statsOverlay.showLights, defaults.statsOverlay.showLights);
    statsCheckbox("entities", "editor.user_preferences.editor.stats_overlay.entities",
                   "editor.user_preferences.editor.stats_overlay.entities.tooltip",
                   cfg.statsOverlay.showEntities, defaults.statsOverlay.showEntities);

    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.section.profiler").c_str());
    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.profiler_frame_count").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.profiler_frame_count.tooltip").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_profiler_frame_count",
                          &cfg.profilerFrameCount, 60, 1200, "%d frames")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("profiler_frame_count",
                     cfg.profilerFrameCount,
                     defaults.profilerFrameCount)) {
        dirty = true;
        saveNow = true;
    }

    // F3H29 polish: hint "Los cambios se aplican al soltar el slider..."
    // eliminado de cada sub-sección (era 4× redundante). El
    // comportamiento es el estándar de cualquier Preferences pane.
}

void UserPreferencesPanel::drawNotifications(UserSettings::EditorSettings& cfg,
                                              const UserSettings::EditorSettings& defaults,
                                              bool& dirty, bool& saveNow) {
    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.editor.toasts.section").c_str());

    if (ImGui::Checkbox(
            (I18n::T("editor.user_preferences.editor.toasts.enabled") +
             "##user_pref_toasts_enabled").c_str(),
            &cfg.toastsEnabled)) {
        dirty = true;
        saveNow = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.toasts.enabled.tooltip").c_str());
    }

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.toasts.lifetime_ms").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.toasts.lifetime_ms.tooltip").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_toasts_lifetime",
                          &cfg.toastsLifetimeMs, 500, 10000, "%d ms")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("toasts_lifetime",
                     cfg.toastsLifetimeMs,
                     defaults.toastsLifetimeMs)) {
        dirty = true;
        saveNow = true;
    }

    ImGui::SeparatorText(
        I18n::T("editor.user_preferences.editor.autosave.section").c_str());

    if (ImGui::Checkbox(
            (I18n::T("editor.user_preferences.editor.autosave.enabled") +
             "##user_pref_autosave_enabled").c_str(),
            &cfg.autosaveEnabled)) {
        dirty = true;
        saveNow = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.autosave.enabled.tooltip").c_str());
    }

    ImGui::TextUnformatted(
        I18n::T("editor.user_preferences.editor.autosave.interval_min").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.user_preferences.editor.autosave.interval_min.tooltip").c_str());
    }
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::SliderInt("##user_pref_autosave_interval",
                          &cfg.autosaveIntervalMin, 1, 60, "%d min")) {
        dirty = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) saveNow = true;
    if (resetButton("autosave_interval",
                     cfg.autosaveIntervalMin,
                     defaults.autosaveIntervalMin)) {
        dirty = true;
        saveNow = true;
    }

    // F3H29 polish: hint "Los cambios se aplican al soltar el slider..."
    // eliminado de cada sub-sección (era 4× redundante). El
    // comportamiento es el estándar de cualquier Preferences pane.
}

} // namespace Mood
