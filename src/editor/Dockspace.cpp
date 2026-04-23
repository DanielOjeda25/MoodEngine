#include "editor/Dockspace.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Mood {

namespace {

/// @brief Arma el layout por defecto la primera vez. Solo corre si el
///        dockspace no tiene nodos hijos aun (imgui.ini ausente o vacio).
///        Si el usuario ya movio paneles antes, los respeta tal cual.
void buildDefaultLayoutIfEmpty(ImGuiID dockspaceId, const ImVec2& size) {
    static bool attempted = false;
    if (attempted) return;
    attempted = true;

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
    if (node != nullptr && node->IsSplitNode()) {
        // Ya hay un layout persistido: no tocar.
        return;
    }

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    // Layout por defecto:
    //   +--------------------------------+----------+
    //   |                                |          |
    //   |          Viewport              | Inspector|
    //   |                                |          |
    //   +-----------------+--------------+----------+
    //   |  Asset Browser  |  Console                |
    //   +-----------------+-------------------------+
    //
    // Orden de splits importa porque cada uno "come" del dock padre.
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);
    // Parte el bottom en dos: izquierda para Asset Browser, derecha para Console.
    ImGuiID dockBottomRight = ImGui::DockBuilderSplitNode(
        dockBottom, ImGuiDir_Right, 0.45f, nullptr, &dockBottom);

    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
    ImGui::DockBuilderDockWindow("Inspector",     dockRight);
    ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Console",       dockBottomRight);
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
        buildDefaultLayoutIfEmpty(dockspaceId, viewport->WorkSize);
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    }

    return open;
}

void Dockspace::end() {
    ImGui::End();
}

} // namespace Mood
