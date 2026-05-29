// F3H28: categoria scene-wide "Grupos" del Inspector. Lista los Empty
// Group_<N> del mapa (backend F3H27 — Empty padre de un sub-tree de
// brushes/meshes). Cada item es clickeable para seleccionar el Group
// completo. Acciones: agrupar selección actual, desagrupar selección.
// No depende de la entity seleccionada — es scene-wide igual que
// Environment, pero sin singleton.

#include "editor/panels/scene/InspectorPanel.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <cstring>
#include <string>
#include <vector>

namespace Mood {

void InspectorPanel::renderGroupsSection() {
    if (m_ui == nullptr) return;
    Scene* scene = m_ui->scene();
    if (scene == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.inspector.groups.no_scene").c_str());
        return;
    }

    // F3H28: header de la categoría.
    ImGui::TextDisabled("%s",
        I18n::T("editor.inspector.groups.header").c_str());
    ImGui::Spacing();

    // Acciones globales: agrupar/desagrupar selección actual.
    const auto& set = m_ui->selectionSet();
    const bool canGroup = set.selected.size() >= 2u;
    bool canUngroup = false;
    for (const Entity& e : set.selected) {
        if (!e) continue;
        if (e.hasComponent<TransformComponent>()
            && e.getComponent<TransformComponent>().parent != entt::null) {
            canUngroup = true;
            break;
        }
    }

    if (!canGroup) ImGui::BeginDisabled();
    if (ImGui::Button(I18n::T("editor.inspector.groups.btn_group").c_str())) {
        m_ui->requestGroupSelection();
    }
    if (!canGroup) ImGui::EndDisabled();
    if (ImGui::IsItemHovered() && !canGroup) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.groups.btn_group_disabled_tooltip").c_str());
    }

    ImGui::SameLine();
    if (!canUngroup) ImGui::BeginDisabled();
    if (ImGui::Button(I18n::T("editor.inspector.groups.btn_ungroup").c_str())) {
        m_ui->requestUngroupSelection();
    }
    if (!canUngroup) ImGui::EndDisabled();
    if (ImGui::IsItemHovered() && !canUngroup) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.groups.btn_ungroup_disabled_tooltip").c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Lista de Empties con descendants (= Groups). Iteramos el registry
    // filtrando entities que NO tienen componentes geométricos / point
    // pero sí tienen descendants. Mismo criterio que ScenePick R7 y el
    // marker XYZ del overlay.
    struct GroupEntry {
        Entity e;
        std::string tag;
        std::size_t descendantCount;
    };
    std::vector<GroupEntry> groups;
    scene->forEach<TransformComponent>([&](Entity e, TransformComponent&) {
        if (e.hasComponent<BrushComponent>())             return;
        if (e.hasComponent<MeshRendererComponent>())      return;
        if (e.hasComponent<LightComponent>())             return;
        if (e.hasComponent<AudioSourceComponent>())       return;
        if (e.hasComponent<TriggerComponent>())           return;
        if (e.hasComponent<CameraComponent>())            return;
        if (e.hasComponent<ParticleEmitterComponent>())   return;
        const auto descendants = scene->descendantsOf(e.handle());
        if (descendants.empty()) return;
        std::string tag = e.hasComponent<TagComponent>()
            ? e.getComponent<TagComponent>().name
            : std::string("(sin tag)");
        groups.push_back({e, std::move(tag), descendants.size()});
    });

    if (groups.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.inspector.groups.empty").c_str());
        return;
    }

    // Header de la lista.
    ImGui::TextDisabled("%s",
        I18n::T("editor.inspector.groups.list_header",
                 static_cast<int>(groups.size())).c_str());
    ImGui::Spacing();

    const Entity activeSelected = set.active;
    for (const GroupEntry& g : groups) {
        ImGui::PushID(static_cast<int>(g.e.handle()));
        char row[160];
        std::snprintf(row, sizeof(row), "%s  %s  (%d)",
                       ICON_FA_FOLDER, g.tag.c_str(),
                       static_cast<int>(g.descendantCount));
        const bool isSelected = activeSelected
            && activeSelected.handle() == g.e.handle();
        if (ImGui::Selectable(row, isSelected)) {
            m_ui->setSelectedEntity(g.e);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.inspector.groups.row_tooltip").c_str());
        }
        ImGui::PopID();
    }
}

} // namespace Mood
