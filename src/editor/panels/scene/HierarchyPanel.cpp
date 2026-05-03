#include "editor/panels/scene/HierarchyPanel.h"

#include "editor/ui/EditorUI.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

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

    // F2H5: cache de entries por frame. Reusa storage entre frames —
    // `clear()` no reduce capacity, asi que tras la primera escena
    // grande el vector mantiene la capacidad maxima observada.
    collectHierarchyEntries(*m_scene, m_entries);

    if (m_entries.empty()) {
        ImGui::TextDisabled("Escena vacia.");
        ImGui::End();
        return;
    }

    const Entity selected = m_ui->selectedEntity();
    const auto totalCount = static_cast<int>(m_entries.size());

    // F2H5: virtualizacion con ImGuiListClipper. ImGui calcula cuantas
    // entries entran en el scroll area visible y solo dispatcha el loop
    // sobre ese rango — independientemente de cuantas haya en total.
    // Sin esto, 8000 entidades = 8000 iteraciones cada frame.
    ImGuiListClipper clipper;
    clipper.Begin(totalCount);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const HierarchyEntry& entry = m_entries[i];
            const Entity e(entry.handle, m_scene);
            const bool isSelected = selected && e == selected;
            // PushID por handle para diferenciar entidades con el mismo tag.
            ImGui::PushID(static_cast<int>(entry.handle));
            if (ImGui::Selectable(entry.tag->name.c_str(), isSelected,
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
                m_ui->setSelectedEntity(e);
            }
            ImGui::PopID();
        }
    }
    clipper.End();

    ImGui::Separator();
    ImGui::TextDisabled("%d entidades", totalCount);

    ImGui::End();
}

} // namespace Mood
