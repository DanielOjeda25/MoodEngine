#include "editor/panels/scene/VisGroupsPanel.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/VisGroupCommands.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconHelpers.h"  // F2H37: iconForEntity compartido
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/VisGroup.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <random>

namespace Mood {

namespace {

/// @brief Color pastel pseudo-aleatorio para grupos nuevos. Genera HSL con
///        S/L medios y H aleatorio — produce colores distinguibles entre
///        si sin saturados.
glm::vec3 randomPastelColor() {
    static std::mt19937 rng{0xC0FFEEu};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float h = dist(rng);
    // HSV -> RGB con S=0.55, V=0.85 (pastel).
    const float s = 0.55f, v = 0.85f;
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r=0, g=0, b=0;
    if      (h < 1.0f/6.0f) { r=c; g=x; b=0; }
    else if (h < 2.0f/6.0f) { r=x; g=c; b=0; }
    else if (h < 3.0f/6.0f) { r=0; g=c; b=x; }
    else if (h < 4.0f/6.0f) { r=0; g=x; b=c; }
    else if (h < 5.0f/6.0f) { r=x; g=0; b=c; }
    else                    { r=c; g=0; b=x; }
    return {r + m, g + m, b + m};
}

/// @brief Cuenta cuantas entities pertenecen al grupo. Lineal en la
///        cantidad de entities con membership component (no en el total).
u64 countMembers(Scene& scene, u64 groupId) {
    u64 n = 0;
    scene.forEach<VisGroupMembershipComponent>(
        [&](Entity, VisGroupMembershipComponent& m) {
            if (m.groupId == groupId) ++n;
        });
    return n;
}

} // namespace

void VisGroupsPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_scene == nullptr || m_history == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.visgroups.not_injected").c_str());
        ImGui::End();
        return;
    }

    // Boton "+ Nuevo grupo" arriba.
    if (ImGui::Button(I18n::T("editor.panel.visgroups.new_group").c_str())) {
        const u64 nextN = m_scene->visgroups().size() + 1;
        const std::string defaultName =
            I18n::T("editor.panel.visgroups.default_name",
                    static_cast<unsigned long long>(nextN));
        m_history->push(std::make_unique<CreateVisGroupCommand>(
            m_scene, defaultName, randomPastelColor()));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.visgroups.new_group_tooltip").c_str());
    }
    ImGui::Separator();

    if (m_scene->visgroups().empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.visgroups.empty").c_str());
        ImGui::End();
        return;
    }

    // Snapshot de los ids visibles (no iterar el vector mientras lo mutamos).
    std::vector<u64> ids;
    ids.reserve(m_scene->visgroups().size());
    for (const auto& vg : m_scene->visgroups()) ids.push_back(vg.id);

    for (u64 gid : ids) {
        VisGroup* vg = m_scene->findVisGroup(gid);
        if (vg == nullptr) continue;

        ImGui::PushID(static_cast<int>(gid));

        const u64 count = countMembers(*m_scene, gid);

        // Header del grupo: TreeNode colapsable + botones overlay (eye,
        // color, label). El AllowOverlap permite que los SmallButton/
        // ColorButton "compitan" con el click del TreeNode — ImGui da
        // prioridad al boton si esta encima.
        char headerLabel[180];
        std::snprintf(headerLabel, sizeof(headerLabel), "###grupo_%llu",
                       static_cast<unsigned long long>(gid));
        constexpr ImGuiTreeNodeFlags k_nodeFlags =
            ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_DefaultOpen;
        const bool expanded = ImGui::TreeNodeEx(headerLabel, k_nodeFlags);

        // Layout overlay sobre el header del TreeNode: SameLine reposiciona
        // el cursor para que los siguientes widgets esten en la misma fila.
        ImGui::SameLine();

        // Toggle hide/show: [V] visible / [X] hidden.
        const char* eyeLabel = vg->hidden ? "[X]" : "[V]";
        if (ImGui::SmallButton(eyeLabel)) {
            VisGroup before = *vg;
            VisGroup after = *vg;
            after.hidden = !before.hidden;
            m_history->push(std::make_unique<EditVisGroupCommand>(
                m_scene, gid, before, after,
                after.hidden ? "Ocultar VisGroup" : "Mostrar VisGroup"));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T(vg->hidden ? "editor.panel.visgroups.show_group"
                                     : "editor.panel.visgroups.hide_group").c_str());
        }

        ImGui::SameLine();

        // Color swatch (click abre color picker en popup).
        const ImVec4 col(vg->color.r, vg->color.g, vg->color.b, 1.0f);
        if (ImGui::ColorButton("##color", col,
                                ImGuiColorEditFlags_NoTooltip,
                                ImVec2(16, 16))) {
            ImGui::OpenPopup("##color_picker");
        }
        if (ImGui::BeginPopup("##color_picker")) {
            // F2H35 fix: snapshot pre cuando el popup recien aparece.
            // Sin esto el `before` capturado en el push de abajo era el
            // mismo valor que el `after` (porque el array `c[3]` se
            // reinicializaba cada frame con el color actual de `vg`,
            // y `vg->color` no se mutaba hasta el final — entonces el
            // command nacia no-op y el editor revertia visualmente).
            if (ImGui::IsWindowAppearing()) {
                m_colorEditingId = gid;
                m_colorEditBefore = vg->color;
            }
            float c[3] = { vg->color.r, vg->color.g, vg->color.b };
            if (ImGui::ColorPicker3("##picker", c)) {
                // Live preview: mutar el VisGroup directo cada frame
                // mientras el dev arrastra el picker. El render del orto
                // ya lee vg->color en cada frame asi que el cambio se
                // ve al instante en los 3 viewports.
                vg->color = glm::vec3(c[0], c[1], c[2]);
            }
            if (ImGui::IsItemDeactivatedAfterEdit() &&
                m_colorEditingId == gid) {
                VisGroup before = *vg;
                before.color = m_colorEditBefore;
                VisGroup after = *vg;  // ya tiene el color final tras live preview
                m_history->push(std::make_unique<EditVisGroupCommand>(
                    m_scene, gid, before, after, "Cambiar color VisGroup"));
                m_colorEditingId = 0;
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // Nombre + count, o input de rename si esta activo.
        if (m_renamingGroupId == gid) {
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", m_renameBuffer,
                                  sizeof(m_renameBuffer),
                                  ImGuiInputTextFlags_EnterReturnsTrue |
                                  ImGuiInputTextFlags_AutoSelectAll)) {
                std::string newName = m_renameBuffer;
                if (!newName.empty() && newName != vg->name) {
                    VisGroup before = *vg;
                    VisGroup after = *vg;
                    after.name = std::move(newName);
                    m_history->push(std::make_unique<EditVisGroupCommand>(
                        m_scene, gid, before, after, "Renombrar VisGroup"));
                }
                m_renamingGroupId = 0;
            }
            if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive()) {
                m_renamingGroupId = 0;
            }
        } else {
            char label[160];
            std::snprintf(label, sizeof(label), "%s  (%llu)",
                           vg->name.c_str(),
                           static_cast<unsigned long long>(count));
            ImGui::TextUnformatted(label);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                m_renamingGroupId = gid;
                std::strncpy(m_renameBuffer, vg->name.c_str(),
                              sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
            }
        }

        // Menu contextual (click derecho sobre la fila del header).
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem(I18n::T("editor.panel.visgroups.rename").c_str())) {
                m_renamingGroupId = gid;
                std::strncpy(m_renameBuffer, vg->name.c_str(),
                              sizeof(m_renameBuffer) - 1);
                m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
            }
            if (m_ui != nullptr) {
                const auto& set = m_ui->selectionSet();
                const bool hasSelection = !set.selected.empty();
                if (ImGui::MenuItem(I18n::T("editor.panel.visgroups.assign_selection").c_str(),
                                     nullptr, false, hasSelection)) {
                    for (const Entity& e : set.selected) {
                        if (!e || !e.hasComponent<TagComponent>()) continue;
                        const std::string& tag =
                            e.getComponent<TagComponent>().name;
                        const u64 before = visgroupIdOf(e);
                        if (before == gid) continue;
                        m_history->push(std::make_unique<AssignVisGroupCommand>(
                            m_scene, tag, before, gid,
                            "Asignar al grupo"));
                    }
                    Log::editor()->info(
                        "[visgroups] asignados {} entities al grupo '{}'",
                        set.selected.size(), vg->name);
                }
                if (ImGui::MenuItem(I18n::T("editor.panel.visgroups.remove_selection").c_str(),
                                     nullptr, false, hasSelection)) {
                    for (const Entity& e : set.selected) {
                        if (!e || !e.hasComponent<TagComponent>()) continue;
                        const std::string& tag =
                            e.getComponent<TagComponent>().name;
                        const u64 before = visgroupIdOf(e);
                        if (before != gid) continue;
                        m_history->push(std::make_unique<AssignVisGroupCommand>(
                            m_scene, tag, before, 0,
                            "Quitar del grupo"));
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.panel.visgroups.delete_group").c_str())) {
                m_history->push(std::make_unique<DeleteVisGroupCommand>(
                    m_scene, gid));
            }
            ImGui::EndPopup();
        }

        // Sub-lista de miembros (visible cuando el TreeNode esta abierto).
        // Cada miembro es clickeable: click selecciona la entity (replace
        // single, igual que click en Hierarchy). Boton [X] al final saca
        // del grupo. Si el grupo esta vacio, hint en gris.
        if (expanded) {
            if (count == 0) {
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.visgroups.no_members").c_str());
            } else {
                // Coleccionar miembros en una pasada (no iterar mientras
                // mutamos el set / pusheamos comandos).
                struct Member {
                    Entity e;
                    std::string tag;
                };
                std::vector<Member> members;
                members.reserve(static_cast<size_t>(count));
                m_scene->forEach<TagComponent, VisGroupMembershipComponent>(
                    [&](Entity e, TagComponent& t,
                        VisGroupMembershipComponent& mem) {
                        if (mem.groupId == gid) {
                            members.push_back({e, t.name});
                        }
                    });

                for (const Member& m : members) {
                    ImGui::PushID(m.tag.c_str());
                    char rowLabel[140];
                    std::snprintf(rowLabel, sizeof(rowLabel), "    %s %s",
                                   iconForEntity(m.e), m.tag.c_str());
                    if (ImGui::Selectable(rowLabel)) {
                        if (m_ui != nullptr) {
                            m_ui->setSelectedEntity(m.e);
                        }
                    }
                    // Boton [X] al final de la fila para sacar del grupo.
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x +
                                    ImGui::GetCursorPosX() - 24.0f);
                    if (ImGui::SmallButton("X")) {
                        m_history->push(std::make_unique<AssignVisGroupCommand>(
                            m_scene, m.tag, gid, 0, "Quitar del grupo"));
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s",
                            I18n::T("editor.panel.visgroups.remove_from_group").c_str());
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace Mood
