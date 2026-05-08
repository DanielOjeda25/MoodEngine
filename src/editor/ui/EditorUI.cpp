#include "editor/ui/EditorUI.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"  // F2H12
#include "engine/scene/components/Components.h"      // F2H12: TagComponent
#include "engine/scene/core/Scene.h"                  // F2H12

#include <imgui.h>
#include <imgui_internal.h>  // BeginViewportSideBar

#include <algorithm>
#include <filesystem>
#include <vector>

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
    m_panels = {&m_viewport, &m_hierarchy, &m_inspector, &m_assetBrowser,
                &m_console, &m_luaApi, &m_performanceHud,
                &m_scriptEditor, &m_materialEditor, &m_toolbar};
    m_toolbar.setEditorUi(this);  // F2H22: la toolbar emite requests a UI

    // F2H7: el dockspace arranca apuntando al workspace default
    // (F2H22: Modelar, era Layout).
    const std::string& initialWs = m_workspaceManager.activeWorkspace().name;
    m_dockspace.setActiveWorkspaceName(initialWs);
    // F2H22: aplicar visibility default del workspace inicial. Antes
    // los panels arrancaban con sus visible default del header, lo
    // cual mostraba Console/LuaApi en Modelar — ruido visual no
    // deseado. Esto sigue respetando el iniLayout custom del dev
    // cuando el .moodproj se cargue (setWorkspaces + applyPendingSwitch
    // toman over despues).
    applyDefaultVisibilityForWorkspace(initialWs);
}

void EditorUI::applyPendingWorkspaceSwitch() {
    if (m_pendingWorkspaceSwitch < 0) return;
    const int target = m_pendingWorkspaceSwitch;
    m_pendingWorkspaceSwitch = -1;

    if (target < 0 || target >= static_cast<int>(m_workspaceManager.count())) {
        return; // defensivo
    }
    if (target == m_workspaceManager.activeIndex()) {
        return; // ya esta activo
    }

    // 1) Capturar el layout actual al workspace que estaba activo (auto-save).
    {
        size_t size = 0;
        const char* iniData = ImGui::SaveIniSettingsToMemory(&size);
        if (iniData != nullptr && size > 0) {
            m_workspaceManager.captureCurrentLayout(std::string(iniData, size));
        }
    }

    // 2) Cambiar de workspace.
    m_workspaceManager.setActiveByIndex(target);
    const auto& nextWs = m_workspaceManager.activeWorkspace();
    m_dockspace.setActiveWorkspaceName(nextWs.name);

    // 3) Aplicar el layout del nuevo workspace.
    if (!nextWs.iniLayout.empty()) {
        // ImGui restaura la posicion + tamano + dock state desde el ini.
        ImGui::LoadIniSettingsFromMemory(nextWs.iniLayout.c_str(),
                                          nextWs.iniLayout.size());
    } else {
        // F2H22: workspace nunca activado en este proyecto. Dos cosas:
        // - aplicar visibility default (qué panels se muestran).
        // - pedir rebuild del Dockspace para que dockee los panels
        //   visibles segun el builder del workspace.
        applyDefaultVisibilityForWorkspace(nextWs.name);
        m_dockspace.requestRebuildForCurrentWorkspace();
    }

    Log::editor()->info("[workspace] switched to '{}'", nextWs.name);
}

