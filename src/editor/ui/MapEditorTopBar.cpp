#include "editor/ui/MapEditorTopBar.h"

#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/project/ProjectSettings.h"                  // F3H6 polish: SnapSettings
#include "engine/scene/serialization/ProjectSerializer.h"    // F3H6 polish: Project struct
#include "core/i18n/I18n.h"  // F2H43

#include <imgui.h>

#include <string>
#include <vector>

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

// F3H6 polish: popover con la config completa del snap. Storage en
// .moodproj > settings.snap (la storage estaba bien per-project, solo
// la UI se mueve a un home conceptual correcto: el editor de mapas).
// Mejoras de UI vs la tab vieja de Project Settings:
//   - Section headers (Pasos / Umbrales) con SeparatorText.
//   - InputInts compactos (80 px) + boton X pegado al lado.
//   - Combo "Paso actual" arriba (acceso rapido sin atajo de teclado).
//   - Reset buttons (↺) per-field visibles solo cuando difieren.
void drawSnapPopoverContent(EditorUI* ui, Project* project) {
    if (project == nullptr) {
        ImGui::TextDisabled("(no project)");
        return;
    }
    SnapSettings& s = project->settings.snap;
    const SnapSettings defaults;

    auto markDirty = [&]() {
        if (ui != nullptr) ui->requestProjectDirty();
    };

    // === Paso actual (acceso rapido) ===
    ImGui::TextUnformatted(I18n::T("editor.map_tools.snap.current_step").c_str());
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(120.0f);

    std::vector<std::string> labels;
    labels.reserve(s.stepsAvailable.size());
    for (int v : s.stepsAvailable) labels.push_back(std::to_string(v));
    std::vector<const char*> labelPtrs;
    labelPtrs.reserve(labels.size());
    for (const auto& l : labels) labelPtrs.push_back(l.c_str());

    // current step ya vive en EditorApplication::m_hammerSnapStep — no
    // lo accedemos directo desde aca; lo modificamos via EditorUI futura.
    // Para evitar acoplar este popover al hammerSnapStep, mostramos el
    // combo con el defaultStepIndex (initial) — el usuario cambia el
    // step "vivo" con Ctrl+= / Shift+wheel sobre el viewport.
    // TODO(F3H7+): exponer EditorUI::currentHammerSnapStep() + setter
    // para que este combo controle el step activo en vivo.
    int comboIdx = s.defaultStepIndex;
    if (comboIdx < 0 || comboIdx >= static_cast<int>(labelPtrs.size())) {
        comboIdx = 0;
    }
    if (!labelPtrs.empty()
        && ImGui::Combo("##current_step_combo", &comboIdx,
                         labelPtrs.data(), static_cast<int>(labelPtrs.size()))) {
        s.defaultStepIndex = comboIdx;
        markDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.current_step_hint").c_str());
    }

    ImGui::Spacing();

    // === Sección: Pasos disponibles ===
    ImGui::SeparatorText(I18n::T("editor.map_tools.snap.section_steps").c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.steps_hint").c_str());
    }

    int indexToDelete = -1;
    for (size_t i = 0; i < s.stepsAvailable.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(80.0f);
        int value = s.stepsAvailable[i];
        if (ImGui::InputInt("##step_value", &value, 0, 0,
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (value > 0) {
                s.stepsAvailable[i] = value;
                markDirty();
            }
        }
        ImGui::SameLine();
        // X compacto al lado del input.
        if (ImGui::SmallButton("X")) {
            if (s.stepsAvailable.size() > 1) {
                indexToDelete = static_cast<int>(i);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.map_tools.snap.remove_step").c_str());
        }
        ImGui::PopID();
    }

    if (indexToDelete >= 0) {
        s.stepsAvailable.erase(s.stepsAvailable.begin() + indexToDelete);
        if (s.defaultStepIndex >= static_cast<int>(s.stepsAvailable.size())) {
            s.defaultStepIndex = static_cast<int>(s.stepsAvailable.size()) - 1;
        }
        markDirty();
    }

    // + para agregar (alineado al final de la lista).
    if (ImGui::SmallButton(" + ")) {
        const int newStep = s.stepsAvailable.empty()
            ? 1 : s.stepsAvailable.back() * 2;
        s.stepsAvailable.push_back(newStep);
        markDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.map_tools.snap.add_step").c_str());
    }
    // Reset del array completo si difiere.
    if (s.stepsAvailable != defaults.stepsAvailable) {
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string(ICON_FA_ROTATE_LEFT)
                                + " ##reset_steps").c_str())) {
            s.stepsAvailable = defaults.stepsAvailable;
            if (s.defaultStepIndex >= static_cast<int>(s.stepsAvailable.size())) {
                s.defaultStepIndex = defaults.defaultStepIndex;
            }
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.common.reset_default").c_str());
        }
    }

    ImGui::Spacing();

    // === Sección: Umbrales ===
    ImGui::SeparatorText(I18n::T("editor.map_tools.snap.section_thresholds").c_str());

    auto thresholdSlider = [&](const char* keyLabel,
                                const char* keyHint,
                                const char* widgetId,
                                const char* resetSuffix,
                                f32& value,
                                f32 defaultValue,
                                f32 minVal,
                                f32 maxVal,
                                const char* fmt) {
        ImGui::TextUnformatted(I18n::T(keyLabel).c_str());
        ImGui::SameLine(140.0f);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat(widgetId, &value, minVal, maxVal, fmt)) {
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(keyHint).c_str());
        }
        if (value != defaultValue) {
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string(ICON_FA_ROTATE_LEFT)
                                    + " ##reset_" + resetSuffix).c_str())) {
                value = defaultValue;
                markDirty();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                    I18n::T("editor.common.reset_default").c_str());
            }
        }
    };

    thresholdSlider("editor.map_tools.snap.vertex_threshold",
                     "editor.map_tools.snap.vertex_threshold_hint",
                     "##vertex_threshold", "vertex_threshold",
                     s.snapToVertexThresholdNdc, defaults.snapToVertexThresholdNdc,
                     0.005f, 0.10f, "%.3f");

    thresholdSlider("editor.map_tools.snap.broadphase",
                     "editor.map_tools.snap.broadphase_hint",
                     "##broadphase", "broadphase",
                     s.snapBroadphaseMinWorld, defaults.snapBroadphaseMinWorld,
                     4.0f, 128.0f, "%.1f u");
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

    // F3H6 polish: boton "Ajustes" que abre popover con la config del
    // snap (movido aca desde Project Settings — snap es del editor de
    // mapas, no del proyecto). Storage sigue en .moodproj > settings.snap.
    const std::string snapCfgLabel = std::string(ICON_FA_ROTATE " ") +
        I18n::T("editor.map_tools.snap.settings_button");
    const std::string snapCfgTooltip = I18n::T("editor.map_tools.snap.settings_tooltip");
    if (toolButton(snapCfgLabel.c_str(),
                    snapCfgTooltip.c_str(),
                    false)) {
        ImGui::OpenPopup("##snap_settings_popup");
    }

    // Popover anclado al boton — se mantiene abierto entre frames hasta
    // que el dev clickee fuera (ImGuiPopupFlags_None).
    if (ImGui::BeginPopup("##snap_settings_popup")) {
        ImGui::TextUnformatted(I18n::T("editor.map_tools.snap.popup_title").c_str());
        ImGui::Separator();
        drawSnapPopoverContent(m_ui, m_ui->currentProject());
        ImGui::EndPopup();
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
