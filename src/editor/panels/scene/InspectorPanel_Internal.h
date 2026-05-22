#pragma once

// F2H24: helpers compartidos por todos los archivos parciales del
// Inspector (InspectorPanel.cpp + InspectorPanel_*.cpp). Header
// privado del modulo — no incluir desde otro modulo.

#include "core/i18n/I18n.h"  // F2H74: field-helpers arman el label traducido
#include "editor/commands/AddComponentCommand.h"  // F2H81: makeRemoveComponentCommand
#include "editor/commands/EditPropertyCommand.h"
#include "editor/panels/scene/InspectorEditTracker.h"
#include "editor/panels/scene/InspectorPanel.h"  // F2H81: def. de beginComponentSection
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en headers de seccion
#include "engine/scene/core/Entity.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <string>

namespace Mood::detail {

// Hito 32 D: helper para empujar un EditPropertyCommand cuando el dev
// suelta un drag/edit en un widget del Inspector. Se llama
// INMEDIATAMENTE despues del widget para que `IsItem*` se refiera a el.
// Captura history desde el ui (puede ser null si todavia no inyectado).
template<typename T>
void pushEditIfDone(InspectorEditTracker& tracker, EditorUI* ui, Entity e,
                     const T& current,
                     typename EditPropertyCommand<T>::Setter setter,
                     const std::string& label) {
    HistoryStack* h = ui ? ui->historyStack() : nullptr;
    if (h == nullptr) return;
    trackPropertyEdit<T>(tracker, current, e, *h, std::move(setter), label);
}

// F2H23: helper estandar de ImGui samples — texto gris "(?)" con tooltip
// al hover. Sirve para descubribilidad sin inflar el panel con texto.
// Llamar INMEDIATAMENTE despues del widget que se quiere documentar.
inline void helpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", desc);
    }
}

// F2H23: detecta si el dev tiene un drag activo de tipo `type` (ej.
// "MOOD_TEXTURE_ASSET"). Sirve para cambiar el color de los botones
// drop-target (que el dev sepa "este boton acepta lo que arrastras").
inline bool isDragActiveOfType(const char* type) {
    const ImGuiPayload* p = ImGui::GetDragDropPayload();
    return p != nullptr && p->IsDataType(type);
}

// === F2H74: field-helpers del Inspector ===========================
// Colapsan el triplete que se repetia ~76 veces en los partials del
// Inspector: (1) armar label i18n + "##suffix", (2) widget, (3)
// pushEditIfDone para undo. Estilo property-drawer de Unity
// (EditorGUILayout) / Unreal (DetailsView). Devuelven `true` si el
// widget se edito este frame (para que el caller setee m_editedThisFrame).
// El `idSuffix` (ej "##trig") evita colisiones de ID entre secciones que
// reusan el mismo label key.

inline bool fieldDragFloat3(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        glm::vec3& value,
        typename EditPropertyCommand<glm::vec3>::Setter setter,
        const std::string& cmdLabel,
        float speed = 0.1f, float vmin = 0.0f, float vmax = 0.0f) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::DragFloat3(label.c_str(), &value.x, speed, vmin, vmax);
    pushEditIfDone<glm::vec3>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

inline bool fieldDragFloat(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        f32& value,
        typename EditPropertyCommand<f32>::Setter setter,
        const std::string& cmdLabel,
        float speed = 0.1f, float vmin = 0.0f, float vmax = 0.0f) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::DragFloat(label.c_str(), &value, speed, vmin, vmax);
    pushEditIfDone<f32>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

// Color RGB (sin speed/min/max). Usa el mismo camino de undo que los drags.
inline bool fieldColorEdit3(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        glm::vec3& value,
        typename EditPropertyCommand<glm::vec3>::Setter setter,
        const std::string& cmdLabel) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::ColorEdit3(label.c_str(), &value.x);
    pushEditIfDone<glm::vec3>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

} // namespace Mood::detail

namespace Mood {

// F2H81: definicion del header plegable (declarado en InspectorPanel.h).
// Templado en T para que el menu "Quitar componente" arme un
// makeRemoveComponentCommand<T> tipado. Reemplaza el SeparatorText
// siempre-abierto: ahora cada componente es una tarjeta que se pliega.
template<typename T>
bool InspectorPanel::beginComponentSection(Entity e, const char* label,
                                            bool removable) {
    // Orden de "plegar/expandir todo" de este frame (botones del toolbar).
    if (m_forceSectionState > 0) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    } else if (m_forceSectionState < 0) {
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    }

    const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

    // Menu contextual (clic derecho sobre el header): quitar componente.
    if (removable && ImGui::BeginPopupContextItem()) {
        const std::string item =
            ICON_FA_TRASH_CAN " " + I18n::T("editor.panel.inspector.remove_component");
        if (ImGui::Selectable(item.c_str())) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            auto cmd = makeRemoveComponentCommand<T>(
                e, I18n::T("editor.panel.inspector.remove_component"));
            if (h != nullptr) {
                h->push(std::move(cmd));  // ejecuta + apila para undo
            } else {
                cmd->execute();  // fallback defensivo sin history
            }
            m_editedThisFrame = true;
            ImGui::EndPopup();
            // El componente ya no existe — el caller NO debe dibujar el
            // cuerpo (su referencia al componente quedaria colgada).
            return false;
        }
        ImGui::EndPopup();
    }

    return open;
}

} // namespace Mood
