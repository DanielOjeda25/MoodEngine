#include "editor/panels/AssetBrowserPanel.h"

#include <imgui.h>

namespace Mood {

void AssetBrowserPanel::onImGuiRender() {
    if (!visible) return;

    if (ImGui::Begin(name(), &visible)) {
        ImGui::TextDisabled("Sin assets cargados");
    }
    ImGui::End();
}

} // namespace Mood
