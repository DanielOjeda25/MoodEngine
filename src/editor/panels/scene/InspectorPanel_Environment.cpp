// F2H58 Bloque A: Inspector — EnvironmentComponent en un panel propio
// con secciones colapsables. Pre-F2H58 la funcion vivia en
// InspectorPanel_Light.cpp por accidente historico (Hito 15 las metio
// juntas porque ambos componentes eran "ambiente"). El dispatch en
// InspectorPanel.cpp ya separaba `LightComponent` de
// `EnvironmentComponent`, asi que mover la funcion no cambia que se
// muestra; lo que cambia es como se ven los sub-bloques: cada uno
// (sky/fog, tonemap, bloom, AO) es un CollapsingHeader independiente
// — convencion Unity Volume / Unreal Post Process Volume / Godot
// WorldEnvironment.
//
// Color grading queda como CollapsingHeader propio en el Bloque B de
// F2H58 (este archivo solo se ocupa del reordenamiento).
//
// pushEditIfDone records undo via InspectorEditTracker (Hito 32). Las
// secciones bloom/SSAO propagan en vivo via applyEnvironmentFromScene
// que corre cada frame en SceneRenderer::renderScene.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>

namespace Mood {

void InspectorPanel::renderEnvironmentSection(Entity e) {
    auto& env = e.getComponent<EnvironmentComponent>();
    ImGui::SeparatorText(ICON_FA_TREE " Environment");

    // ----- Sky + Fog -----
    if (ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.fog").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.environment.skybox",
                    env.skyboxPath).c_str());

        const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
        int fogIdx = static_cast<int>(env.fogMode);
        const std::string fogModeLabel =
            I18n::T("editor.panel.inspector.environment.mode") + "##env";
        if (ImGui::Combo(fogModeLabel.c_str(), &fogIdx, fogModes, 4)) {
            env.fogMode = static_cast<u32>(fogIdx);
            m_editedThisFrame = true;
        }
        const std::string fogColorLabel =
            I18n::T("editor.panel.inspector.environment.color") + "##envfog";
        if (ImGui::ColorEdit3(fogColorLabel.c_str(), &env.fogColor.x)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, env.fogColor,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<EnvironmentComponent>().fogColor = v;
            },
            "Editar fog color");
        if (env.fogMode == 1) {
            const std::string startLabel =
                I18n::T("editor.panel.inspector.environment.start") + "##env";
            if (ImGui::DragFloat(startLabel.c_str(), &env.fogLinearStart,
                                  0.1f, 0.0f, 500.0f)) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearStart,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearStart = v;
                },
                "Editar fog linear start");
            const std::string endLabel =
                I18n::T("editor.panel.inspector.environment.end") + "##env";
            if (ImGui::DragFloat(endLabel.c_str(),   &env.fogLinearEnd,
                                  0.1f, 0.0f, 500.0f)) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearEnd,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearEnd = v;
                },
                "Editar fog linear end");
        } else if (env.fogMode == 2 || env.fogMode == 3) {
            const std::string densityLabel =
                I18n::T("editor.panel.inspector.environment.density") + "##env";
            if (ImGui::DragFloat(densityLabel.c_str(), &env.fogDensity,
                                  0.001f, 0.0f, 1.0f)) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogDensity,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogDensity = v;
                },
                "Editar fog density");
        }
    }

    // ----- Tonemap + Exposure + IBL -----
    if (ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.post_process").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::string exposureLabel =
            I18n::T("editor.panel.inspector.environment.exposure") + "##env";
        if (ImGui::DragFloat(exposureLabel.c_str(), &env.exposure,
                              0.05f, -5.0f, 5.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.exposure,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().exposure = v;
            },
            "Editar exposure");

        const char* tonemaps[] = {"None", "Reinhard", "ACES"};
        int toneIdx = static_cast<int>(env.tonemapMode);
        const std::string tonemapLabel =
            I18n::T("editor.panel.inspector.environment.tonemap") + "##env";
        if (ImGui::Combo(tonemapLabel.c_str(), &toneIdx, tonemaps, 3)) {
            env.tonemapMode = static_cast<u32>(toneIdx);
            m_editedThisFrame = true;
        }

        // Hito 18: control del aporte del IBL al ambient. Util cuando el
        // skybox es muy claro y "ahoga" la directional + point lights.
        const std::string iblLabel =
            I18n::T("editor.panel.inspector.environment.ibl_intensity") + "##env";
        if (ImGui::SliderFloat(iblLabel.c_str(),
                                &env.iblIntensity, 0.0f, 2.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.iblIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().iblIntensity = v;
            },
            "Editar IBL intensity");
    }

    // ----- Bloom (F2H55) -----
    if (ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.bloom").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::string bloomEnabledLabel =
            I18n::T("editor.panel.inspector.environment.bloom_enabled") + "##env";
        if (ImGui::Checkbox(bloomEnabledLabel.c_str(), &env.bloomEnabled)) {
            m_editedThisFrame = true;
        }
        if (env.bloomEnabled) {
            const std::string bloomThLabel =
                I18n::T("editor.panel.inspector.environment.bloom_threshold") + "##env";
            if (ImGui::SliderFloat(bloomThLabel.c_str(),
                                    &env.bloomThreshold, 0.0f, 3.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.bloomThreshold,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().bloomThreshold = v;
                },
                "Editar bloom threshold");
            const std::string bloomIntLabel =
                I18n::T("editor.panel.inspector.environment.bloom_intensity") + "##env";
            if (ImGui::SliderFloat(bloomIntLabel.c_str(),
                                    &env.bloomIntensity, 0.0f, 2.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.bloomIntensity,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().bloomIntensity = v;
                },
                "Editar bloom intensity");
            const std::string bloomRadLabel =
                I18n::T("editor.panel.inspector.environment.bloom_radius") + "##env";
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
    }

    // ----- SSAO (F2H56) -----
    if (ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.environment.ssao").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::string ssaoEnabledLabel =
            I18n::T("editor.panel.inspector.environment.ssao_enabled") + "##env";
        if (ImGui::Checkbox(ssaoEnabledLabel.c_str(), &env.ssaoEnabled)) {
            m_editedThisFrame = true;
        }
        if (env.ssaoEnabled) {
            const std::string ssaoRadLabel =
                I18n::T("editor.panel.inspector.environment.ssao_radius") + "##env";
            if (ImGui::SliderFloat(ssaoRadLabel.c_str(),
                                    &env.ssaoRadius, 0.05f, 2.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, env.ssaoRadius,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().ssaoRadius = v;
                },
                "Editar SSAO radius");
            const std::string ssaoIntLabel =
                I18n::T("editor.panel.inspector.environment.ssao_intensity") + "##env";
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
    }
}

} // namespace Mood
