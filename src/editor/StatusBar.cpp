#include "editor/StatusBar.h"

#include <imgui.h>

namespace Mood {

void StatusBar::draw(EditorMode mode) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float statusHeight = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(
        viewport->WorkPos.x,
        viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##MoodStatusBar", nullptr, flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("FPS: %.1f", static_cast<double>(m_fps));
            ImGui::Separator();
            ImGui::TextUnformatted(mode == EditorMode::Play ? "Play Mode" : "Editor Mode");
            ImGui::Separator();
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Mood
