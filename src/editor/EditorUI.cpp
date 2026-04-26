#include "editor/EditorUI.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace Mood {

void EditorUI::eraseRecent(const std::filesystem::path& path) {
    const auto canonical = std::filesystem::absolute(path);
    auto it = std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
        [&canonical](const std::filesystem::path& p) {
            return std::filesystem::absolute(p) == canonical;
        });
    if (it != m_recentProjects.end()) {
        m_recentProjects.erase(it, m_recentProjects.end());
        m_recentsDirty = true;
    }
}

void EditorUI::pruneMissingRecents() {
    const auto before = m_recentProjects.size();
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [](const std::filesystem::path& p) {
                std::error_code ec;
                return !std::filesystem::exists(p, ec);
            }),
        m_recentProjects.end());
    if (m_recentProjects.size() != before) {
        m_recentsDirty = true;
    }
}

EditorUI::EditorUI() {
    m_panels = {&m_viewport, &m_hierarchy, &m_inspector, &m_assetBrowser, &m_console};
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
            ImGui::SameLine();
            // Boton al tope que filtra los recientes que ya no existen en
            // disco. Util cuando se mueven o borran proyectos por afuera.
            const float availX = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availX - 160.0f);
            if (ImGui::SmallButton("Limpiar inexistentes")) {
                pruneMissingRecents();
            }

            // Lista scrollable por si crece. Cada fila: label clickeable +
            // boton "X" al final para borrar manualmente esa entrada.
            ImGui::BeginChild("##recents", ImVec2(0.0f, 180.0f), true);
            std::filesystem::path toErase; // diferido hasta despues del loop
            for (const auto& path : m_recentProjects) {
                ImGui::PushID(path.generic_string().c_str());

                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                const std::string label = path.stem().generic_string() +
                                          "  —  " + path.generic_string() +
                                          (exists ? "" : "  (no existe)");

                // Boton X al final de la fila.
                const float rowAvail = ImGui::GetContentRegionAvail().x;
                const float xButtonW = 24.0f;

                // Selectable que toma todo menos el ancho del boton X.
                if (!exists) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        ImVec4(0.7f, 0.4f, 0.4f, 1.0f));
                }
                if (ImGui::Selectable(label.c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick,
                                       ImVec2(rowAvail - xButtonW - 4.0f, 0))) {
                    if (exists) {
                        m_openProjectPath = path;
                        ImGui::CloseCurrentPopup();
                    } else {
                        // Click sobre uno inexistente: marcarlo para sacar.
                        toErase = path;
                    }
                }
                if (!exists) ImGui::PopStyleColor();

                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    toErase = path;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Quitar de la lista de recientes");
                }
                ImGui::PopID();
            }
            ImGui::EndChild();

            if (!toErase.empty()) {
                eraseRecent(toErase);
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextDisabled(
            "El editor va a permanecer bloqueado hasta que elijas un proyecto.");

        ImGui::EndPopup();
    }
}

} // namespace Mood
