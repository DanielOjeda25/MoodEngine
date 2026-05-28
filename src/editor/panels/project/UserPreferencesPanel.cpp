#include "editor/panels/project/UserPreferencesPanel.h"

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
    if (!visible) return;

    // Ventana flotante centrada + tamano fijo, no dockeable, sin resize/
    // collapse — espejo de ProjectSettingsPanel (consistencia UX).
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540.0f, 360.0f), ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking;

    if (!ImGui::Begin(name(), &visible, kFlags)) {
        ImGui::End();
        return;
    }

    ImGui::Spacing();

    // F3H7: TabBar con General (tema + idioma de F3H2) + Editor
    // (sensibilidades nuevas). Mismo patron que ProjectSettingsPanel.
    if (ImGui::BeginTabBar("##user_pref_tabs")) {
        if (ImGui::BeginTabItem(
                I18n::T("editor.user_preferences.tab.general").c_str())) {
            ImGui::Spacing();
            drawGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                I18n::T("editor.user_preferences.tab.editor").c_str())) {
            ImGui::Spacing();
            drawEditorTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void UserPreferencesPanel::drawGeneralTab() {
    ImGui::Indent();

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
            }
        }
        if (isEs) ImGui::SetItemDefaultFocus();
        const bool isEn = (curLang == I18n::Language::English);
        if (ImGui::Selectable(
                I18n::T("editor.user_preferences.language.english").c_str(), isEn)) {
            if (I18n::setLanguage(I18n::Language::English)) {
                UserSettings::setLanguage(I18n::Language::English);
                UserSettings::save();
            }
        }
        if (isEn) ImGui::SetItemDefaultFocus();
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.user_preferences.live_apply_hint").c_str());

    ImGui::Unindent();
}

void UserPreferencesPanel::drawEditorTab() {
    ImGui::Indent();

    // F3H7: trabajamos sobre una copia + flags dirty/saveNow para
    // separar 2 cosas:
    //  - `dirty`: el dev movio algo este frame -> `setEditor(cfg)` para
    //    que el live read en los call-sites refleje el cambio.
    //  - `saveNow`: el dev solto el slider (o clickeo reset) -> `save()`
    //    al disco. Asi el JSON se escribe una vez al soltar, no 60 fps
    //    mientras se arrastra el slider.
    UserSettings::EditorSettings cfg = UserSettings::editor();
    const UserSettings::EditorSettings defaults;
    bool dirty   = false;
    bool saveNow = false;

    // Helper local para SliderFloat + reset button + tooltip.
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
            saveNow = true;  // click es commit instantaneo
        }
    };

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

    ImGui::Spacing();

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

    ImGui::Spacing();

    // Click/drag threshold como int — SliderInt.
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

    ImGui::Spacing();

    // F3H14: thumbnail resolution del Asset Browser (mesh thumbs). Cambiar
    // este valor en vivo recrea el renderer + invalida cache memoria; la
    // cache disco persiste con el size viejo en el filename.
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

    // F3H16: hover preview delay (ms) del Asset Browser. Tiempo que el
    // dev tiene que dejar el cursor quieto sobre un thumb antes de que
    // aparezca el tooltip ampliado.
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

    // F3H23: Stats overlay del viewport (Unity-style bottom bar). Cada
    // checkbox controla un widget independiente — el dev arma su HUD
    // según lo que quiera monitorear. Defaults: FPS + Draws + Tris ON
    // (las 3 más usadas day-to-day); memoria/lights/entities OFF.
    ImGui::Spacing();
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
        // Reset inline a la derecha si el valor difiere del default.
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

    // F3H23: ring buffer size del profiler (frames de historia para
    // avg/min/max + histograma).
    ImGui::Spacing();
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

    if (dirty)   UserSettings::setEditor(cfg);
    if (saveNow) UserSettings::save();

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.user_preferences.editor.live_apply_hint").c_str());

    ImGui::Unindent();
}

} // namespace Mood
