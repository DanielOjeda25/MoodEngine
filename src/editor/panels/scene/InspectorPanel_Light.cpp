// F2H24: Inspector — LightComponent + EnvironmentComponent (afines:
// iluminacion + IBL + post-process).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"  // F2H43
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

// EnvironmentComponent (Hito 15 Bloque 4)
// Sky + fog + post-process. Solo el primer Environment encontrado en la
// Scene se aplica al frame; varios = warning suave (no aca, en
// EditorApplication). Cambios en vivo via DragFloat / Combo.
void InspectorPanel::renderEnvironmentSection(Entity e) {
    auto& env = e.getComponent<EnvironmentComponent>();
    ImGui::SeparatorText(ICON_FA_TREE " Environment");

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.environment.skybox",
                env.skyboxPath).c_str());

    ImGui::Separator();
    ImGui::TextUnformatted(I18n::T("editor.panel.inspector.environment.fog").c_str());
    const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
    int fogIdx = static_cast<int>(env.fogMode);
    const std::string fogModeLabel = I18n::T("editor.panel.inspector.environment.mode") + "##env";
    if (ImGui::Combo(fogModeLabel.c_str(), &fogIdx, fogModes, 4)) {
        env.fogMode = static_cast<u32>(fogIdx);
        m_editedThisFrame = true;
    }
    const std::string fogColorLabel = I18n::T("editor.panel.inspector.environment.color") + "##envfog";
    if (ImGui::ColorEdit3(fogColorLabel.c_str(), &env.fogColor.x)) m_editedThisFrame = true;
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, env.fogColor,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<EnvironmentComponent>().fogColor = v;
        },
        "Editar fog color");
    if (env.fogMode == 1) {
        const std::string startLabel = I18n::T("editor.panel.inspector.environment.start") + "##env";
        if (ImGui::DragFloat(startLabel.c_str(), &env.fogLinearStart, 0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearStart,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogLinearStart = v;
            },
            "Editar fog linear start");
        const std::string endLabel = I18n::T("editor.panel.inspector.environment.end") + "##env";
        if (ImGui::DragFloat(endLabel.c_str(),   &env.fogLinearEnd,   0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearEnd,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogLinearEnd = v;
            },
            "Editar fog linear end");
    } else if (env.fogMode == 2 || env.fogMode == 3) {
        const std::string densityLabel = I18n::T("editor.panel.inspector.environment.density") + "##env";
        if (ImGui::DragFloat(densityLabel.c_str(), &env.fogDensity, 0.001f, 0.0f, 1.0f)) m_editedThisFrame = true;
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogDensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().fogDensity = v;
            },
            "Editar fog density");
    }

    ImGui::Separator();
    ImGui::TextUnformatted(I18n::T("editor.panel.inspector.environment.post_process").c_str());
    const std::string exposureLabel = I18n::T("editor.panel.inspector.environment.exposure") + "##env";
    if (ImGui::DragFloat(exposureLabel.c_str(), &env.exposure, 0.05f, -5.0f, 5.0f)) m_editedThisFrame = true;
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.exposure,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().exposure = v;
        },
        "Editar exposure");
    const char* tonemaps[] = {"None", "Reinhard", "ACES"};
    int toneIdx = static_cast<int>(env.tonemapMode);
    const std::string tonemapLabel = I18n::T("editor.panel.inspector.environment.tonemap") + "##env";
    if (ImGui::Combo(tonemapLabel.c_str(), &toneIdx, tonemaps, 3)) {
        env.tonemapMode = static_cast<u32>(toneIdx);
        m_editedThisFrame = true;
    }
    // Hito 18: control del aporte del IBL al ambient. Util cuando el
    // skybox es muy claro y "ahoga" la directional + point lights.
    const std::string iblLabel = I18n::T("editor.panel.inspector.environment.ibl_intensity") + "##env";
    if (ImGui::SliderFloat(iblLabel.c_str(),
                            &env.iblIntensity, 0.0f, 2.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.iblIntensity,
        [](Entity& en, const f32& v) {
            en.getComponent<EnvironmentComponent>().iblIntensity = v;
        },
        "Editar IBL intensity");

    // F2H55: bloom. Master switch + threshold + intensity + radius.
    // Cambios propagan en vivo via applyEnvironmentFromScene() que
    // corre cada frame en renderScene.
    ImGui::Separator();
    ImGui::TextUnformatted(I18n::T("editor.panel.inspector.environment.bloom").c_str());
    const std::string bloomEnabledLabel = I18n::T("editor.panel.inspector.environment.bloom_enabled") + "##env";
    if (ImGui::Checkbox(bloomEnabledLabel.c_str(), &env.bloomEnabled)) {
        m_editedThisFrame = true;
    }
    if (env.bloomEnabled) {
        const std::string bloomThLabel = I18n::T("editor.panel.inspector.environment.bloom_threshold") + "##env";
        if (ImGui::SliderFloat(bloomThLabel.c_str(),
                                &env.bloomThreshold, 0.0f, 3.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.bloomThreshold,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomThreshold = v;
            },
            "Editar bloom threshold");
        const std::string bloomIntLabel = I18n::T("editor.panel.inspector.environment.bloom_intensity") + "##env";
        if (ImGui::SliderFloat(bloomIntLabel.c_str(),
                                &env.bloomIntensity, 0.0f, 2.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.bloomIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomIntensity = v;
            },
            "Editar bloom intensity");
        const std::string bloomRadLabel = I18n::T("editor.panel.inspector.environment.bloom_radius") + "##env";
        if (ImGui::SliderFloat(bloomRadLabel.c_str(),
                                &env.bloomRadius, 0.5f, 3.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.bloomRadius,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().bloomRadius = v;
            },
            "Editar bloom radius");
    }

    // F2H56: SSAO (sombras de rincon). Master switch + radio + intensidad.
    ImGui::Separator();
    ImGui::TextUnformatted(I18n::T("editor.panel.inspector.environment.ssao").c_str());
    const std::string ssaoEnabledLabel = I18n::T("editor.panel.inspector.environment.ssao_enabled") + "##env";
    if (ImGui::Checkbox(ssaoEnabledLabel.c_str(), &env.ssaoEnabled)) {
        m_editedThisFrame = true;
    }
    if (env.ssaoEnabled) {
        const std::string ssaoRadLabel = I18n::T("editor.panel.inspector.environment.ssao_radius") + "##env";
        if (ImGui::SliderFloat(ssaoRadLabel.c_str(),
                                &env.ssaoRadius, 0.05f, 2.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.ssaoRadius,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssaoRadius = v;
            },
            "Editar SSAO radius");
        const std::string ssaoIntLabel = I18n::T("editor.panel.inspector.environment.ssao_intensity") + "##env";
        if (ImGui::SliderFloat(ssaoIntLabel.c_str(),
                                &env.ssaoIntensity, 0.0f, 3.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.ssaoIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().ssaoIntensity = v;
            },
            "Editar SSAO intensity");
    }
    ImGui::Separator();
}

} // namespace Mood
