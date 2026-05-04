#include "editor/ui/Dockspace.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Mood {

namespace {

/// @brief Layout default del workspace "Layout" — el de trabajo general.
///        Hierarchy izq, Inspector der, Asset Browser + Console abajo,
///        Viewport central.
void buildLayoutWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);
    ImGuiID dockBottomRight = ImGui::DockBuilderSplitNode(
        dockBottom, ImGuiDir_Right, 0.45f, nullptr, &dockBottom);

    ImGui::DockBuilderDockWindow("Hierarchy",     dockLeft);
    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
    ImGui::DockBuilderDockWindow("Inspector",     dockRight);
    ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Console",       dockBottomRight);
    // Otros panels van al dock central (tabs sobre el viewport) si el
    // user los abre desde "Ver".
    ImGui::DockBuilderDockWindow("Lua API",       dockRight);
    ImGui::DockBuilderDockWindow("Performance",   dockRight);
    ImGui::DockBuilderDockWindow("Script Editor", dockMain);
    ImGui::DockBuilderDockWindow("Material Editor", dockRight);
}

/// @brief Workspace "Scripting" — Script Editor central grande, Console
///        abajo grande, viewport pequeño esquina, hierarchy chico.
void buildScriptingWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.35f, nullptr, &dockMain);
    ImGuiID dockLeftBottom = ImGui::DockBuilderSplitNode(
        dockLeft, ImGuiDir_Down, 0.50f, nullptr, &dockLeft);

    ImGui::DockBuilderDockWindow("Script Editor", dockMain);
    ImGui::DockBuilderDockWindow("Console",       dockBottom);
    ImGui::DockBuilderDockWindow("Lua API",       dockBottom);   // tab al lado
    ImGui::DockBuilderDockWindow("Hierarchy",     dockLeft);
    ImGui::DockBuilderDockWindow("Inspector",     dockLeftBottom);
    ImGui::DockBuilderDockWindow("Viewport",      dockLeftBottom); // tab
    ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Performance",   dockBottom);
    ImGui::DockBuilderDockWindow("Material Editor", dockMain);
}

/// @brief Workspace "Profile" — Performance HUD destacado, Console grande
///        para logs, viewport central para validar visual.
void buildProfileWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.35f, nullptr, &dockMain);

    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
    ImGui::DockBuilderDockWindow("Performance",   dockRight);
    ImGui::DockBuilderDockWindow("Hierarchy",     dockRight);    // tab
    ImGui::DockBuilderDockWindow("Inspector",     dockRight);    // tab
    ImGui::DockBuilderDockWindow("Console",       dockBottom);
    ImGui::DockBuilderDockWindow("Lua API",       dockBottom);   // tab
    ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);   // tab
    ImGui::DockBuilderDockWindow("Script Editor", dockMain);
    ImGui::DockBuilderDockWindow("Material Editor", dockRight);
}

/// @brief Workspace "Materials" — Material Editor destacado izq, Asset
///        Browser para drag & drop de texturas, Viewport para preview.
void buildMaterialsWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.30f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.30f, nullptr, &dockMain);

    ImGui::DockBuilderDockWindow("Material Editor", dockLeft);
    ImGui::DockBuilderDockWindow("Asset Browser",   dockBottom);
    ImGui::DockBuilderDockWindow("Viewport",        dockMain);
    ImGui::DockBuilderDockWindow("Inspector",       dockRight);
    ImGui::DockBuilderDockWindow("Hierarchy",       dockLeft);    // tab
    ImGui::DockBuilderDockWindow("Console",         dockBottom);  // tab
    ImGui::DockBuilderDockWindow("Lua API",         dockBottom);  // tab
    ImGui::DockBuilderDockWindow("Performance",     dockRight);   // tab
    ImGui::DockBuilderDockWindow("Script Editor",   dockMain);    // tab
}

/// @brief Dispatcher: elige el builder segun el nombre del workspace
///        activo. Default = Layout para nombres desconocidos.
void buildLayoutForWorkspace(const std::string& name, ImGuiID dockspaceId) {
    if      (name == "Scripting") buildScriptingWorkspace(dockspaceId);
    else if (name == "Profile")   buildProfileWorkspace(dockspaceId);
    else if (name == "Materials") buildMaterialsWorkspace(dockspaceId);
    else                          buildLayoutWorkspace(dockspaceId);
}

void rebuildLayout(ImGuiID dockspaceId, const ImVec2& size,
                    const std::string& workspaceName, bool force) {
    if (!force) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
        if (node != nullptr && node->IsSplitNode()) {
            // Ya hay layout valido (ej. recargado del ini del workspace
            // activo): no tocar.
            return;
        }
    }

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    buildLayoutForWorkspace(workspaceName, dockspaceId);

    ImGui::DockBuilderFinish(dockspaceId);
}

} // namespace

bool Dockspace::begin() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking
                           | ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoBringToFrontOnFocus
                           | ImGuiWindowFlags_NoNavFocus
                           | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    const bool open = ImGui::Begin("##MoodDockHost", nullptr, flags);
    ImGui::PopStyleVar(3);

    if (open) {
        const ImGuiID dockspaceId = ImGui::GetID("MoodDockSpace");

        // Aplicar el layout: la primera vez del session (si el dockspace
        // esta vacio), si se solicito un reset explicito, o si se pidio
        // un rebuild por cambio de workspace.
        const bool force = m_resetRequested || m_rebuildRequested;
        if (!m_buildAttempted || force) {
            rebuildLayout(dockspaceId, viewport->WorkSize,
                           m_activeWorkspaceName, force);
            m_buildAttempted = true;
            m_resetRequested = false;
            m_rebuildRequested = false;
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    return open;
}

void Dockspace::end() {
    ImGui::End();
}

} // namespace Mood
