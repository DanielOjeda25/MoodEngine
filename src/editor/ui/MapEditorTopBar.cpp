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
    const MapTool currentTool = m_ui->mapTool();
    const bool polyActive = m_ui->polygonDrawActive();

    // F2H31 Bloque B: arriba, los 3 TOOLS mutually exclusive (que pasa
    // con un drag en empty space del orto). Default Hammer-style =
    // Select. CreateBlock spawnea brushes; Pincel agrega vertices. Solo
    // uno activo a la vez — el highlight refleja el m_mapTool.
    ImGui::TextDisabled("Herramienta");
    if (toolButton("Selecc.",
                    "Selección - drag en vacio dibuja rectangulo de marquee "
                    "y selecciona los brushes que toca",
                    currentTool == MapTool::Select && !polyActive)) {
        m_ui->requestMapTool(MapTool::Select);
    }
    if (toolButton("Bloque",
                    "Block tool - drag en vacio = preview + spawn brush",
                    currentTool == MapTool::CreateBlock && !polyActive)) {
        m_ui->requestMapTool(MapTool::CreateBlock);
    }
    if (toolButton("Pincel",
                    "Pincel poligonal (B) - clicks agregan vertices, Enter cierra",
                    polyActive)) {
        // Pincel sigue manejandose via togglePolygonDrawMode (F2H30 C)
        // que ya cancela y revierte si esta activo.
        m_ui->requestTogglePolygonDraw();
    }
    // F2H32 Bloque B: clip tool.
    if (toolButton("Clip",
                    "Clip tool - 2 clicks definen un plano que splittea "
                    "los brushes selectos. T cycle Front/Back/Both, "
                    "Enter confirma, Esc cancela",
                    currentTool == MapTool::Clip && !polyActive)) {
        m_ui->requestMapTool(MapTool::Clip);
    }

    ImGui::Separator();

    // F2H30 Bloque C: sub-modos del SelectionSet (que NIVEL de geometria
    // se manipula). Ortogonal al Tool de arriba. Solo aplica cuando hay
    // un brush selecto.
    ImGui::TextDisabled("Sub-modo");
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

    // F2H31 Bloque C: toggle snap-to-vertex (tecla V tambien dispara).
    ImGui::TextDisabled("Snap");
    if (toolButton("Snap V",
                    "Snap a vertex (V) - snapea al vertex mas cercano de un "
                    "brush existente en lugar del grid",
                    m_ui->snapToVertexEnabled())) {
        m_ui->requestToggleSnapToVertex();
    }

    ImGui::Separator();

    // F2H35 Bloque E: toggle labels arriba de point entities en
    // perspective + ortos. Default ON. El boton highlight refleja el
    // state actual.
    ImGui::TextDisabled("Visualizacion");
    if (toolButton("Nombres",
                    "Mostrar/ocultar nombres (TagComponent.name) arriba "
                    "de los iconos de Light/Audio/Trigger/Camera/Particle "
                    "en el perspective y en los 3 ortos. Persistido "
                    "por proyecto.",
                    m_ui->showEntityLabels())) {
        m_ui->requestToggleEntityLabels();
    }

    ImGui::Separator();

    // F2H32 Bloque C: carve UI button. Click destructivo — resta el
    // brush activo por todos los brushes que intersectan su AABB.
    // Sin keyboard shortcut para evitar accidentes.
    ImGui::TextDisabled("Acciones");
    if (toolButton("Carve",
                    "Carve - resta el brush activo por todos los brushes "
                    "que lo atraviesan. Ctrl+Z deshace.",
                    false)) {
        m_ui->requestCarve();
    }

    ImGui::End();
}

} // namespace Mood
