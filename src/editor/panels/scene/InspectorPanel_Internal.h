#pragma once

// F2H24: helpers compartidos por todos los archivos parciales del
// Inspector (InspectorPanel.cpp + InspectorPanel_*.cpp). Header
// privado del modulo — no incluir desde otro modulo.

#include "core/i18n/I18n.h"  // F2H74: field-helpers arman el label traducido
#include "editor/commands/EditPropertyCommand.h"
#include "editor/panels/scene/InspectorEditTracker.h"
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
