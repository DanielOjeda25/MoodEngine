#include "editor/panels/ViewportPanel.h"

#include <imgui.h>

namespace Mood {

void ViewportPanel::onImGuiRender() {
    if (!visible) return;

    // Fondo gris oscuro dentro del panel para que se distinga del dockspace.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    if (ImGui::Begin(name(), &visible)) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const char* placeholder = "Viewport (Hito 2: primer triangulo)";
        const ImVec2 textSize = ImGui::CalcTextSize(placeholder);

        // Centrar texto en el panel.
        ImGui::SetCursorPos(ImVec2(
            (avail.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
            (avail.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()));
        ImGui::TextUnformatted(placeholder);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace Mood
