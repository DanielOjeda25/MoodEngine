// F3H28: categoria scene-wide "Map Tools" del Inspector. Concentra
// configuración global del workspace Editor de Mapas. Reemplaza al
// extinto MapEditorTopBar (F2H30/F2H31) — su contenido (tools, sub-mode,
// snap, names, carve) + el popover de configuración fina del snap
// (F3H6) ahora viven acá. Layout vertical adaptado al sidebar del
// Inspector. Atajos de teclado siguen funcionando como antes
// (V para snap, 1/2/3 para sub-mode, Ctrl+G para grupos, etc).

#include "editor/panels/scene/InspectorPanel.h"

#include "core/i18n/I18n.h"
#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "editor/ui/SnapPopoverContent.h"

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Helper local — botón "tool" estilo Toolbar pero adaptado al sidebar
// del Inspector (ancho completo del child, alto fijo). Background
// resaltado cuando el modo está activo.
bool mapToolsToggleButton(const char* label, const char* tooltip,
                           bool active) {
    const ImU32 kActiveBg = IM_COL32(60, 140, 200, 255);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        kActiveBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kActiveBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  kActiveBg);
    }
    const float w = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::Button(label, ImVec2(w, 0.0f));
    if (active) ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

} // namespace

void InspectorPanel::renderMapToolsSection() {
    if (m_ui == nullptr) return;

    // Header descriptivo.
    ImGui::TextDisabled("%s",
        I18n::T("editor.inspector.maptools.header").c_str());
    ImGui::Spacing();

    // === Sección 1: Sub-mode (Object/Vertex/Edge/Face) ===
    ImGui::SeparatorText(I18n::T(
        "editor.inspector.maptools.section_submode").c_str());

    const EditorSubMode currentSubMode = m_ui->subMode();
    const bool polyActive = m_ui->polygonDrawActive();

    const std::string objLbl = std::string(ICON_FA_OBJECT_GROUP " ")
        + I18n::T("editor.panel.map_tools.object");
    if (mapToolsToggleButton(objLbl.c_str(),
            I18n::T("editor.panel.map_tools.object_tooltip").c_str(),
            currentSubMode == EditorSubMode::Object && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Object);
    }
    const std::string vertLbl = std::string(ICON_FA_CIRCLE_DOT " ")
        + I18n::T("editor.panel.map_tools.vertex");
    if (mapToolsToggleButton(vertLbl.c_str(),
            I18n::T("editor.panel.map_tools.vertex_tooltip").c_str(),
            currentSubMode == EditorSubMode::Vertex && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Vertex);
    }
    const std::string edgeLbl = std::string(ICON_FA_MINUS " ")
        + I18n::T("editor.panel.map_tools.edge");
    if (mapToolsToggleButton(edgeLbl.c_str(),
            I18n::T("editor.panel.map_tools.edge_tooltip").c_str(),
            currentSubMode == EditorSubMode::Edge && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Edge);
    }
    const std::string faceLbl = std::string(ICON_FA_VECTOR_SQUARE " ")
        + I18n::T("editor.panel.map_tools.face");
    if (mapToolsToggleButton(faceLbl.c_str(),
            I18n::T("editor.panel.map_tools.face_tooltip").c_str(),
            currentSubMode == EditorSubMode::Face && !polyActive)) {
        m_ui->requestSubMode(EditorSubMode::Face);
    }

    // === Sección 2: Map Tool (Select/CreateBlock/Pincel) ===
    ImGui::SeparatorText(I18n::T(
        "editor.inspector.maptools.section_tool").c_str());

    const MapTool currentTool = m_ui->mapTool();
    const std::string selLbl = std::string(ICON_FA_ARROW_POINTER " ")
        + I18n::T("editor.inspector.maptools.tool_select");
    if (mapToolsToggleButton(selLbl.c_str(),
            I18n::T("editor.inspector.maptools.tool_select_tooltip").c_str(),
            currentTool == MapTool::Select)) {
        m_ui->requestMapTool(MapTool::Select);
    }
    const std::string blockLbl = std::string(ICON_FA_CUBE " ")
        + I18n::T("editor.inspector.maptools.tool_block");
    if (mapToolsToggleButton(blockLbl.c_str(),
            I18n::T("editor.inspector.maptools.tool_block_tooltip").c_str(),
            currentTool == MapTool::CreateBlock)) {
        m_ui->requestMapTool(MapTool::CreateBlock);
    }
    const std::string pincelLbl = std::string(ICON_FA_PAINTBRUSH " ")
        + I18n::T("editor.inspector.maptools.tool_pincel");
    if (mapToolsToggleButton(pincelLbl.c_str(),
            I18n::T("editor.inspector.maptools.tool_pincel_tooltip").c_str(),
            polyActive)) {
        m_ui->requestTogglePolygonDraw();
    }
    // F3H28: clip tool (F2H32 Bloque B). Migrado del extinto MapEditorTopBar.
    const std::string clipLbl = std::string(ICON_FA_SCISSORS " ")
        + I18n::T("editor.inspector.maptools.tool_clip");
    if (mapToolsToggleButton(clipLbl.c_str(),
            I18n::T("editor.inspector.maptools.tool_clip_tooltip").c_str(),
            currentTool == MapTool::Clip && !polyActive)) {
        m_ui->requestMapTool(MapTool::Clip);
    }

    // === Sección 3: Snap a vértice ===
    ImGui::SeparatorText(I18n::T(
        "editor.inspector.maptools.section_snap").c_str());

    const std::string snapLbl = std::string(ICON_FA_MAGNET " ")
        + I18n::T("editor.inspector.maptools.snap_v");
    if (mapToolsToggleButton(snapLbl.c_str(),
            I18n::T("editor.inspector.maptools.snap_v_tooltip").c_str(),
            m_ui->snapToVertexEnabled())) {
        m_ui->requestToggleSnapToVertex();
    }
    // F3H28: botón "Ajustes" que abre popover de configuración fina del
    // snap (steps, umbrales). Migrado del extinto MapEditorTopBar.
    const std::string snapCfgLbl = std::string(ICON_FA_ROTATE " ")
        + I18n::T("editor.map_tools.snap.settings_button");
    if (mapToolsToggleButton(snapCfgLbl.c_str(),
            I18n::T("editor.map_tools.snap.settings_tooltip").c_str(),
            false)) {
        ImGui::OpenPopup("##snap_settings_popup");
    }
    if (ImGui::BeginPopup("##snap_settings_popup")) {
        // F3H29: header con título + botón cerrar a la derecha. El dev
        // reportó que el modal sólo cerraba clickeando afuera — falta el
        // botón explícito. Pattern estilo Hammer/Blender popovers.
        ImGui::TextUnformatted(I18n::T("editor.map_tools.snap.popup_title").c_str());
        const float closeBtnW = ImGui::CalcTextSize("X").x
                                 + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - closeBtnW);
        if (ImGui::SmallButton("X##snap_popup_close")) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.modal.common.close").c_str());
        }
        ImGui::Separator();
        drawSnapPopoverContent(m_ui, m_ui->currentProject());
        ImGui::EndPopup();
    }

    // === Sección 4: Visualización (labels) ===
    ImGui::SeparatorText(I18n::T(
        "editor.inspector.maptools.section_viz").c_str());

    const std::string namesLbl = std::string(ICON_FA_TAG " ")
        + I18n::T("editor.inspector.maptools.names");
    if (mapToolsToggleButton(namesLbl.c_str(),
            I18n::T("editor.inspector.maptools.names_tooltip").c_str(),
            m_ui->showEntityLabels())) {
        m_ui->requestToggleEntityLabels();
    }

    // === Sección 5: Acciones ===
    ImGui::SeparatorText(I18n::T(
        "editor.inspector.maptools.section_actions").c_str());

    const std::string carveLbl = std::string(ICON_FA_CIRCLE_MINUS " ")
        + I18n::T("editor.inspector.maptools.action_carve");
    if (mapToolsToggleButton(carveLbl.c_str(),
            I18n::T("editor.inspector.maptools.action_carve_tooltip").c_str(),
            false)) {
        m_ui->requestCarve();
    }
}

} // namespace Mood
