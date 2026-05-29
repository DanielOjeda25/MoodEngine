// F3H28: extraido del antiguo MapEditorTopBar (F3H6 polish). Mismo código,
// distinto home: ahora la categoría "Map Tools" del Inspector lo invoca
// desde su popover. Sin cambios de lógica respecto a la versión previa.

#include "editor/ui/SnapPopoverContent.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/project/ProjectSettings.h"
#include "engine/scene/serialization/ProjectSerializer.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood {

void drawSnapPopoverContent(EditorUI* ui, Project* project) {
    if (project == nullptr) {
        ImGui::TextDisabled("(no project)");
        return;
    }
    SnapSettings& s = project->settings.snap;
    const SnapSettings defaults;

    auto markDirty = [&]() {
        if (ui != nullptr) ui->requestProjectDirty();
    };

    // === Paso actual (acceso rapido) ===
    ImGui::TextUnformatted(I18n::T("editor.map_tools.snap.current_step").c_str());
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(120.0f);

    std::vector<std::string> labels;
    labels.reserve(s.stepsAvailable.size());
    for (int v : s.stepsAvailable) labels.push_back(std::to_string(v));
    std::vector<const char*> labelPtrs;
    labelPtrs.reserve(labels.size());
    for (const auto& l : labels) labelPtrs.push_back(l.c_str());

    int comboIdx = s.defaultStepIndex;
    if (comboIdx < 0 || comboIdx >= static_cast<int>(labelPtrs.size())) {
        comboIdx = 0;
    }
    if (!labelPtrs.empty()
        && ImGui::Combo("##current_step_combo", &comboIdx,
                         labelPtrs.data(), static_cast<int>(labelPtrs.size()))) {
        s.defaultStepIndex = comboIdx;
        markDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.current_step_hint").c_str());
    }

    ImGui::Spacing();

    // === Sección: Pasos disponibles ===
    ImGui::SeparatorText(I18n::T("editor.map_tools.snap.section_steps").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.steps_hint").c_str());
    }

    int indexToDelete = -1;
    for (size_t i = 0; i < s.stepsAvailable.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(80.0f);
        int value = s.stepsAvailable[i];
        if (ImGui::InputInt("##step_value", &value, 0, 0,
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (value > 0) {
                s.stepsAvailable[i] = value;
                markDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            if (s.stepsAvailable.size() > 1) {
                indexToDelete = static_cast<int>(i);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.map_tools.snap.remove_step").c_str());
        }
        ImGui::PopID();
    }

    if (indexToDelete >= 0) {
        s.stepsAvailable.erase(s.stepsAvailable.begin() + indexToDelete);
        if (s.defaultStepIndex >= static_cast<int>(s.stepsAvailable.size())) {
            s.defaultStepIndex = static_cast<int>(s.stepsAvailable.size()) - 1;
        }
        markDirty();
    }

    if (ImGui::SmallButton(" + ")) {
        const int newStep = s.stepsAvailable.empty()
            ? 1 : s.stepsAvailable.back() * 2;
        s.stepsAvailable.push_back(newStep);
        markDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.add_step").c_str());
    }
    if (s.stepsAvailable != defaults.stepsAvailable) {
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string(ICON_FA_ROTATE_LEFT)
                                + " ##reset_steps").c_str())) {
            s.stepsAvailable = defaults.stepsAvailable;
            if (s.defaultStepIndex >= static_cast<int>(s.stepsAvailable.size())) {
                s.defaultStepIndex = defaults.defaultStepIndex;
            }
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.common.reset_default").c_str());
        }
    }

    ImGui::Spacing();

    // === Sección: Umbrales ===
    ImGui::SeparatorText(I18n::T("editor.map_tools.snap.section_thresholds").c_str());

    auto thresholdSlider = [&](const char* keyLabel,
                                const char* keyHint,
                                const char* widgetId,
                                const char* resetSuffix,
                                f32& value,
                                f32 defaultValue,
                                f32 minVal,
                                f32 maxVal,
                                const char* fmt) {
        ImGui::TextUnformatted(I18n::T(keyLabel).c_str());
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat(widgetId, &value, minVal, maxVal, fmt)) {
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(keyHint).c_str());
        }
        if (value != defaultValue) {
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string(ICON_FA_ROTATE_LEFT)
                                    + " ##reset_" + resetSuffix).c_str())) {
                value = defaultValue;
                markDirty();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                    I18n::T("editor.common.reset_default").c_str());
            }
        }
    };

    thresholdSlider("editor.map_tools.snap.vertex_threshold",
                     "editor.map_tools.snap.vertex_threshold_hint",
                     "##vertex_threshold", "vertex_threshold",
                     s.snapToVertexThresholdNdc, defaults.snapToVertexThresholdNdc,
                     0.005f, 0.10f, "%.3f");

    thresholdSlider("editor.map_tools.snap.broadphase",
                     "editor.map_tools.snap.broadphase_hint",
                     "##broadphase", "broadphase",
                     s.snapBroadphaseMinWorld, defaults.snapBroadphaseMinWorld,
                     4.0f, 128.0f, "%.1f u");
}

} // namespace Mood
