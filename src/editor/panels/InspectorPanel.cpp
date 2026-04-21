#include "editor/panels/InspectorPanel.h"

#include <imgui.h>

namespace Mood {

void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (ImGui::Begin(name(), &visible)) {
        ImGui::TextDisabled("No hay entidad seleccionada");
    }
    ImGui::End();
}

} // namespace Mood
