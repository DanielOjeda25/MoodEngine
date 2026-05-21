// F2H24: Inspector — secciones simples (Tag, Camera, Trigger).
// Componentes con UI corta que no justifican su propio archivo.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace Mood {

void InspectorPanel::renderTagSection(Entity e) {
    auto& tag = e.getComponent<TagComponent>();
    ImGui::SeparatorText(ICON_FA_TAG " Tag");
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", tag.name.c_str());
    if (ImGui::InputText("##tag", buf, sizeof(buf))) {
        tag.name = buf;
        m_editedThisFrame = true;
    }
    // Hito 32 D: undo/redo del nombre. InputText reporta IsItemDeactivatedAfterEdit
    // cuando el dev sale del campo (Tab, click fuera, Enter).
    detail::pushEditIfDone<std::string>(m_editTracker, m_ui, e, tag.name,
        [](Entity& en, const std::string& v) {
            en.getComponent<TagComponent>().name = v;
        },
        "Renombrar entidad");
    ImGui::Separator();
}

void InspectorPanel::renderCameraSection(Entity e) {
    auto& cam = e.getComponent<CameraComponent>();
    ImGui::SeparatorText(ICON_FA_VIDEO " Camera");
    const std::string fovLabel = I18n::T("editor.panel.inspector.camera.fov") + "##cam";
    if (ImGui::DragFloat(fovLabel.c_str(), &cam.fovDeg, 0.1f, 1.0f, 179.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.fovDeg,
        [](Entity& en, const f32& v) {
            en.getComponent<CameraComponent>().fovDeg = v;
        },
        "Editar camera fov");
    const std::string nearLabel = I18n::T("editor.panel.inspector.camera.near") + "##cam";
    if (ImGui::DragFloat(nearLabel.c_str(), &cam.nearPlane, 0.001f, 0.001f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.nearPlane,
        [](Entity& en, const f32& v) {
            en.getComponent<CameraComponent>().nearPlane = v;
        },
        "Editar camera near");
    const std::string farLabel = I18n::T("editor.panel.inspector.camera.far") + "##cam";
    if (ImGui::DragFloat(farLabel.c_str(), &cam.farPlane, 0.1f, 1.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.farPlane,
        [](Entity& en, const f32& v) {
            en.getComponent<CameraComponent>().farPlane = v;
        },
        "Editar camera far");
    ImGui::Separator();
}

void InspectorPanel::renderTriggerSection(Entity e) {
    auto& tc = e.getComponent<TriggerComponent>();
    ImGui::SeparatorText(ICON_FA_BORDER_NONE " Trigger");
    const std::string halfLabel = I18n::T("editor.panel.inspector.trigger.half_extents") + "##trig";
    if (ImGui::DragFloat3(halfLabel.c_str(), &tc.halfExtents.x,
                            0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, tc.halfExtents,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<TriggerComponent>().halfExtents = v;
        },
        "Editar trigger halfExtents");
    ImGui::TextDisabled("%s",
        I18n::T(tc.playerInside ? "editor.panel.inspector.trigger.player_inside_yes"
                                  : "editor.panel.inspector.trigger.player_inside_no").c_str());
    ImGui::Separator();
}

void InspectorPanel::renderForceFieldSection(Entity e) {
    auto& ff = e.getComponent<ForceFieldComponent>();
    ImGui::SeparatorText(ICON_FA_MAGNET " Force Field");

    // --- Shape combo + parametro de la zona ---
    const char* shapeNames[] = {"Box", "Sphere"};
    int shapeIdx = static_cast<int>(ff.shape);
    const std::string shapeLabel = I18n::T("editor.panel.inspector.force_field.shape") + "##ff";
    if (ImGui::Combo(shapeLabel.c_str(), &shapeIdx, shapeNames, 2)) {
        ff.shape = static_cast<ForceFieldComponent::Shape>(shapeIdx);
        m_editedThisFrame = true;
    }
    if (ff.shape == ForceFieldComponent::Shape::Box) {
        const std::string heLabel = I18n::T("editor.panel.inspector.force_field.half_extents") + "##ff";
        if (ImGui::DragFloat3(heLabel.c_str(), &ff.halfExtents.x, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, ff.halfExtents,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ForceFieldComponent>().halfExtents = v;
            },
            "Editar force field halfExtents");
    } else {
        const std::string rLabel = I18n::T("editor.panel.inspector.force_field.radius") + "##ff";
        if (ImGui::DragFloat(rLabel.c_str(), &ff.radius, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, ff.radius,
            [](Entity& en, const f32& v) {
                en.getComponent<ForceFieldComponent>().radius = v;
            },
            "Editar force field radius");
    }

    // --- Mode combo + parametro especifico ---
    const char* modeNames[] = {"Directional", "Radial"};
    int modeIdx = static_cast<int>(ff.mode);
    const std::string modeLabel = I18n::T("editor.panel.inspector.force_field.mode") + "##ff";
    if (ImGui::Combo(modeLabel.c_str(), &modeIdx, modeNames, 2)) {
        ff.mode = static_cast<ForceFieldComponent::Mode>(modeIdx);
        m_editedThisFrame = true;
    }
    if (ff.mode == ForceFieldComponent::Mode::Directional) {
        const std::string dirLabel = I18n::T("editor.panel.inspector.force_field.direction") + "##ff";
        if (ImGui::DragFloat3(dirLabel.c_str(), &ff.direction.x, 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, ff.direction,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ForceFieldComponent>().direction = v;
            },
            "Editar force field direction");
    } else {
        const std::string falloffLabel = I18n::T("editor.panel.inspector.force_field.linear_falloff") + "##ff";
        if (ImGui::Checkbox(falloffLabel.c_str(), &ff.linearFalloff)) {
            m_editedThisFrame = true;
        }
    }

    // --- Strength (compartido) ---
    const std::string sLabel = I18n::T("editor.panel.inspector.force_field.strength") + "##ff";
    if (ImGui::DragFloat(sLabel.c_str(), &ff.strength, 0.5f, -10000.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    detail::helpMarker(I18n::T("editor.panel.inspector.force_field.strength_help").c_str());
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, ff.strength,
        [](Entity& en, const f32& v) {
            en.getComponent<ForceFieldComponent>().strength = v;
        },
        "Editar force field strength");

    // --- Toggles ---
    const std::string imLabel = I18n::T("editor.panel.inspector.force_field.ignore_mass") + "##ff";
    if (ImGui::Checkbox(imLabel.c_str(), &ff.ignoreMass)) m_editedThisFrame = true;
    detail::helpMarker(I18n::T("editor.panel.inspector.force_field.ignore_mass_help").c_str());
    const std::string enLabel = I18n::T("editor.panel.inspector.force_field.enabled") + "##ff";
    if (ImGui::Checkbox(enLabel.c_str(), &ff.enabled)) m_editedThisFrame = true;

    ImGui::Separator();
}

} // namespace Mood
