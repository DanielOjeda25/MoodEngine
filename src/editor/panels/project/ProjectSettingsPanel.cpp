#include "editor/panels/project/ProjectSettingsPanel.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/project/ProjectSettings.h"
#include "engine/scene/serialization/ProjectSerializer.h"  // Project struct

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Mood {

void ProjectSettingsPanel::onImGuiRender() {
    if (!visible) return;
    if (!ImGui::Begin(name(), &visible)) {
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

    if (ImGui::BeginTabBar("##project_settings_tabs")) {
        if (ImGui::BeginTabItem(I18n::T("editor.project_settings.tab.general").c_str())) {
            drawGeneralTab(project->settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(I18n::T("editor.project_settings.tab.spawn").c_str())) {
            drawPlaceholderTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(I18n::T("editor.project_settings.tab.rendering").c_str())) {
            drawPlaceholderTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(I18n::T("editor.project_settings.tab.physics").c_str())) {
            drawPlaceholderTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ProjectSettingsPanel::drawGeneralTab(ProjectSettings& settings) {
    ImGui::Spacing();

    // === Target FPS ===
    int targetFps = settings.targetFps;
    const int kMinFps = 10;
    const int kMaxFps = 240;
    if (ImGui::DragInt(I18n::T("editor.project_settings.target_fps").c_str(),
                       &targetFps, 1.0f, kMinFps, kMaxFps)) {
        targetFps = std::clamp(targetFps, kMinFps, kMaxFps);
        if (targetFps != settings.targetFps) {
            settings.targetFps = targetFps;
            if (m_ui != nullptr) m_ui->requestProjectDirty();
        }
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.project_settings.target_fps_hint").c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === Descripcion ===
    // InputTextMultiline necesita buffer mutable. Hacemos copia local +
    // commit al perder foco (IsItemDeactivatedAfterEdit) para no
    // disparar dirty cada keystroke (mucho ruido en el undo histo
    // futuro + spam en la status bar "* dirty").
    static constexpr size_t kDescBufSize = 2048;
    char descBuf[kDescBufSize];
    std::snprintf(descBuf, sizeof(descBuf), "%s", settings.description.c_str());
    if (ImGui::InputTextMultiline(
            I18n::T("editor.project_settings.description").c_str(),
            descBuf, sizeof(descBuf),
            ImVec2(0.0f, 100.0f))) {
        // edit in progress — copia al modelo pero no marca dirty
        settings.description = descBuf;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (m_ui != nullptr) m_ui->requestProjectDirty();
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.project_settings.description_hint").c_str());
}

void ProjectSettingsPanel::drawPlaceholderTab() {
    ImGui::Spacing();
    ImGui::TextWrapped("%s",
        I18n::T("editor.project_settings.placeholder_f3h4").c_str());
}

} // namespace Mood
