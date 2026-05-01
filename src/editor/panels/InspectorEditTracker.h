#pragma once

// InspectorEditTracker (Hito 32 D): infra para que los widgets del
// Inspector emitan UN solo `EditPropertyCommand` por gesto de edicion
// (drag, hold, type) en lugar de N por frame.
//
// Modelo:
//   - `IsItemActivated()`: el widget acaba de recibir foco/click. Snapshot
//     del valor pre-edit en `tracker.before` (variant tipada).
//   - `IsItemDeactivatedAfterEdit()`: el widget perdio foco Y el valor
//     cambio durante la interaccion. Capturamos `after`, revertimos al
//     `before`, push command (que re-aplica via execute() — mismo patron
//     que el gizmo en Hito 27).
//
// Solo un widget puede estar activo a la vez (no se pueden draggear dos
// sliders simultaneo en ImGui) — un solo buffer global por panel basta.
//
// Uso desde InspectorPanel:
//
//   ImGui::DragFloat3("position##tr", &t.position.x, 0.05f);
//   trackPropertyEdit<glm::vec3>(m_editTracker, t.position,
//       e, *m_ui->historyStack(),
//       [](Entity& en, const glm::vec3& v) {
//           en.getComponent<TransformComponent>().position = v;
//       },
//       "Mover entidad");

#include "core/Types.h"
#include "editor/commands/EditPropertyCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/scene/Entity.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <imgui.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Mood {

struct InspectorEditTracker {
    /// Id del widget que esta siendo editado. 0 = sin drag activo.
    ImGuiID activeId = 0;
    /// Valor pre-edit tipado. Variant cubre los tipos que el Inspector
    /// realmente edita: f32 sliders, glm::vec3 transforms, glm::vec4
    /// colors, bool checkboxes, std::string text inputs, u32 DragInts +
    /// asset ids (Hito 36 — habilita undo de maxParticles del particle
    /// emitter y de drops de textura sobre material slot).
    std::variant<f32, glm::vec3, glm::vec4, bool, std::string, u32> before;
};

/// Helper template: llamar INMEDIATAMENTE despues del widget ImGui que se
/// quiere hacer undoable. `current` es el campo que el widget acaba de
/// mutar (ya tiene el valor post-drag durante el drag).
///
/// `setter` es la lambda que aplica el valor sobre la entidad —
/// captura el path al campo (componente + miembro). Necesario porque
/// el comando puede ejecutarse despues de que el `current&` haya
/// quedado invalidado.
template<typename T>
void trackPropertyEdit(InspectorEditTracker& tracker,
                       const T& current,
                       Entity entity,
                       HistoryStack& history,
                       typename EditPropertyCommand<T>::Setter setter,
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
                // execute() (mismo patron del gizmo en Hito 27 — UNA sola
                // fuente de verdad de "como se aplica el cambio").
                setter(entity, before);
                auto cmd = std::make_unique<EditPropertyCommand<T>>(
                    entity, before, after, std::move(setter), label);
                history.push(std::move(cmd));
            }
        }
        tracker.activeId = 0;
    }
}

} // namespace Mood
