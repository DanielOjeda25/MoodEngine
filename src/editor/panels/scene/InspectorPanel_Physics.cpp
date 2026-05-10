// F2H24: Inspector — RigidBodyComponent (type / shape / halfExtents /
// mass / friction).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>

namespace Mood {

// RigidBodyComponent (Hito 12)
// Edit del shape/mass requiere recrear el body en Jolt, asi que por
// ahora los campos quedan read-only en Play Mode y editables en Editor.
// Al salir de Play, cualquier cambio toma efecto via rebuildSceneFromMap
// si se recarga el proyecto, o al destruir/recrear el body manualmente.
void InspectorPanel::renderRigidBodySection(Entity e) {
    auto& rb = e.getComponent<RigidBodyComponent>();
    ImGui::SeparatorText(ICON_FA_WEIGHT_HANGING " RigidBody");

    const char* typeNames[] = {"Static", "Kinematic", "Dynamic"};
    int typeIdx = static_cast<int>(rb.type);
    const std::string typeLabel = I18n::T("editor.panel.inspector.physics.type") + "##rb";
    if (ImGui::Combo(typeLabel.c_str(), &typeIdx, typeNames, 3)) {
        // Hito 40 F: undo via EditPropertyCommand<u32> (cambio
        // estructural, atomico — no usa el tracker drag-end).
        const auto oldType = rb.type;
        const auto newType = static_cast<RigidBodyComponent::Type>(typeIdx);
        if (oldType != newType) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldType), static_cast<u32>(newType),
                    [](Entity& en, const u32& v) {
                        en.getComponent<RigidBodyComponent>().type =
                            static_cast<RigidBodyComponent::Type>(v);
                    },
                    "Cambiar RigidBody type");
                h->push(std::move(cmd));
            } else {
                rb.type = newType;
            }
            m_editedThisFrame = true;
        }
    }

    const char* shapeNames[] = {"Box", "Sphere", "Capsule"};
    int shapeIdx = static_cast<int>(rb.shape);
    const std::string shapeLabel = I18n::T("editor.panel.inspector.physics.shape") + "##rb";
    if (ImGui::Combo(shapeLabel.c_str(), &shapeIdx, shapeNames, 3)) {
        // Hito 40 F: undo del shape combo.
        const auto oldShape = rb.shape;
        const auto newShape = static_cast<RigidBodyComponent::Shape>(shapeIdx);
        if (oldShape != newShape) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldShape), static_cast<u32>(newShape),
                    [](Entity& en, const u32& v) {
                        en.getComponent<RigidBodyComponent>().shape =
                            static_cast<RigidBodyComponent::Shape>(v);
                    },
                    "Cambiar RigidBody shape");
                h->push(std::move(cmd));
            } else {
                rb.shape = newShape;
            }
            m_editedThisFrame = true;
        }
    }

    const std::string halfLabel = I18n::T("editor.panel.inspector.physics.half_extents") + "##rb";
    if (ImGui::DragFloat3(halfLabel.c_str(), &rb.halfExtents.x, 0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, rb.halfExtents,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<RigidBodyComponent>().halfExtents = v;
        },
        "Editar rigid body halfExtents");
    if (rb.type == RigidBodyComponent::Type::Dynamic) {
        const std::string massLabel = I18n::T("editor.panel.inspector.physics.mass") + "##rb";
        if (ImGui::DragFloat(massLabel.c_str(), &rb.mass, 0.1f, 0.001f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, rb.mass,
            [](Entity& en, const f32& v) {
                en.getComponent<RigidBodyComponent>().mass = v;
            },
            "Editar rigid body mass");
    }
    // Hito 34 A: friction. Aplica a static + dynamic (el contacto en
    // ambos lados afecta el comportamiento).
    const std::string fricLabel = I18n::T("editor.panel.inspector.physics.friction") + "##rb";
    if (ImGui::DragFloat(fricLabel.c_str(), &rb.friction, 0.01f, 0.0f, 2.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, rb.friction,
        [](Entity& en, const f32& v) {
            en.getComponent<RigidBodyComponent>().friction = v;
        },
        "Editar friction (RigidBody)");
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.physics.body_id_hint", rb.bodyId).c_str());
    ImGui::Separator();
}

} // namespace Mood
