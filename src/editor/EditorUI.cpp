#include "editor/EditorUI.h"

#include <imgui.h>

namespace Mood {

EditorUI::EditorUI() {
    m_panels = {&m_viewport, &m_inspector, &m_assetBrowser, &m_console};
}

void EditorUI::draw(bool& requestQuit) {
    // Status bar primero: usa BeginViewportSideBar que reserva una franja
    // inferior del viewport. Al dibujarse ANTES del dockspace, este toma
    // solo el area disponible restante y los paneles docked no se solapan
    // con la status bar.
    m_statusBar.draw(m_mode);

    if (m_dockspace.begin()) {
        m_menuBar.draw(*this, requestQuit);
    }
    m_dockspace.end();

    for (IPanel* panel : m_panels) {
        if (panel->visible) {
            panel->onImGuiRender();
        }
    }

    // Modal Welcome: si no hay proyecto activo, bloquea toda interaccion
    // hasta que el usuario elija crear / abrir / elegir reciente.
    // Convencion Unity/Godot: no se entra al editor sin proyecto.
    if (!m_hasProject) {
        drawWelcomeModal();
    }
}

void EditorUI::drawWelcomeModal() {
    // Trigger unico por frame: si el popup no esta abierto, abrirlo.
    // ImGui reabre sin "parpadeo" si ya esta open.
    if (!ImGui::IsPopupOpen("MoodEngine — bienvenida")) {
        ImGui::OpenPopup("MoodEngine — bienvenida");
    }

    // Centrar el modal en la ventana principal.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->GetCenter().x, viewport->GetCenter().y),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::BeginPopupModal("MoodEngine — bienvenida", nullptr, flags)) {
        ImGui::TextUnformatted("No hay proyecto activo.");
        ImGui::TextUnformatted("Elegi que hacer para empezar:");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Accion principal: crear uno nuevo.
        if (ImGui::Button("Nuevo proyecto", ImVec2(240.0f, 32.0f))) {
            requestProjectAction(ProjectAction::NewProject);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Abrir proyecto...", ImVec2(240.0f, 32.0f))) {
            requestProjectAction(ProjectAction::OpenProject);
            ImGui::CloseCurrentPopup();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        if (m_recentProjects.empty()) {
            ImGui::TextDisabled("Sin proyectos recientes todavia.");
        } else {
            ImGui::TextUnformatted("Recientes:");
            // Lista scrollable por si crece.
            ImGui::BeginChild("##recents", ImVec2(0.0f, 180.0f), true);
            for (const auto& path : m_recentProjects) {
                const std::string label = path.stem().generic_string() +
                                          "  —  " + path.generic_string();
                if (ImGui::Selectable(label.c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_openProjectPath = path;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextDisabled(
            "El editor va a permanecer bloqueado hasta que elijas un proyecto.");

        ImGui::EndPopup();
    }
}

} // namespace Mood
