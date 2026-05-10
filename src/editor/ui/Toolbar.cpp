#include "editor/ui/Toolbar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/i18n/I18n.h"  // F2H43

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Botón rectangular con label y tooltip. Devuelve true si fue clickeado.
// F2H36: el label puede llevar un icono FA prepended (ICON_FA_*) y el
// botón se ensancha al ancho real del texto + icono.
bool toolButton(const char* label, const char* tooltip, bool active = false) {
    // F2H36: bump 72 -> 92 px para acomodar icon + label castellano
    // sin truncar (ej. "Cilindro" + ICON_FA_CIRCLE).
    constexpr ImVec2 kBtnSize{92.0f, 36.0f};
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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.toolbar.no_ui").c_str());
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
    const std::string moveLabel = std::string(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " ") +
        I18n::T("editor.panel.toolbar.move");
    const std::string moveTooltip = I18n::T("editor.panel.toolbar.move_tooltip");
    if (toolButton(moveLabel.c_str(),
                    moveTooltip.c_str())) {
        m_ui->requestGizmoMode(0);
    }
    const std::string rotLabel = std::string(ICON_FA_ROTATE " ") +
        I18n::T("editor.panel.toolbar.rotate");
    const std::string rotTooltip = I18n::T("editor.panel.toolbar.rotate_tooltip");
    if (toolButton(rotLabel.c_str(),
                    rotTooltip.c_str())) {
        m_ui->requestGizmoMode(1);
    }
    const std::string scaleLabel = std::string(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER " ") +
        I18n::T("editor.panel.toolbar.scale");
    const std::string scaleTooltip = I18n::T("editor.panel.toolbar.scale_tooltip");
    if (toolButton(scaleLabel.c_str(),
                    scaleTooltip.c_str())) {
        m_ui->requestGizmoMode(2);
    }

    ImGui::Separator();

    // ---- Brushes (Add Box, Add Cylinder) ----
    const std::string boxLabel = std::string(ICON_FA_CUBE " ") +
        I18n::T("editor.panel.toolbar.box");
    const std::string boxTooltip = I18n::T("editor.panel.toolbar.box_tooltip");
    if (toolButton(boxLabel.c_str(),
                    boxTooltip.c_str())) {
        m_ui->requestProjectAction(ProjectAction::AddBoxBrush);
    }
    const std::string cylLabel = std::string(ICON_FA_CIRCLE " ") +
        I18n::T("editor.panel.toolbar.cylinder");
    const std::string cylTooltip = I18n::T("editor.panel.toolbar.cylinder_tooltip");
    if (toolButton(cylLabel.c_str(),
                    cylTooltip.c_str())) {
        m_ui->requestProjectAction(ProjectAction::AddCylinderBrush);
    }

    ImGui::Separator();

    // ---- Face mode toggle ----
    const bool faceActive = (m_ui->subMode() == EditorSubMode::Face);
    const std::string faceLabel = std::string(ICON_FA_VECTOR_SQUARE " ") +
        I18n::T("editor.panel.toolbar.face");
    const std::string faceTooltip = I18n::T("editor.panel.toolbar.face_tooltip");
    if (toolButton(faceLabel.c_str(),
                    faceTooltip.c_str(),
                    faceActive)) {
        m_ui->requestToggleFaceMode();
    }

    ImGui::End();
}

} // namespace Mood
