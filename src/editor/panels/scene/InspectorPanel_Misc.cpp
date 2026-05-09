// F2H24: Inspector — secciones simples (Tag, Camera, Trigger).
// Componentes con UI corta que no justifican su propio archivo.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

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
    if (ImGui::DragFloat("fov (deg)##cam", &cam.fovDeg, 0.1f, 1.0f, 179.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.fovDeg,
        [](Entity& en, const f32& v) {
            en.getComponent<CameraComponent>().fovDeg = v;
        },
        "Editar camera fov");
    if (ImGui::DragFloat("near##cam", &cam.nearPlane, 0.001f, 0.001f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.nearPlane,
        [](Entity& en, const f32& v) {
            en.getComponent<CameraComponent>().nearPlane = v;
        },
        "Editar camera near");
    if (ImGui::DragFloat("far##cam", &cam.farPlane, 0.1f, 1.0f, 10000.0f)) {
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
    if (ImGui::DragFloat3("halfExtents##trig", &tc.halfExtents.x,
                            0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, tc.halfExtents,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<TriggerComponent>().halfExtents = v;
        },
        "Editar trigger halfExtents");
    ImGui::TextDisabled("playerInside: %s",
                         tc.playerInside ? "si" : "no");
    ImGui::Separator();
}

} // namespace Mood
