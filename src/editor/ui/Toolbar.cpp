#include "editor/ui/Toolbar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"

#include <imgui.h>

namespace Mood {

namespace {

// Botón rectangular con label y tooltip. Devuelve true si fue clickeado.
// F2H22 polish: usar palabras cortas en español ("Mover" / "Rotar" /
// "Escala") en lugar de letras solas (T/R/S) — el dev al validar
// confirmo que las letras no le quedaban claras. Iconos verdaderos
// (FontAwesome / IcoMoon) requieren mergear una font icon-pack en el
// init de ImGui — anotado como pendiente futuro.
bool toolButton(const char* label, const char* tooltip, bool active = false) {
    constexpr ImVec2 kBtnSize{72.0f, 36.0f};
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
    // F2H30 Bloque D iter 3: hibrido Maya + Blender via double-tap.
    //   W           -> Translate gizmo.
    //   E (1 tap)   -> Scale gizmo per-axis.
    //   E (2 taps)  -> modal Scale uniforme (cursor distance).
    //   R (1 tap)   -> Rotate gizmo rings.
    //   R (2 taps)  -> modal Rotate libre (cursor angle, anillo visual).
    if (toolButton("Mover",   "Translate (W) - mover via gizmo arrows")) {
        m_ui->requestGizmoMode(0);
    }
    if (toolButton("Rotar",   "Rotate (R) - rings; doble-tap R = modal angulo libre")) {
        m_ui->requestGizmoMode(1);
    }
    if (toolButton("Escala",  "Scale (E) - per-axis; doble-tap E = modal uniforme")) {
        m_ui->requestGizmoMode(2);
    }

    ImGui::Separator();

    // ---- Brushes (Add Box, Add Cylinder) ----
    if (toolButton("Box",     "Anadir Box brush al mapa actual")) {
        m_ui->requestProjectAction(ProjectAction::AddBoxBrush);
    }
    if (toolButton("Cilindro", "Anadir Cylinder brush al mapa actual")) {
        m_ui->requestProjectAction(ProjectAction::AddCylinderBrush);
    }

    ImGui::Separator();

    // ---- Face mode toggle ----
    const bool faceActive = (m_ui->subMode() == EditorSubMode::Face);
    if (toolButton("Cara",
                    "Toggle Face mode (3) - editar caras del brush",
                    faceActive)) {
        m_ui->requestToggleFaceMode();
    }

    ImGui::End();
}

} // namespace Mood