void EditorUI::applyDefaultVisibilityForWorkspace(const std::string& name) {
    // F2H22: cada workspace muestra solo los panels relevantes a su
    // tarea. El resto sigue accesible desde menu Ver pero no se rendea
    // por default — reduce ruido visual al primer activado del workspace.
    //
    // Las visibilidades de aqui son el state INICIAL — si el dev abre
    // un panel "extra" en, por ejemplo, Modelar, esa decision persiste
    // en su iniLayout (que se captura al cambiar de workspace) y la
    // proxima vez que vuelva a Modelar, ese panel sigue abierto.
    //
    // Solo se llama cuando iniLayout esta vacio (primer activado o reset).

    // Los nombres de panel matchean el `IPanel::name()`.
    auto setVisible = [&](const char* panelName, bool v) {
        for (IPanel* p : m_panels) {
            if (std::string_view(p->name()) == panelName) {
                p->visible = v;
                return;
            }
        }
    };

    // F2H23 polish: panel Hierarchy renombrado a "Escena". Los nombres
    // de panel matchean el `IPanel::name()` actualizado.
    if (name == "Programar") {
        setVisible("Viewport",        true);
        setVisible("Escena",          true);  // F2H23: era Hierarchy
        setVisible("Inspector",       true);
        setVisible("Asset Browser",   false);
        setVisible("Console",         true);
        setVisible("Lua API",         true);
        setVisible("Performance",     false);
        setVisible("Script Editor",   true);
        setVisible("Material Editor", false);
        setVisible("Tools",           false);
    } else if (name == "Materiales") {
        setVisible("Viewport",        true);
        setVisible("Escena",          false);
        setVisible("Inspector",       true);
        setVisible("Asset Browser",   true);
        setVisible("Console",         false);
        setVisible("Lua API",         false);
        setVisible("Performance",     false);
        setVisible("Script Editor",   false);
        setVisible("Material Editor", true);
        setVisible("Tools",           false);
    } else {
        // "Layout" (default) — flow general de mapping. Tools visible.
        // Console explicitamente FALSE (pedido del dev: "no quiero
        // Console abajo en Layout"). El dev puede abrirla desde Ver
        // y persiste en su iniLayout custom.
        setVisible("Viewport",        true);
        setVisible("Escena",          true);
        setVisible("Inspector",       true);
        setVisible("Asset Browser",   true);
        setVisible("Console",         false);
        setVisible("Lua API",         false);
        setVisible("Performance",     false);
        setVisible("Script Editor",   false);
        setVisible("Material Editor", false);
        setVisible("Tools",           true);
    }
}

void EditorUI::draw(bool& requestQuit) {
    // F2H7: los workspace tabs se dibujan ahora DENTRO del menu bar
    // principal (estilo Blender, todo en una sola fila: Archivo / Editar /
    // Ver / Ayuda | Layout / Scripting / Profile / Materials | ... Play).
    // La logica vive en MenuBar.cpp.

    // F2H16: sincronizar el "Ultimo: <command>" del statusbar con el
    // tope del HistoryStack. Cada frame el dev ve que va a deshacer
    // Ctrl+Z (Blender-style "Last Operator").
    if (m_history != nullptr) {
        m_statusBar.setLastCommand(m_history->undoName());
    }

    // Status bar inferior: BeginViewportSideBar con ImGuiDir_Down.
    m_statusBar.draw(m_mode, m_subMode);

    if (m_dockspace.begin()) {
        m_menuBar.draw(*this, requestQuit);
    }
    m_dockspace.end();

    // Hito 28 F: el ScriptEditorPanel necesita saber la entidad seleccionada
    // cada frame para detectar cambios y reload del .lua. La inyectamos aca
    // antes del render del panel (mismo flujo que InspectorPanel pero por
    // setter en lugar de pointer-back al EditorUI).
    m_scriptEditor.setSelectedEntity(m_selectionSet.active);

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

// ----------------------------------------------------------------------------
// F2H12: drawBooleanOpMenu
// ----------------------------------------------------------------------------

void EditorUI::drawBooleanOpMenu() {
    // F2H13: la op opera sobre el SelectionSet completo. Habilitado
    // solo si hay >= 2 entidades seleccionadas y todas tienen
    // BrushComponent. La `active` es el "tool brush" B; las demas
    // son las A's a operar contra B.
    int brushCount = 0;
    for (const Entity& e : m_selectionSet.selected) {
        if (e && e.hasComponent<BrushComponent>()) ++brushCount;
    }
    const bool ready = (brushCount >= 2)
        && (m_selectionSet.selected.size() == static_cast<usize>(brushCount));

    if (!ImGui::BeginMenu("Boolean", ready)) {
        return;
    }

    // Header informativo: cuantos brushes y cual es la "tool".
    const std::string activeTag = (m_selectionSet.active &&
                                    m_selectionSet.active.hasComponent<TagComponent>())
        ? m_selectionSet.active.getComponent<TagComponent>().name
        : std::string{"?"};
    ImGui::TextDisabled("%d brushes seleccionados", brushCount);
    ImGui::TextDisabled("Tool brush (B): %s", activeTag.c_str());
    ImGui::Separator();

    if (ImGui::MenuItem("Subtract (A - B) por cada A")) {
        requestBooleanOp(BooleanOpRequestKind::Subtract);
    }
    if (ImGui::MenuItem("Union (A U B) en cascada")) {
        requestBooleanOp(BooleanOpRequestKind::Union);
    }
    if (ImGui::MenuItem("Intersect (A & B) en cascada")) {
        requestBooleanOp(BooleanOpRequestKind::Intersect);
    }

    ImGui::EndMenu();
}

} // namespace Mood
