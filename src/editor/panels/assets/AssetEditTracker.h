#pragma once

// F2H84: AssetEditTracker — gemelo de InspectorEditTracker pero para los
// asset editors (Material / Item / Quest). Modelo idéntico: un widget activo
// por panel, snapshot del valor pre-edit al `IsItemActivated`, push de un
// `EditAssetPropertyCommand<T>` al `IsItemDeactivatedAfterEdit`. Un drag
// largo genera UN solo comando en el history.
//
// Uso desde un panel:
//
//   ImGui::SliderFloat("metallic", &mat->metallicMult, 0.0f, 1.0f);
//   trackAssetPropertyEdit<f32>(m_editTracker, mat->metallicMult,
//       *m_ui->historyStack(),
//       [mat](const f32& v) { mat->metallicMult = v; },
//       "Material: metallic");
//
// El history del panel debe `clear()` al cambiar de asset cargado (mismo
// patrón de NodeGraphSandboxPanel/ShaderGraphEditorPanel): los comandos
// capturan punteros/refs que dejan de ser válidos al swapear.

#include "core/Types.h"
#include "editor/commands/EditAssetPropertyCommand.h"
#include "editor/commands/HistoryStack.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Mood {

struct AssetEditTracker {
    /// Id del widget que está siendo editado. 0 = sin drag activo.
    ImGuiID activeId = 0;
    /// Valor pre-edit tipado. Variant cubre los tipos que los asset editors
    /// realmente editan: f32 sliders, int spinners, bool checkboxes,
    /// glm::vec3 colors, std::string text inputs.
    std::variant<f32, int, bool, glm::vec3, glm::vec4, std::string> before;
};

/// Helper template: llamar INMEDIATAMENTE después del widget ImGui que se
/// quiere hacer undoable. `current` es el campo que el widget acaba de
/// mutar (ya tiene el valor post-drag durante el drag).
template<typename T>
void trackAssetPropertyEdit(AssetEditTracker& tracker,
                             const T& current,
                             HistoryStack& history,
                             typename EditAssetPropertyCommand<T>::Setter setter,
                             const std::string& label) {
    const ImGuiID itemId = ImGui::GetItemID();

    if (ImGui::IsItemActivated()) {
        tracker.activeId = itemId;
        tracker.before = current;  // assignment al variant
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && tracker.activeId == itemId) {
        const T after = current;
        if (const T* beforePtr = std::get_if<T>(&tracker.before)) {
            const T before = *beforePtr;
            if (before != after && setter) {
                // Revertir al before para que push() aplique el after via
                // execute() — UNA sola fuente de verdad de "cómo se aplica
                // el cambio". Mismo patrón que el Inspector.
                setter(before);
                auto cmd = std::make_unique<EditAssetPropertyCommand<T>>(
                    before, after, std::move(setter), label);
                history.push(std::move(cmd));
            }
        }
        tracker.activeId = 0;
    }
}

} // namespace Mood
