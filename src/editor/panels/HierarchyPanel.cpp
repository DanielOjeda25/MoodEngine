#include "editor/panels/HierarchyPanel.h"

#include "editor/EditorUI.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <imgui.h>

#include <string>

namespace Mood {

void HierarchyPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_scene == nullptr || m_ui == nullptr) {
        ImGui::TextDisabled("Escena no inyectada");
        ImGui::End();
        return;
    }

    const Entity selected = m_ui->selectedEntity();
    int rendered = 0;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        ++rendered;
        const bool isSelected = selected && e == selected;
        // PushID por handle para diferenciar entidades con el mismo tag.
        ImGui::PushID(static_cast<int>(e.handle()));
        if (ImGui::Selectable(tag.name.c_str(), isSelected,
                               ImGuiSelectableFlags_AllowDoubleClick)) {
            m_ui->setSelectedEntity(e);
        }
        ImGui::PopID();
    });

    if (rendered == 0) {
        ImGui::TextDisabled("Escena vacia.");
    } else {
        ImGui::Separator();
        ImGui::TextDisabled("%d entidades", rendered);
    }

    ImGui::End();
}

} // namespace Mood
