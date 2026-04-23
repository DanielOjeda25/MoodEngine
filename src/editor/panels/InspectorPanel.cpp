#include "editor/panels/InspectorPanel.h"

#include "editor/EditorUI.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"

#include <imgui.h>

#include <cstdio>

namespace Mood {

void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("Editor UI no inyectado");
        ImGui::End();
        return;
    }

    Entity e = m_ui->selectedEntity();
    if (!e) {
        ImGui::TextDisabled("No hay entidad seleccionada");
        ImGui::TextDisabled("Click en el panel Hierarchy para elegir una.");
        ImGui::End();
        return;
    }

    // --- TagComponent ---
    if (e.hasComponent<TagComponent>()) {
        auto& tag = e.getComponent<TagComponent>();
        ImGui::TextDisabled("Tag");
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", tag.name.c_str());
        if (ImGui::InputText("##tag", buf, sizeof(buf))) {
            tag.name = buf;
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- TransformComponent ---
    if (e.hasComponent<TransformComponent>()) {
        auto& t = e.getComponent<TransformComponent>();
        ImGui::TextDisabled("Transform");
        if (ImGui::DragFloat3("position##tr", &t.position.x, 0.05f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat3("rotation (deg)##tr", &t.rotationEuler.x, 0.5f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat3("scale##tr", &t.scale.x, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- MeshRendererComponent ---
    if (e.hasComponent<MeshRendererComponent>()) {
        const auto& mr = e.getComponent<MeshRendererComponent>();
        ImGui::TextDisabled("MeshRenderer");
        ImGui::Text("mesh: %s", mr.mesh != nullptr ? "set" : "null");
        ImGui::Text("texture id: %u", mr.texture);
        ImGui::TextDisabled("(editor de materiales en hitos posteriores)");
        ImGui::Separator();
    }

    // --- CameraComponent ---
    if (e.hasComponent<CameraComponent>()) {
        auto& cam = e.getComponent<CameraComponent>();
        ImGui::TextDisabled("Camera");
        if (ImGui::DragFloat("fov (deg)##cam", &cam.fovDeg, 0.1f, 1.0f, 179.0f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat("near##cam", &cam.nearPlane, 0.001f, 0.001f, 100.0f)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat("far##cam", &cam.farPlane, 0.1f, 1.0f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- LightComponent ---
    if (e.hasComponent<LightComponent>()) {
        auto& lt = e.getComponent<LightComponent>();
        ImGui::TextDisabled("Light");
        const char* items[] = {"Directional", "Point"};
        int current = static_cast<int>(lt.type);
        if (ImGui::Combo("type##lt", &current, items, 2)) {
            lt.type = static_cast<LightComponent::Type>(current);
            m_editedThisFrame = true;
        }
        if (ImGui::ColorEdit3("color##lt", &lt.color.x)) {
            m_editedThisFrame = true;
        }
        if (ImGui::DragFloat("intensity##lt", &lt.intensity, 0.01f, 0.0f, 100.0f)) {
            m_editedThisFrame = true;
        }
    }

    ImGui::End();
}

} // namespace Mood
