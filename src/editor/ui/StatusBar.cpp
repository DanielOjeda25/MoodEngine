#include "editor/ui/StatusBar.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Mood {

void StatusBar::draw(EditorMode mode, EditorSubMode subMode) {
    // Usamos BeginViewportSideBar para que ImGui reserve la franja inferior
    // del viewport principal ANTES del dockspace. Asi el Asset Browser no se
    // superpone con la status bar. Requiere llamarse antes de Dockspace::begin
    // (ver EditorUI::draw).
    const float statusHeight = ImGui::GetFrameHeight();

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::BeginViewportSideBar("##MoodStatusBar", nullptr, ImGuiDir_Down, statusHeight, flags)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::Text("FPS: %.1f", static_cast<double>(m_fps));
            ImGui::Separator();
            ImGui::TextUnformatted(mode == EditorMode::Play ? "Play Mode" : "Editor Mode");
            // F2H17: sub-modo (Face / Object) en color distinto.
            if (mode == EditorMode::Editor && subMode != EditorSubMode::Object) {
                ImGui::SameLine();
                const char* subLabel = (subMode == EditorSubMode::Face)
                    ? " | Face Mode (3)" : " | Sub-mode";
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.10f, 1.0f),
                                     "%s", subLabel);
            }
            ImGui::Separator();
            ImGui::TextUnformatted(m_message.c_str());
            // F2H16: "Ultimo: <command name>" Blender-style. Solo se
            // muestra si hay algo en el undo stack.
            if (!m_lastCommand.empty()) {
                ImGui::Separator();
                ImGui::Text("Ultimo: %s", m_lastCommand.c_str());
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Mood
