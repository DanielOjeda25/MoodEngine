// F2H24: Inspector — ParticleEmitterComponent (Hito 29 / 37 / 39 / 40).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>
#include <utility>

namespace Mood {

// ParticleEmitterComponent (Hito 29)
// Editar campos en vivo afecta la simulacion del frame siguiente.
// El render del proximo frame ya muestra los nuevos parametros.
void InspectorPanel::renderParticleEmitterSection(Entity e) {
    auto& em = e.getComponent<ParticleEmitterComponent>();
    ImGui::SeparatorText(ICON_FA_FIRE " Particle Emitter");

    const std::string emittingLabel = I18n::T("editor.panel.inspector.particles.emitting") + "##pe";
    if (ImGui::Checkbox(emittingLabel.c_str(), &em.emitting)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.emitting,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().emitting = v;
        },
        "Toggle particle emitting");
    ImGui::SameLine();
    const std::string additiveLabel = I18n::T("editor.panel.inspector.particles.additive") + "##pe";
    if (ImGui::Checkbox(additiveLabel.c_str(), &em.additive)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.additive,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().additive = v;
        },
        "Toggle particle additive");
    ImGui::SameLine();
    const std::string localSpaceLabel = I18n::T("editor.panel.inspector.particles.local_space") + "##pe";
    if (ImGui::Checkbox(localSpaceLabel.c_str(), &em.localSpace)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.localSpace,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().localSpace = v;
        },
        "Toggle particle localSpace");

    // Hito 37 C / 39 A: shape de emision (Point/Box/Sphere/Disc/Cone).
    using ES = ParticleEmitterComponent::EmissionShape;
    const char* shapeNames[] = {"Point", "Box", "Sphere", "Disc", "Cone"};
    int shapeIdx = static_cast<int>(em.emissionShape);
    const std::string emitShapeLabel = I18n::T("editor.panel.inspector.particles.emit_shape") + "##pe";
    if (ImGui::Combo(emitShapeLabel.c_str(), &shapeIdx, shapeNames, 5)) {
        em.emissionShape = static_cast<ES>(shapeIdx);
        m_editedThisFrame = true;
    }
    if (em.emissionShape != ES::Point) {
        const std::string shapeSizeLabel = I18n::T("editor.panel.inspector.particles.shape_size") + "##pe";
        if (ImGui::DragFloat(shapeSizeLabel.c_str(),
                              &em.emissionShapeSize, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, em.emissionShapeSize,
            [](Entity& en, const f32& v) {
                en.getComponent<ParticleEmitterComponent>().emissionShapeSize = v;
            },
            "Editar emission shape size");
    }
    // Hito 40 A: cone axis solo visible si shape == Cone.
    if (em.emissionShape == ES::Cone) {
        const std::string coneAxisLabel = I18n::T("editor.panel.inspector.particles.cone_axis") + "##pe";
        if (ImGui::DragFloat3(coneAxisLabel.c_str(),
                                &em.emissionConeAxis.x, 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.emissionConeAxis,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ParticleEmitterComponent>().emissionConeAxis = v;
            },
            "Editar cone axis");
    }

    const std::string rateLabel = I18n::T("editor.panel.inspector.particles.rate") + "##pe";
    if (ImGui::DragFloat(rateLabel.c_str(), &em.emitRate, 1.0f, 0.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, em.emitRate,
        [](Entity& en, const f32& v) {
            en.getComponent<ParticleEmitterComponent>().emitRate = v;
        },
        "Editar emit rate");
    const std::string lifeLabel = I18n::T("editor.panel.inspector.particles.lifetime") + "##pe";
    if (ImGui::DragFloatRange2(lifeLabel.c_str(),
                                 &em.lifetimeMin, &em.lifetimeMax,
                                 0.05f, 0.05f, 60.0f)) {
        m_editedThisFrame = true;
    }
    // Hito 40 E: undo del DragFloatRange2 (par de floats).
    detail::pushEditIfDone<std::pair<f32, f32>>(m_editTracker, m_ui, e,
        std::pair<f32, f32>{em.lifetimeMin, em.lifetimeMax},
        [](Entity& en, const std::pair<f32, f32>& v) {
            auto& emc = en.getComponent<ParticleEmitterComponent>();
            emc.lifetimeMin = v.first;
            emc.lifetimeMax = v.second;
        },
        "Editar lifetime range");
    const std::string velMinLabel = I18n::T("editor.panel.inspector.particles.vel_min") + "##pe";
    if (ImGui::DragFloat3(velMinLabel.c_str(), &em.velocityMin.x, 0.05f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMin,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<ParticleEmitterComponent>().velocityMin = v;
        },
        "Editar particle velocityMin");
    const std::string velMaxLabel = I18n::T("editor.panel.inspector.particles.vel_max") + "##pe";
    if (ImGui::DragFloat3(velMaxLabel.c_str(), &em.velocityMax.x, 0.05f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMax,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<ParticleEmitterComponent>().velocityMax = v;
        },
        "Editar particle velocityMax");
    const std::string sizeLabel = I18n::T("editor.panel.inspector.particles.size") + "##pe";
    if (ImGui::DragFloatRange2(sizeLabel.c_str(),
                                 &em.sizeStart, &em.sizeEnd,
                                 0.005f, 0.0f, 5.0f)) {
        m_editedThisFrame = true;
    }
    // Hito 40 E: undo del DragFloatRange2 (par de floats) — size.
    detail::pushEditIfDone<std::pair<f32, f32>>(m_editTracker, m_ui, e,
        std::pair<f32, f32>{em.sizeStart, em.sizeEnd},
        [](Entity& en, const std::pair<f32, f32>& v) {
            auto& emc = en.getComponent<ParticleEmitterComponent>();
            emc.sizeStart = v.first;
            emc.sizeEnd   = v.second;
        },
        "Editar size range");
    const std::string colorStartLabel = I18n::T("editor.panel.inspector.particles.color_start") + "##pe";
    if (ImGui::ColorEdit4(colorStartLabel.c_str(), &em.colorStart.x)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorStart,
        [](Entity& en, const glm::vec4& v) {
            en.getComponent<ParticleEmitterComponent>().colorStart = v;
        },
        "Editar particle colorStart");
    const std::string colorEndLabel = I18n::T("editor.panel.inspector.particles.color_end") + "##pe";
    if (ImGui::ColorEdit4(colorEndLabel.c_str(), &em.colorEnd.x)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorEnd,
        [](Entity& en, const glm::vec4& v) {
            en.getComponent<ParticleEmitterComponent>().colorEnd = v;
        },
        "Editar particle colorEnd");
    const std::string gravLabel = I18n::T("editor.panel.inspector.particles.gravity_factor") + "##pe";
    if (ImGui::DragFloat(gravLabel.c_str(), &em.gravityFactor, 0.01f,
                          -2.0f, 2.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, em.gravityFactor,
        [](Entity& en, const f32& v) {
            en.getComponent<ParticleEmitterComponent>().gravityFactor = v;
        },
        "Editar gravity factor (particles)");
    const std::string maxLabel = I18n::T("editor.panel.inspector.particles.max_particles") + "##pe";
    if (ImGui::DragInt(maxLabel.c_str(),
                         reinterpret_cast<int*>(&em.maxParticles),
                         1.0f, 1, 4096)) {
        // Resize lazy en el proximo update; vaciamos pool ahora para
        // que no haya particulas vivas con indices fuera de rango.
        em.alive.clear();
        em.positions.clear();
        em.velocities.clear();
        em.ages.clear();
        em.lifetimes.clear();
        em.aliveCount = 0;
        m_editedThisFrame = true;
    }
    // Hito 36 B: undo del DragInt. El setter del comando reaplica el
    // mismo cleanup de la pool runtime que el handler manual de arriba —
    // sin esto, un undo dejaria particulas vivas con indices >= la
    // nueva capacidad.
    detail::pushEditIfDone<u32>(m_editTracker, m_ui, e, em.maxParticles,
        [](Entity& en, const u32& v) {
            auto& emc = en.getComponent<ParticleEmitterComponent>();
            emc.maxParticles = v;
            emc.alive.clear();
            emc.positions.clear();
            emc.velocities.clear();
            emc.ages.clear();
            emc.lifetimes.clear();
            emc.aliveCount = 0;
        },
        "Editar maxParticles");
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.particles.alive_count",
                em.aliveCount, em.maxParticles).c_str());

    if (m_assets != nullptr) {
        const std::string texPath = m_assets->pathOf(em.texture);
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.particles.texture",
                    texPath).c_str());
    }
    ImGui::Separator();
}

} // namespace Mood
