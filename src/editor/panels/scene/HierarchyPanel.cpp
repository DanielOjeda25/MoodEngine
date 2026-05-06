#include "editor/panels/scene/HierarchyPanel.h"

#include "editor/selection/SelectionSet.h"  // F2H13
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

    SelectionSet& set = m_ui->selectionSet();
    const auto totalCount = static_cast<int>(m_entries.size());

    // F2H13: detectar modifiers para Shift+click (toggle) y
    // Ctrl+click (add). Plain click = replaceWithSingle.
    const ImGuiIO& io = ImGui::GetIO();
    const bool keyShift = io.KeyShift;
    const bool keyCtrl  = io.KeyCtrl;

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
            const bool isInSelection = contains(set, e);
            const bool isActive = static_cast<bool>(set.active)
                && set.active.handle() == e.handle();

            // PushID por handle para diferenciar entidades con el mismo tag.
            ImGui::PushID(static_cast<int>(entry.handle));

            // F2H13: color del Header distinto cuando es la "active"
            // (ImGui ya destaca isInSelection con el color default
            // del header). Para la active overrideamos a un naranja
            // discreto que matchea el outline del overlay.
            const bool pushedColor = isActive;
            if (pushedColor) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(0.95f, 0.55f, 0.10f, 0.65f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(0.95f, 0.60f, 0.15f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                    ImVec4(1.00f, 0.65f, 0.20f, 1.00f));
            }

            if (ImGui::Selectable(entry.tag->name.c_str(), isInSelection,
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
                if (keyShift) {
                    toggle(set, e);
                } else if (keyCtrl) {
                    add(set, e);
                } else {
                    replaceWithSingle(set, e);
                }
            }

            if (pushedColor) {
                ImGui::PopStyleColor(3);
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
