#include "editor/panels/project/ProjectSettingsPanel.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/project/ProjectSettings.h"
#include "engine/scene/serialization/ProjectSerializer.h"  // Project struct

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Presets de Target FPS (estilo Unity Quality > Target Frame Rate). Si
// el .moodproj trae un valor que no esta en esta lista (caso edge: dev
// edito el JSON a mano), se prepende como una entry extra "Personalizado"
// para no perder el valor — el dev puede seguir editandolo solo via JSON
// hasta que aparezca un control de input numerico en hitos siguientes.
constexpr int kFpsPresets[]   = {30, 60, 120, 144};
constexpr int kFpsPresetCount = 4;
const char*   kFpsLabels[]    = {"30 FPS", "60 FPS", "120 FPS", "144 FPS"};

int findPresetIndex(int targetFps) {
    for (int i = 0; i < kFpsPresetCount; ++i) {
        if (kFpsPresets[i] == targetFps) return i;
    }
    return -1;  // no es un preset estandar
}

// Layout estilo Unity: label a la izquierda con ancho fijo, control a
// la derecha. Hint debajo en color disabled.
constexpr float kLabelColumnWidth = 160.0f;
constexpr float kControlWidth     = 200.0f;

} // namespace

void ProjectSettingsPanel::onImGuiRender() {
    if (!visible) return;

    // Ventana flotante centrada + tamano fijo, no dockeable, sin
    // resize/collapse — estilo Unity Project Settings.
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

    Project* project = (m_ui != nullptr) ? m_ui->currentProject() : nullptr;
    if (project == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.project_settings.no_project").c_str());
        ImGui::End();
        return;
    }

    ImGui::Spacing();
    drawPerformanceSection(project->settings);

    ImGui::End();
}

void ProjectSettingsPanel::drawPerformanceSection(ProjectSettings& settings) {
    ImGui::SeparatorText(I18n::T("editor.project_settings.section.performance").c_str());
    ImGui::Spacing();
    ImGui::Indent();

    // === Target FPS (Combo de presets) ===
    int currentIdx = findPresetIndex(settings.targetFps);

    // Si el valor cargado no es un preset, prepender una entry extra al
    // combo para mostrarlo sin perderlo. El i18n `target_fps.custom_label`
    // usa interpolacion estilo fmt (`{}`).
    std::string customLabel;
    const bool isCustom = (currentIdx < 0);
    const char* labels[kFpsPresetCount + 1];
    int comboCount = kFpsPresetCount;
    if (isCustom) {
        customLabel = I18n::T(
            "editor.project_settings.target_fps.custom_label",
            settings.targetFps);
        labels[0] = customLabel.c_str();
        for (int i = 0; i < kFpsPresetCount; ++i) labels[i + 1] = kFpsLabels[i];
        comboCount = kFpsPresetCount + 1;
        currentIdx = 0;  // entry custom en posicion 0
    } else {
        for (int i = 0; i < kFpsPresetCount; ++i) labels[i] = kFpsLabels[i];
    }

    ImGui::TextUnformatted(I18n::T("editor.project_settings.target_fps").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);

    if (ImGui::Combo("##target_fps", &currentIdx, labels, comboCount)) {
        // Mapear el index seleccionado al int real. Si habia entry Custom
        // en index 0 y el dev eligio un preset, sumamos -1 para skipearla.
        int newVal = settings.targetFps;
        if (isCustom) {
            if (currentIdx == 0) {
                newVal = settings.targetFps;  // re-seleccionar Custom = no-op
            } else {
                newVal = kFpsPresets[currentIdx - 1];
            }
        } else {
            newVal = kFpsPresets[currentIdx];
        }
        if (newVal != settings.targetFps) {
            settings.targetFps = newVal;
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.project_settings.target_fps_hint").c_str());

    ImGui::Unindent();
}

} // namespace Mood
