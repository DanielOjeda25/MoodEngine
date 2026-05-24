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

    // F3H4: TabBar reintroducido (F3H1 polish lo elimino por scope chico
    // con 1 sola seccion; ahora hay 2 con contenido real).
    if (ImGui::BeginTabBar("##project_settings_tabs")) {
        if (ImGui::BeginTabItem(
                I18n::T("editor.project_settings.section.performance").c_str())) {
            ImGui::Spacing();
            drawPerformanceSection(project->settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                I18n::T("editor.project_settings.section.gameplay").c_str())) {
            ImGui::Spacing();
            drawGameplaySection(project->settings);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ProjectSettingsPanel::drawPerformanceSection(ProjectSettings& settings) {
    // F3H4: SeparatorText eliminado — el TabBar ya rotula la seccion.
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

// F3H4: seccion Gameplay (walk/crouch/jump). 4 SliderFloat con tooltip
// hint para cada uno. Cada cambio dispara dirty -> EditorApplication
// llama markDirty() en pumpUiRequests. Las lecturas en PlayerApplication
// y EditorPlayMode leen settings.gameplay live cada frame (no se cachea
// al cargar el proyecto — el dev puede editar y sentir el cambio sin
// reiniciar Play).
void ProjectSettingsPanel::drawGameplaySection(ProjectSettings& settings) {
    ImGui::Indent();

    auto drawSlider = [&](const char* keyLabel,
                          const char* keyHint,
                          const char* widgetId,
                          f32& value,
                          f32 minVal,
                          f32 maxVal,
                          const char* fmt) {
        ImGui::TextUnformatted(I18n::T(keyLabel).c_str());
        ImGui::SameLine(kLabelColumnWidth);
        ImGui::SetNextItemWidth(kControlWidth);
        if (ImGui::SliderFloat(widgetId, &value, minVal, maxVal, fmt)) {
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(keyHint).c_str());
        }
    };

    drawSlider("editor.project_settings.gameplay.walk_speed",
               "editor.project_settings.gameplay.walk_speed_hint",
               "##walk_speed",
               settings.gameplay.walkSpeed, 1.0f, 12.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.crouch_speed",
               "editor.project_settings.gameplay.crouch_speed_hint",
               "##crouch_speed",
               settings.gameplay.crouchSpeed, 0.5f, 6.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.jump_velocity",
               "editor.project_settings.gameplay.jump_velocity_hint",
               "##jump_velocity",
               settings.gameplay.jumpVelocity, 1.0f, 15.0f, "%.1f m/s");

    drawSlider("editor.project_settings.gameplay.jump_cooldown",
               "editor.project_settings.gameplay.jump_cooldown_hint",
               "##jump_cooldown",
               settings.gameplay.jumpCooldownSec, 0.0f, 1.0f, "%.2f s");

    ImGui::Unindent();
}

} // namespace Mood
