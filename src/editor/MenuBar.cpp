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
                Log::editor()->info("Archivo > Nuevo Proyecto (no implementado)");
                m_showNotImplementedPopup = true;
            }
            if (ImGui::MenuItem("Abrir Proyecto")) {
                Log::editor()->info("Archivo > Abrir Proyecto (no implementado)");
                m_showNotImplementedPopup = true;
            }
            if (ImGui::MenuItem("Guardar", "Ctrl+S")) {
                Log::editor()->info("Archivo > Guardar (no implementado)");
                m_showNotImplementedPopup = true;
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ayuda")) {
            if (ImGui::MenuItem("Acerca de")) {
                m_showAboutPopup = true;
            }
            ImGui::EndMenu();
        }

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
        ImGui::Text("Version 0.1.0 (Hito 1)");
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
