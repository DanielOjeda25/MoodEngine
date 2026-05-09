#include "editor/ui/MapEditorTopBar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"

#include <imgui.h>

namespace Mood {

namespace {

bool toolButton(const char* label, const char* tooltip, bool active) {
    // Layout columna: ancho = todo el espacio disponible, alto fijo.
    const ImVec2 kBtnSize{ImGui::GetContentRegionAvail().x, 32.0f};
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

} // anonymous

void MapEditorTopBar::onImGuiRender() {
    if (!visible) return;

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar;

    if (!ImGui::Begin(name(), &visible, flags)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("EditorUI no inyectado");
        ImGui::End();
        return;
    }

    const EditorSubMode currentSubMode = m_ui->subMode();
    const bool polyActive = m_ui->polygonDrawActive();

    // F2H30 Bloque C: layout vertical (columna lateral derecha) — un
    // boton por linea, ancho completo. Sub-modos arriba, separador,
    // pincel abajo. Estilo Hammer / Blender.
    if (toolButton("Objeto",
                    "Object Mode (Esc) - mover/rotar/escalar brush entero",
                    currentSubMode == EditorSubMode::Object && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Object);
    }
    if (toolButton("Vertex",
                    "Vertex Mode (1) - mover vertex del brush",
                    currentSubMode == EditorSubMode::Vertex && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Vertex);
    }
    if (toolButton("Edge",
                    "Edge Mode (2) - mover arista del brush",
                    currentSubMode == EditorSubMode::Edge && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Edge);
    }
    if (toolButton("Cara",
                    "Face Mode (3) - editar caras del brush",
                    currentSubMode == EditorSubMode::Face && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Face);
    }

    ImGui::Separator();

    if (toolButton("Pincel",
                    "Pincel poligonal (B) - clickear vertices + Enter cierra",
                    polyActive)) {
        m_ui->requestTogglePolygonDraw();
    }

    ImGui::End();
}

} // namespace Mood
