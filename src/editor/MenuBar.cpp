#include "editor/MenuBar.h"

#include "core/Log.h"
#include "editor/EditorUI.h"
#include "editor/panels/IPanel.h"

#include <imgui.h>

namespace Mood {

void MenuBar::draw(EditorUI& ui, bool& requestQuit) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Archivo")) {
            if (ImGui::MenuItem("Nuevo Proyecto")) {
                ui.requestProjectAction(ProjectAction::NewProject);
            }
            if (ImGui::MenuItem("Abrir Proyecto")) {
                ui.requestProjectAction(ProjectAction::OpenProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Guardar", "Ctrl+S", false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::Save);
            }
            if (ImGui::MenuItem("Guardar como...", nullptr, false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::SaveAs);
            }
            if (ImGui::MenuItem("Cerrar proyecto", nullptr, false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::CloseProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Salir", "Alt+F4")) {
                requestQuit = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Editar")) {
            if (ImGui::MenuItem("Deshacer", "Ctrl+Z", false, false)) {}
            if (ImGui::MenuItem("Rehacer", "Ctrl+Y", false, false)) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ver")) {
            for (IPanel* panel : ui.panels()) {
                ImGui::MenuItem(panel->name(), nullptr, &panel->visible);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Restablecer layout")) {
                ui.dockspace().requestResetToDefault();
                Log::editor()->info("Layout restablecido al default");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ayuda")) {
            if (ImGui::MenuItem("Acerca de")) {
                m_showAboutPopup = true;
            }
            ImGui::EndMenu();
        }

        // Boton Play/Stop empujado a la derecha de la menu bar.
        const bool isPlay = ui.mode() == EditorMode::Play;
        const char* btnLabel = isPlay ? "Stop" : "Play";
        const float btnWidth = 64.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        if (avail > btnWidth) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnWidth));
        }
        if (isPlay) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.85f, 0.35f, 1.0f));
        }
        if (ImGui::Button(btnLabel, ImVec2(btnWidth, 0.0f))) {
            ui.requestTogglePlay();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndMenuBar();
    }

    // --- Popups ---
    if (m_showAboutPopup) {
        ImGui::OpenPopup("Acerca de MoodEngine");
        m_showAboutPopup = false;
    }
    if (m_showNotImplementedPopup) {
        ImGui::OpenPopup("No implementado");
        m_showNotImplementedPopup = false;
    }

    if (ImGui::BeginPopupModal("Acerca de MoodEngine", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("MoodEngine");
        ImGui::Text("Version 0.3.0 (Hito 3)");
        ImGui::Separator();
        ImGui::Text("Motor grafico 3D propio con editor integrado.");
        ImGui::Text("Repositorio: https://github.com/DanielOjeda25/MoodEngine");
        ImGui::Separator();
        if (ImGui::Button("Cerrar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("No implementado", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Esta accion se implementara en un hito futuro.");
        if (ImGui::Button("Ok", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Mood
