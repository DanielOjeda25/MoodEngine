#pragma once

// F2H24: helpers compartidos por todos los archivos parciales del
// Inspector (InspectorPanel.cpp + InspectorPanel_*.cpp). Header
// privado del modulo — no incluir desde otro modulo.

#include "editor/commands/EditPropertyCommand.h"
#include "editor/panels/scene/InspectorEditTracker.h"
#include "editor/ui/EditorUI.h"
#include "engine/scene/core/Entity.h"

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

} // namespace Mood::detail
