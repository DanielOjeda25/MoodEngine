#include "editor/ui/Toolbar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"

#include <imgui.h>

namespace Mood {

namespace {

// Botón cuadrado con label y tooltip. Devuelve true si fue clickeado.
bool toolButton(const char* label, const char* tooltip, bool active = false) {
    constexpr ImVec2 kBtnSize{32.0f, 32.0f};
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                                ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(label, kBtnSize);
    if (active) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && tooltip != nullptr) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

} // namespace

void Toolbar::onImGuiRender() {
    if (!visible) return;

    // Layout vertical compacto. Sin TitleBar para que el dev lo vea
    // como una franja de tools, no como otra ventana mas. Movible y
    // dockable desde Drag/Drop si quiere.
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!ImGui::Begin(name(), &visible, flags)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("EditorUI no inyectado");
        ImGui::End();
        return;
    }

    // ---- Gizmo modes (Translate / Rotate / Scale) ----
    // Highlight del modo activo es nice-to-have; por ahora la Toolbar
    // no conoce el `m_gizmoMode` actual del EditorApplication. Se
    // podria pasar como setter desde EditorApplication en cada frame
    // — diferido hasta que emerja necesidad real (al validar el dev
    // ya ve W/E/R en hover y eso alcanza).
    if (toolButton("T", "Translate (W) - mover entidad")) {
        m_ui->requestGizmoMode(0);
    }
    if (toolButton("R", "Rotate (E) - rotar entidad")) {
        m_ui->requestGizmoMode(1);
    }
    if (toolButton("S", "Scale (R) - escalar entidad")) {
        m_ui->requestGizmoMode(2);
    }

    ImGui::Separator();

    // ---- Brushes (Add Box, Add Cylinder) ----
    if (toolButton("Box", "Anadir Box brush al mapa actual")) {
        m_ui->requestProjectAction(ProjectAction::AddBoxBrush);
    }
    if (toolButton("Cyl", "Anadir Cylinder brush al mapa actual")) {
        m_ui->requestProjectAction(ProjectAction::AddCylinderBrush);
    }

    ImGui::Separator();

    // ---- Face mode toggle ----
    const bool faceActive = (m_ui->subMode() == EditorSubMode::Face);
    if (toolButton("Fce", "Toggle Face mode (3) - editar caras del brush",
                    faceActive)) {
        m_ui->requestToggleFaceMode();
    }

    ImGui::End();
}

} // namespace Mood
