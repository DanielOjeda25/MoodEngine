// F2H65: Inspector — JointComponent (Hinge / Distance / Point).
//
// Edita el constraint que une esta entidad ("body A") con otra ("body B").
// El target se asigna via drag-drop desde el HierarchyPanel (payload
// `MOOD_ENTITY` con el raw entt::entity handle). Todos los edits son
// undoable via EditPropertyCommand / pushEditIfDone.
//
// Cualquier cambio en type/pivot/axis/limits/target setea
// `joint.dirty = true` para que el PhysicsSystem destruya el constraint
// viejo y lo recree en el proximo tick.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <cstring>
#include <memory>
#include <string>

namespace Mood {

void InspectorPanel::renderJointSection(Entity e) {
    auto& joint = e.getComponent<JointComponent>();
    ImGui::SeparatorText(ICON_FA_LINK " Joint");

    // --- Type combo ---
    const char* typeNames[] = {"Hinge", "Distance", "Point"};
    int typeIdx = static_cast<int>(joint.type);
    const std::string typeLabel = I18n::T("editor.panel.inspector.joint.type") + "##joint";
    if (ImGui::Combo(typeLabel.c_str(), &typeIdx, typeNames, 3)) {
        const auto oldType = joint.type;
        const auto newType = static_cast<JointComponent::Type>(typeIdx);
        if (oldType != newType) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldType), static_cast<u32>(newType),
                    [](Entity& en, const u32& v) {
                        auto& j = en.getComponent<JointComponent>();
                        j.type = static_cast<JointComponent::Type>(v);
                        j.dirty = true;
                    },
                    "Cambiar Joint type");
                h->push(std::move(cmd));
            } else {
                joint.type = newType;
                joint.dirty = true;
            }
            m_editedThisFrame = true;
        }
    }

    // --- Target entity (drop target) ---
    // Mostramos el tag de la entidad apuntada, o un placeholder. El boton
    // entero actua como drop target del payload `MOOD_ENTITY`.
    Scene* scene = m_ui ? m_ui->scene() : nullptr;
    std::string targetLabel;
    if (joint.targetEntity == kJointNoTarget) {
        targetLabel = I18n::T("editor.panel.inspector.joint.target_unassigned");
    } else if (scene != nullptr) {
        const auto handle = static_cast<entt::entity>(joint.targetEntity);
        Entity tgt = scene->entityFromHandle(handle);
        if (static_cast<bool>(tgt) && tgt.hasComponent<TagComponent>()) {
            targetLabel = tgt.getComponent<TagComponent>().name;
        } else {
            targetLabel = I18n::T("editor.panel.inspector.joint.target_dead",
                                   static_cast<u32>(handle));
        }
    } else {
        targetLabel = I18n::T("editor.panel.inspector.joint.target_unassigned");
    }

    const std::string targetCaption = I18n::T("editor.panel.inspector.joint.target");
    ImGui::TextUnformatted(targetCaption.c_str());
    detail::helpMarker(
        I18n::T("editor.panel.inspector.joint.target_help").c_str());

    // Boton ancho-full que muestra el target actual y acepta drag-drop.
    // Pintamos verde tenue si hay un drag activo del tipo correcto — UX
    // estandar para que el dev sepa "aca puedo soltar".
    const bool dragActive = detail::isDragActiveOfType("MOOD_ENTITY");
    if (dragActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(0.20f, 0.55f, 0.25f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.25f, 0.65f, 0.30f, 0.80f));
    }
    ImGui::Button(targetLabel.c_str(), ImVec2(-1.0f, 0.0f));
    if (dragActive) {
        ImGui::PopStyleColor(2);
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("MOOD_ENTITY")) {
            entt::entity droppedHandle = entt::null;
            if (payload->DataSize == sizeof(droppedHandle)) {
                std::memcpy(&droppedHandle, payload->Data, sizeof(droppedHandle));
            }
            const u32 droppedRaw = static_cast<u32>(droppedHandle);
            // Evitar self-link (entity dropea a si misma -> ignorar).
            if (droppedHandle != entt::null &&
                droppedHandle != e.handle() &&
                droppedRaw != joint.targetEntity) {
                HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                const u32 oldTarget = joint.targetEntity;
                if (h != nullptr) {
                    auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                        e, oldTarget, droppedRaw,
                        [](Entity& en, const u32& v) {
                            auto& j = en.getComponent<JointComponent>();
                            j.targetEntity = v;
                            j.dirty = true;
                        },
                        "Asignar Joint target");
                    h->push(std::move(cmd));
                } else {
                    joint.targetEntity = droppedRaw;
                    joint.dirty = true;
                }
                m_editedThisFrame = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Warning si el target esta asignado pero no tiene RigidBody — el
    // PhysicsSystem no podra materializar el constraint hasta que lo
    // tenga. Texto amarillo para que el dev lo note sin error rojo.
    if (joint.targetEntity != kJointNoTarget && scene != nullptr) {
        const auto handle = static_cast<entt::entity>(joint.targetEntity);
        Entity tgt = scene->entityFromHandle(handle);
        if (static_cast<bool>(tgt) && !tgt.hasComponent<RigidBodyComponent>()) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(1.00f, 0.85f, 0.25f, 1.00f));
            ImGui::TextWrapped("%s",
                I18n::T("editor.panel.inspector.joint.body_b_missing").c_str());
            ImGui::PopStyleColor();
        }
    }

    // --- Pivot local ---
    const std::string pivotLabel = I18n::T("editor.panel.inspector.joint.pivot_local") + "##joint";
    if (ImGui::DragFloat3(pivotLabel.c_str(), &joint.pivotLocal.x, 0.05f, -1000.0f, 1000.0f)) {
        joint.dirty = true;
        m_editedThisFrame = true;
    }
    detail::helpMarker(
        I18n::T("editor.panel.inspector.joint.pivot_local_help").c_str());
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, joint.pivotLocal,
        [](Entity& en, const glm::vec3& v) {
            auto& j = en.getComponent<JointComponent>();
            j.pivotLocal = v;
            j.dirty = true;
        },
        "Editar Joint pivotLocal");

    // --- Conditional fields per type ---
    if (joint.type == JointComponent::Type::Hinge) {
        const std::string axisLabel = I18n::T("editor.panel.inspector.joint.axis_local") + "##joint";
        if (ImGui::DragFloat3(axisLabel.c_str(), &joint.axisLocal.x, 0.01f, -1.0f, 1.0f)) {
            joint.dirty = true;
            m_editedThisFrame = true;
        }
        detail::helpMarker(
            I18n::T("editor.panel.inspector.joint.axis_local_help").c_str());
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, joint.axisLocal,
            [](Entity& en, const glm::vec3& v) {
                auto& j = en.getComponent<JointComponent>();
                j.axisLocal = v;
                j.dirty = true;
            },
            "Editar Joint axisLocal");

        const std::string limMinLabel = I18n::T("editor.panel.inspector.joint.limit_min") + "##joint";
        if (ImGui::DragFloat(limMinLabel.c_str(), &joint.limitMinDeg, 1.0f, -180.0f, 180.0f)) {
            joint.dirty = true;
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, joint.limitMinDeg,
            [](Entity& en, const f32& v) {
                auto& j = en.getComponent<JointComponent>();
                j.limitMinDeg = v;
                j.dirty = true;
            },
            "Editar Joint limitMinDeg");

        const std::string limMaxLabel = I18n::T("editor.panel.inspector.joint.limit_max") + "##joint";
        if (ImGui::DragFloat(limMaxLabel.c_str(), &joint.limitMaxDeg, 1.0f, -180.0f, 180.0f)) {
            joint.dirty = true;
            m_editedThisFrame = true;
        }
        detail::helpMarker(
            I18n::T("editor.panel.inspector.joint.limit_help").c_str());
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, joint.limitMaxDeg,
            [](Entity& en, const f32& v) {
                auto& j = en.getComponent<JointComponent>();
                j.limitMaxDeg = v;
                j.dirty = true;
            },
            "Editar Joint limitMaxDeg");
    } else if (joint.type == JointComponent::Type::Distance) {
        const std::string minLabel = I18n::T("editor.panel.inspector.joint.min_distance") + "##joint";
        if (ImGui::DragFloat(minLabel.c_str(), &joint.minDistance, 0.05f, 0.0f, 1000.0f)) {
            joint.dirty = true;
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, joint.minDistance,
            [](Entity& en, const f32& v) {
                auto& j = en.getComponent<JointComponent>();
                j.minDistance = v;
                j.dirty = true;
            },
            "Editar Joint minDistance");

        const std::string maxLabel = I18n::T("editor.panel.inspector.joint.max_distance") + "##joint";
        if (ImGui::DragFloat(maxLabel.c_str(), &joint.maxDistance, 0.05f, 0.0f, 1000.0f)) {
            joint.dirty = true;
            m_editedThisFrame = true;
        }
        detail::helpMarker(
            I18n::T("editor.panel.inspector.joint.distance_help").c_str());
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, joint.maxDistance,
            [](Entity& en, const f32& v) {
                auto& j = en.getComponent<JointComponent>();
                j.maxDistance = v;
                j.dirty = true;
            },
            "Editar Joint maxDistance");
    }
    // Point: sin campos extra — los 3 ejes rotacion son libres por defecto.

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.joint.constraint_id_hint",
                joint.constraintId).c_str());
    ImGui::Separator();
}

} // namespace Mood
