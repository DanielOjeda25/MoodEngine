#include "editor/panels/scene/HierarchyPanel.h"

#include "core/Log.h"  // F2H23: log de selection
#include "editor/selection/SelectionSet.h"  // F2H13
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconHelpers.h"  // F2H37: iconForEntity compartido
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/VisGroup.h"  // F2H33: gray-out hidden entities
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace Mood {

void HierarchyPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_scene == nullptr || m_ui == nullptr) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.hierarchy.scene_not_injected").c_str());
        ImGui::End();
        return;
    }

    // F2H57: boton "+ Crear Entidad" arriba de la lista (workflow
    // Hammer Editor). Click -> file picker del SO -> spawnea entidad
    // con MeshRenderer apuntando al modelo elegido. EditorApplication
    // consume el request y maneja el file dialog + carga del asset.
    const std::string createLabel = std::string("+ ") +
        I18n::T("editor.panel.hierarchy.create_entity");
    if (ImGui::Button(createLabel.c_str())) {
        m_ui->requestCreateEntityFromModel();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.hierarchy.create_entity_tooltip").c_str());
    }
    ImGui::Separator();

    // F2H5: cache de entries por frame. Reusa storage entre frames —
    // `clear()` no reduce capacity, asi que tras la primera escena
    // grande el vector mantiene la capacidad maxima observada.
    collectHierarchyEntries(*m_scene, m_entries);

    if (m_entries.empty()) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.hierarchy.empty").c_str());
        ImGui::End();
        return;
    }

    SelectionSet& set = m_ui->selectionSet();
    const auto totalCount = static_cast<int>(m_entries.size());

    // F2H23: hint de shortcuts arriba de la lista. Texto chico gris para
    // no robar foco visual; tooltip al hover por si el dev quiere ver
    // el detalle de cada modifier. F2H23 polish: Shift=add, Ctrl=toggle
    // (convencion Maya / Hammer — pedido del dev).
    ImGui::TextDisabled("%s", I18n::T("editor.panel.hierarchy.shortcut_hint").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", I18n::T("editor.panel.hierarchy.shortcut_tooltip").c_str());
    }
    ImGui::Separator();

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

            // F2H13: color del Header naranja para "active" (primary del set).
            // F2H37 polish multi-select: amarillo para secundarias (en
            // selection pero no active) — pre-F2H37 todas las del set
            // se veian igual al ImGui default y el dev no distinguia
            // cual era la primary sin abrir el Inspector.
            const bool pushedColor = isInSelection;
            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(0.95f, 0.55f, 0.10f, 0.65f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(0.95f, 0.60f, 0.15f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                    ImVec4(1.00f, 0.65f, 0.20f, 1.00f));
            } else if (isInSelection) {
                ImGui::PushStyleColor(ImGuiCol_Header,
                    ImVec4(0.85f, 0.75f, 0.15f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(0.90f, 0.80f, 0.20f, 0.75f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                    ImVec4(0.95f, 0.85f, 0.25f, 1.00f));
            }

            // F2H37: icono FontAwesome via helper compartido. Pre-F2H37
            // era ASCII `[M]`/`[B]`/... (F2H23). Buffer chico — los
            // nombres tipicos son <50 chars + 1 codepoint UTF-8 + " ".
            char labelBuf[88];
            std::snprintf(labelBuf, sizeof(labelBuf), "%s %s",
                            iconForEntity(e), entry.tag->name.c_str());
            // F2H33: si la entidad pertenece a un VisGroup hidden, mostrar
            // el label en gris claro para que el dev sepa que esta oculta
            // en viewport (sino se sorprende: "click en X y no la veo
            // resaltada en el editor"). El click sigue funcionando — la
            // entidad existe, solo el render + picking la skipean.
            const bool grayedHidden = isEntityHiddenByVisGroup(*m_scene, e);
            if (grayedHidden) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
            }
            if (ImGui::Selectable(labelBuf, isInSelection,
                                    ImGuiSelectableFlags_AllowDoubleClick)) {
                // F2H23 polish: convencion Maya / Hammer / Unreal.
                //   Click       = replace (single).
                //   Shift+click = ADD (entidad nueva al set, active = ella).
                //   Ctrl+click  = TOGGLE (anadir si no esta, quitar si esta).
                // F2H22 tenia Shift=toggle / Ctrl=add (convencion Blender).
                // El dev pidio explicitamente "shift seleccionar ambos y
                // de ahi mover" — convencion Maya.
                if (keyShift) {
                    add(set, e);
                    Log::editor()->info(
                        "[escena] Shift+click ADD '{}' (selected={})",
                        entry.tag->name, set.selected.size());
                } else if (keyCtrl) {
                    toggle(set, e);
                    Log::editor()->info(
                        "[escena] Ctrl+click TOGGLE '{}' (selected={})",
                        entry.tag->name, set.selected.size());
                } else {
                    replaceWithSingle(set, e);
                    Log::editor()->info(
                        "[escena] click replace '{}' (selected=1)",
                        entry.tag->name);
                }
            }

            // F2H57 Bloque C: menu contextual con click derecho sobre
            // la entidad. Convencion Hammer Editor: Cambiar tipo /
            // Eliminar (Rename + Duplicar quedan como follow-up).
            if (ImGui::BeginPopupContextItem("##entity_ctx")) {
                // Si el right-click cae sobre una entidad fuera de la
                // seleccion, hacer que esa entidad sea la activa
                // (UX estandar: el menu opera sobre lo que clickeaste).
                if (!isInSelection) {
                    replaceWithSingle(set, e);
                }
                if (ImGui::MenuItem(
                        I18n::T("editor.panel.hierarchy.ctx.change_type").c_str())) {
                    m_ui->requestEntityConvertModal(entry.handle);
                }
                ImGui::Separator();
                if (ImGui::MenuItem(
                        I18n::T("editor.panel.hierarchy.ctx.delete").c_str())) {
                    m_ui->requestDeleteSelectedEntity();
                }
                ImGui::EndPopup();
            }

            if (grayedHidden) {
                ImGui::PopStyleColor();
            }
            if (pushedColor) {
                ImGui::PopStyleColor(3);
            }
            ImGui::PopID();
        }
    }
    clipper.End();

    ImGui::Separator();
    // F2H23: warning visual si la escena tiene >5000 entidades (limite
    // donde el panel empieza a sentirse pesado pese a F2H5 ListClipper —
    // el cuello restante es scene iter en otros sistemas, ver F2H5
    // PERFORMANCE.md). Color amarillo no rojo, no es error.
    if (totalCount > 5000) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                                ImVec4(1.00f, 0.85f, 0.25f, 1.00f));
        ImGui::Text("%s",
                     I18n::T("editor.panel.hierarchy.count_large", totalCount).c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("%s",
                              I18n::T("editor.panel.hierarchy.count", totalCount).c_str());
    }

    ImGui::End();
}

} // namespace Mood
