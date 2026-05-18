// F2H24: Inspector — LightComponent. EnvironmentComponent vive en
// InspectorPanel_Environment.cpp desde F2H58 Bloque A (pre-F2H58 estaba
// aca por accidente historico).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>

namespace Mood {

// LightComponent (Hito 11)
// Activado: tiene type / color / intensity (Hito 7) + radius (point) +
// direction (directional) + enabled.
void InspectorPanel::renderLightSection(Entity e) {
    auto& lt = e.getComponent<LightComponent>();
    ImGui::SeparatorText(ICON_FA_LIGHTBULB " Light");

    const std::string enabledLabel = I18n::T("editor.panel.inspector.light.enabled") + "##lt";
    if (ImGui::Checkbox(enabledLabel.c_str(), &lt.enabled)) m_editedThisFrame = true;

    const char* items[] = {"Directional", "Point"};
    int current = static_cast<int>(lt.type);
    const std::string typeLabel = I18n::T("editor.panel.inspector.light.type") + "##lt";
    if (ImGui::Combo(typeLabel.c_str(), &current, items, 2)) {
        // Hito 40 F: undo del light type combo.
        const auto oldType = lt.type;
        const auto newType = static_cast<LightComponent::Type>(current);
        if (oldType != newType) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldType), static_cast<u32>(newType),
                    [](Entity& en, const u32& v) {
                        en.getComponent<LightComponent>().type =
                            static_cast<LightComponent::Type>(v);
                    },
                    "Cambiar Light type");
                h->push(std::move(cmd));
            } else {
                lt.type = newType;
            }
            m_editedThisFrame = true;
        }
    }
    const std::string colorLabel = I18n::T("editor.panel.inspector.light.color") + "##lt";
    if (ImGui::ColorEdit3(colorLabel.c_str(), &lt.color.x)) m_editedThisFrame = true;
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, lt.color,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<LightComponent>().color = v;
        },
        "Editar light color");
    const std::string intensityLabel = I18n::T("editor.panel.inspector.light.intensity") + "##lt";
    if (ImGui::DragFloat(intensityLabel.c_str(), &lt.intensity, 0.01f, 0.0f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.intensity,
        [](Entity& en, const f32& v) {
            en.getComponent<LightComponent>().intensity = v;
        },
        "Editar light intensity");

    if (lt.type == LightComponent::Type::Point) {
        const std::string radiusLabel = I18n::T("editor.panel.inspector.light.radius") + "##lt";
        if (ImGui::DragFloat(radiusLabel.c_str(), &lt.radius, 0.1f, 0.1f, 1000.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.radius,
            [](Entity& en, const f32& v) {
                en.getComponent<LightComponent>().radius = v;
            },
            "Editar light radius");
    } else {
        const std::string dirLabel = I18n::T("editor.panel.inspector.light.direction") + "##lt";
        if (ImGui::DragFloat3(dirLabel.c_str(), &lt.direction.x, 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        // Hito 16: solo directional puede emitir shadow map (point shadows
        // requeririan cubemap depth, fuera de scope).
        const std::string castLabel = I18n::T("editor.panel.inspector.light.cast_shadows") + "##lt";
        if (ImGui::Checkbox(castLabel.c_str(), &lt.castShadows)) {
            m_editedThisFrame = true;
        }
    }
    ImGui::Separator();
}

} // namespace Mood
