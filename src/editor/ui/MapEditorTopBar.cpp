#include "editor/ui/MapEditorTopBar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/i18n/I18n.h"  // F2H43

#include <imgui.h>

#include <string>

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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.map_tools.no_ui").c_str());
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
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.map_tools.tool").c_str());
    const std::string selectLabel = std::string(ICON_FA_ARROW_POINTER " ") +
        I18n::T("editor.panel.map_tools.select");
    const std::string selectTooltip = I18n::T("editor.panel.map_tools.select_tooltip");
    if (toolButton(selectLabel.c_str(),
                    selectTooltip.c_str(),
                    currentTool == MapTool::Select && !polyActive)) {
        m_ui->requestMapTool(MapTool::Select);
    }
    const std::string blockLabel = std::string(ICON_FA_CUBE " ") +
        I18n::T("editor.panel.map_tools.block");
    const std::string blockTooltip = I18n::T("editor.panel.map_tools.block_tooltip");
    if (toolButton(blockLabel.c_str(),
                    blockTooltip.c_str(),
                    currentTool == MapTool::CreateBlock && !polyActive)) {
        m_ui->requestMapTool(MapTool::CreateBlock);
    }
    const std::string brushLabel = std::string(ICON_FA_PAINTBRUSH " ") +
        I18n::T("editor.panel.map_tools.brush");
    const std::string brushTooltip = I18n::T("editor.panel.map_tools.brush_tooltip");
    if (toolButton(brushLabel.c_str(),
                    brushTooltip.c_str(),
                    polyActive)) {
        // Pincel sigue manejandose via togglePolygonDrawMode (F2H30 C)
        // que ya cancela y revierte si esta activo.
        m_ui->requestTogglePolygonDraw();
    }
    // F2H32 Bloque B: clip tool.
    const std::string clipLabel = std::string(ICON_FA_SCISSORS " ") +
        I18n::T("editor.panel.map_tools.clip");
    const std::string clipTooltip = I18n::T("editor.panel.map_tools.clip_tooltip");
    if (toolButton(clipLabel.c_str(),
                    clipTooltip.c_str(),
                    currentTool == MapTool::Clip && !polyActive)) {
        m_ui->requestMapTool(MapTool::Clip);
    }

    ImGui::Separator();

    // F2H30 Bloque C: sub-modos del SelectionSet (que NIVEL de geometria
    // se manipula). Ortogonal al Tool de arriba. Solo aplica cuando hay
    // un brush selecto.
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.map_tools.sub_mode").c_str());
    const std::string objLabel = std::string(ICON_FA_OBJECT_GROUP " ") +
        I18n::T("editor.panel.map_tools.object");
    const std::string objTooltip = I18n::T("editor.panel.map_tools.object_tooltip");
    if (toolButton(objLabel.c_str(),
                    objTooltip.c_str(),
                    currentSubMode == EditorSubMode::Object && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Object);
    }
    const std::string vertLabel = std::string(ICON_FA_CIRCLE_DOT " ") +
        I18n::T("editor.panel.map_tools.vertex");
    const std::string vertTooltip = I18n::T("editor.panel.map_tools.vertex_tooltip");
    if (toolButton(vertLabel.c_str(),
                    vertTooltip.c_str(),
                    currentSubMode == EditorSubMode::Vertex && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Vertex);
    }
    const std::string edgeLabel = std::string(ICON_FA_MINUS " ") +
        I18n::T("editor.panel.map_tools.edge");
    const std::string edgeTooltip = I18n::T("editor.panel.map_tools.edge_tooltip");
    if (toolButton(edgeLabel.c_str(),
                    edgeTooltip.c_str(),
                    currentSubMode == EditorSubMode::Edge && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Edge);
    }
    const std::string faceLabel = std::string(ICON_FA_VECTOR_SQUARE " ") +
        I18n::T("editor.panel.map_tools.face");
    const std::string faceTooltip = I18n::T("editor.panel.map_tools.face_tooltip");
    if (toolButton(faceLabel.c_str(),
                    faceTooltip.c_str(),
                    currentSubMode == EditorSubMode::Face && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Face);
    }

    ImGui::Separator();

    // F2H31 Bloque C: toggle snap-to-vertex (tecla V tambien dispara).
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.map_tools.snap").c_str());
    const std::string snapLabel = std::string(ICON_FA_MAGNET " ") +
        I18n::T("editor.panel.map_tools.snap_v");
    const std::string snapTooltip = I18n::T("editor.panel.map_tools.snap_v_tooltip");
    if (toolButton(snapLabel.c_str(),
                    snapTooltip.c_str(),
                    m_ui->snapToVertexEnabled())) {
        m_ui->requestToggleSnapToVertex();
    }

    ImGui::Separator();

    // F2H35 Bloque E: toggle labels arriba de point entities en
    // perspective + ortos. Default ON. El boton highlight refleja el
    // state actual.
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.map_tools.visualization").c_str());
    const std::string namesLabel = std::string(ICON_FA_TAG " ") +
        I18n::T("editor.panel.map_tools.names");
    const std::string namesTooltip = I18n::T("editor.panel.map_tools.names_tooltip");
    if (toolButton(namesLabel.c_str(),
                    namesTooltip.c_str(),
                    m_ui->showEntityLabels())) {
        m_ui->requestToggleEntityLabels();
    }

    ImGui::Separator();

    // F2H32 Bloque C: carve UI button. Click destructivo — resta el
    // brush activo por todos los brushes que intersectan su AABB.
    // Sin keyboard shortcut para evitar accidentes.
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.map_tools.actions").c_str());
    const std::string carveLabel = std::string(ICON_FA_CIRCLE_MINUS " ") +
        I18n::T("editor.panel.map_tools.carve");
    const std::string carveTooltip = I18n::T("editor.panel.map_tools.carve_tooltip");
    if (toolButton(carveLabel.c_str(),
                    carveTooltip.c_str(),
                    false)) {
        m_ui->requestCarve();
    }

    ImGui::End();
}

} // namespace Mood
