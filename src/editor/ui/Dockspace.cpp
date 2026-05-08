#include "editor/ui/Dockspace.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Mood {

namespace {

/// @brief Layout default del workspace "Modelar" (era "Layout") — el de
///        trabajo general. F2H22: agregamos una franja angosta a la
///        IZQUIERDA del Hierarchy para la `Toolbar` (gizmo modes +
///        primitivas brush + face mode). Resto: Hierarchy izq, Inspector
///        der, Asset Browser + Console abajo, Viewport central.
void buildLayoutWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    // F2H22: franja para la Toolbar — ~100px en viewport 1280.
    // Botones de 72x36 (Mover/Rotar/Escala/Box/Cilindro/Cara) caben con
    // padding lateral.
    ImGuiID dockToolbar = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.08f, nullptr, &dockMain);
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);
    ImGuiID dockBottomRight = ImGui::DockBuilderSplitNode(
        dockBottom, ImGuiDir_Right, 0.45f, nullptr, &dockBottom);

    // F2H23: solo dockear los panels relevantes a ESTE workspace.
    // Antes (F2H22) dockeabamos TODOS los panels en cada workspace
    // — el dev veia "Script Editor" como tab al lado del Viewport en
    // Modelar y se confundia. Si el dev abre un panel ajeno desde
    // menu Ver, ImGui lo pone flotante (donde quiera) y el dev decide
    // si acoplarlo o no — el workspace no le impone una posicion.
    ImGui::DockBuilderDockWindow("Tools",         dockToolbar);
    ImGui::DockBuilderDockWindow("Hierarchy",     dockLeft);
    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
    ImGui::DockBuilderDockWindow("Inspector",     dockRight);
    ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);
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

    // F2H23: solo panels relevantes a Programar — sin AssetBrowser /
    // Performance / MaterialEditor (esos pertenecen a Materiales /
    // Optimizar / Materiales respectivamente).
    ImGui::DockBuilderDockWindow("Script Editor", dockMain);
    ImGui::DockBuilderDockWindow("Console",       dockBottom);
    ImGui::DockBuilderDockWindow("Lua API",       dockBottom);   // tab al lado
    ImGui::DockBuilderDockWindow("Hierarchy",     dockLeft);
    ImGui::DockBuilderDockWindow("Inspector",     dockLeftBottom);
    ImGui::DockBuilderDockWindow("Viewport",      dockLeftBottom); // tab
}

/// @brief Workspace "Profile" — Performance HUD destacado, Console grande
///        para logs, viewport central para validar visual.
void buildProfileWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.35f, nullptr, &dockMain);

    // F2H23: solo panels relevantes a Optimizar — Performance HUD +
    // Console son los protagonistas. Hierarchy/Inspector como tab por
    // si el dev necesita inspeccionar una entidad pesada. Resto de
    // panels (AssetBrowser, ScriptEditor, MaterialEditor) NO se dockean.
    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
    ImGui::DockBuilderDockWindow("Performance",   dockRight);
    ImGui::DockBuilderDockWindow("Hierarchy",     dockRight);    // tab
    ImGui::DockBuilderDockWindow("Inspector",     dockRight);    // tab
    ImGui::DockBuilderDockWindow("Console",       dockBottom);
    ImGui::DockBuilderDockWindow("Lua API",       dockBottom);   // tab
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

    // F2H23: solo panels relevantes a Materiales — Material Editor
    // protagonista, AssetBrowser para arrastrar texturas, Viewport para
    // preview con la entidad seleccionada, Inspector + Hierarchy para
    // contexto. Console / LuaApi / Performance / ScriptEditor NO se
    // dockean.
    ImGui::DockBuilderDockWindow("Material Editor", dockLeft);
    ImGui::DockBuilderDockWindow("Asset Browser",   dockBottom);
    ImGui::DockBuilderDockWindow("Viewport",        dockMain);
    ImGui::DockBuilderDockWindow("Inspector",       dockRight);
    ImGui::DockBuilderDockWindow("Hierarchy",       dockLeft);    // tab
}

/// @brief Dispatcher: elige el builder segun el nombre del workspace
///        activo. F2H22: nombres nuevos en español orientados a tareas
///        (Modelar / Programar / Optimizar / Materiales). Acepta tambien
///        los nombres viejos (Layout / Scripting / Profile / Materials)
///        como alias defensivo, aunque la migracion en
///        `WorkspaceManager::setWorkspaces` los reemplaza al cargar.
///        Default = Modelar para nombres desconocidos.
void buildLayoutForWorkspace(const std::string& name, ImGuiID dockspaceId) {
    if      (name == "Programar"  || name == "Scripting") buildScriptingWorkspace(dockspaceId);
    else if (name == "Optimizar"  || name == "Profile")   buildProfileWorkspace(dockspaceId);
    else if (name == "Materiales" || name == "Materials") buildMaterialsWorkspace(dockspaceId);
    else                                                    buildLayoutWorkspace(dockspaceId);
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
