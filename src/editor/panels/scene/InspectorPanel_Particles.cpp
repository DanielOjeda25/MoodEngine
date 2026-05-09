// F2H24: Inspector — ParticleEmitterComponent (Hito 29 / 37 / 39 / 40).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "engine/assets/manager/AssetManager.h"
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

    if (ImGui::Checkbox("emitting##pe", &em.emitting)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.emitting,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().emitting = v;
        },
        "Toggle particle emitting");
    ImGui::SameLine();
    if (ImGui::Checkbox("additive##pe", &em.additive)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.additive,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().additive = v;
        },
        "Toggle particle additive");
    ImGui::SameLine();
    if (ImGui::Checkbox("localSpace##pe", &em.localSpace)) m_editedThisFrame = true;
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, em.localSpace,
        [](Entity& en, const bool& v) {
            en.getComponent<ParticleEmitterComponent>().localSpace = v;
        },
        "Toggle particle localSpace");

    // Hito 37 C / 39 A: shape de emision (Point/Box/Sphere/Disc/Cone).
    using ES = ParticleEmitterComponent::EmissionShape;
    const char* shapeNames[] = {"Point", "Box", "Sphere", "Disc", "Cone"};
    int shapeIdx = static_cast<int>(em.emissionShape);
    if (ImGui::Combo("emit shape##pe", &shapeIdx, shapeNames, 5)) {
        em.emissionShape = static_cast<ES>(shapeIdx);
        m_editedThisFrame = true;
    }
    if (em.emissionShape != ES::Point) {
        if (ImGui::DragFloat("shape size (m)##pe",
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
        if (ImGui::DragFloat3("cone axis##pe",
                                &em.emissionConeAxis.x, 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.emissionConeAxis,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ParticleEmitterComponent>().emissionConeAxis = v;
            },
            "Editar cone axis");
    }

    if (ImGui::DragFloat("rate (1/s)##pe", &em.emitRate, 1.0f, 0.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, em.emitRate,
        [](Entity& en, const f32& v) {
            en.getComponent<ParticleEmitterComponent>().emitRate = v;
        },
        "Editar emit rate");
    if (ImGui::DragFloatRange2("lifetime (s)##pe",
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
    if (ImGui::DragFloat3("velMin (m/s)##pe", &em.velocityMin.x, 0.05f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMin,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<ParticleEmitterComponent>().velocityMin = v;
        },
        "Editar particle velocityMin");
    if (ImGui::DragFloat3("velMax (m/s)##pe", &em.velocityMax.x, 0.05f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMax,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<ParticleEmitterComponent>().velocityMax = v;
        },
        "Editar particle velocityMax");
    if (ImGui::DragFloatRange2("size (m)##pe",
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
    if (ImGui::ColorEdit4("colorStart##pe", &em.colorStart.x)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorStart,
        [](Entity& en, const glm::vec4& v) {
            en.getComponent<ParticleEmitterComponent>().colorStart = v;
        },
        "Editar particle colorStart");
    if (ImGui::ColorEdit4("colorEnd##pe", &em.colorEnd.x)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorEnd,
        [](Entity& en, const glm::vec4& v) {
            en.getComponent<ParticleEmitterComponent>().colorEnd = v;
        },
        "Editar particle colorEnd");
    if (ImGui::DragFloat("gravityFactor##pe", &em.gravityFactor, 0.01f,
                          -2.0f, 2.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, em.gravityFactor,
        [](Entity& en, const f32& v) {
            en.getComponent<ParticleEmitterComponent>().gravityFactor = v;
        },
        "Editar gravity factor (particles)");
    if (ImGui::DragInt("maxParticles##pe",
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
    ImGui::TextDisabled("vivas: %u / %u",
                         em.aliveCount, em.maxParticles);

    if (m_assets != nullptr) {
        const std::string texPath = m_assets->pathOf(em.texture);
        ImGui::TextDisabled("textura: %s", texPath.c_str());
    }
    ImGui::Separator();
}

} // namespace Mood
