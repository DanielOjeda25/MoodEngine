// F2H24: Inspector — LightComponent + EnvironmentComponent (afines:
// iluminacion + IBL + post-process).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
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

    if (ImGui::Checkbox("enabled##lt", &lt.enabled)) m_editedThisFrame = true;

    const char* items[] = {"Directional", "Point"};
    int current = static_cast<int>(lt.type);
    if (ImGui::Combo("type##lt", &current, items, 2)) {
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
    if (ImGui::ColorEdit3("color##lt", &lt.color.x)) m_editedThisFrame = true;
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, lt.color,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<LightComponent>().color = v;
        },
        "Editar light color");
    if (ImGui::DragFloat("intensity##lt", &lt.intensity, 0.01f, 0.0f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.intensity,
        [](Entity& en, const f32& v) {
            en.getComponent<LightComponent>().intensity = v;
        },
        "Editar light intensity");

    if (lt.type == LightComponent::Type::Point) {
        if (ImGui::DragFloat("radius (m)##lt", &lt.radius, 0.1f, 0.1f, 1000.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.radius,
            [](Entity& en, const f32& v) {
                en.getComponent<LightComponent>().radius = v;
            },
            "Editar light radius");
    } else {
        if (ImGui::DragFloat3("direction##lt", &lt.direction.x, 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        // Hito 16: solo directional puede emitir shadow map (point shadows
        // requeririan cubemap depth, fuera de scope).
        if (ImGui::Checkbox("castShadows##lt", &lt.castShadows)) {
            m_editedThisFrame = true;
        }
    }
    ImGui::Separator();
}

// EnvironmentComponent (Hito 15 Bloque 4)
// Sky + fog + post-process. Solo el primer Environment encontrado en la
// Scene se aplica al frame; varios = warning suave (no aca, en
// EditorApplication). Cambios en vivo via DragFloat / Combo.
void InspectorPanel::renderEnvironmentSection(Entity e) {
    auto& env = e.getComponent<EnvironmentComponent>();
    ImGui::SeparatorText(ICON_FA_TREE " Environment");

    ImGui::TextDisabled("Skybox: %s (asset catalog futuro)",
                         env.skyboxPath.c_str());

    ImGui::Separator();
    ImGui::TextUnformatted("Fog");
    const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
    int fogIdx = static_cast<int>(env.fogMode);
    if (ImGui::Combo("mode##env", &fogIdx, fogModes, 4)) {
        env.fogMode = static_cast<u32>(fogIdx);
        m_editedThisFrame = true;
    }
    if (ImGui::ColorEdit3("color##envfog", &env.fogColor.x)) m_editedThisFrame = true;
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, env.fogColor,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<EnvironmentComponent>().fogColor = v;
        },
        "Editar fog color");
    if (env.fogMode == 1) {
        if (ImGui::DragFloat("start (m)##env", &env.fogLinearStart, 0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearStart,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogLinearStart = v;
            },
            "Editar fog linear start");
        if (ImGui::DragFloat("end (m)##env",   &env.fogLinearEnd,   0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearEnd,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogLinearEnd = v;
            },
            "Editar fog linear end");
    } else if (env.fogMode == 2 || env.fogMode == 3) {
        if (ImGui::DragFloat("density##env", &env.fogDensity, 0.001f, 0.0f, 1.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogDensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogDensity = v;
            },
            "Editar fog density");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Post-process");
    if (ImGui::DragFloat("exposure (EV)##env", &env.exposure, 0.05f, -5.0f, 5.0f)) m_editedThisFrame = true;
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.exposure,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().exposure = v;
        },
        "Editar exposure");
    const char* tonemaps[] = {"None", "Reinhard", "ACES"};
    int toneIdx = static_cast<int>(env.tonemapMode);
    if (ImGui::Combo("tonemap##env", &toneIdx, tonemaps, 3)) {
        env.tonemapMode = static_cast<u32>(toneIdx);
        m_editedThisFrame = true;
    }
    // Hito 18: control del aporte del IBL al ambient. Util cuando el
    // skybox es muy claro y "ahoga" la directional + point lights.
    if (ImGui::SliderFloat("IBL intensity##env",
                            &env.iblIntensity, 0.0f, 2.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.iblIntensity,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().iblIntensity = v;
        },
        "Editar IBL intensity");
    ImGui::Separator();
}

} // namespace Mood
