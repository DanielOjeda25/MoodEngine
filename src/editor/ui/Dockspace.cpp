#include "editor/ui/Dockspace.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Mood {

namespace {

/// @brief Layout default del workspace "Layout" — el de trabajo general
///        (mapping CSG + edicion de entidades). F2H22 lo renombro a
///        "Modelar"; F2H23 polish lo revierte a "Layout" (pedido del
///        dev). F2H22 agrego una franja angosta a la IZQUIERDA del
///        Hierarchy para la `Toolbar` (gizmo modes + primitivas brush +
///        face mode). Resto: Hierarchy(Escena) izq, Inspector der,
///        Asset Browser abajo, Viewport central.
void buildLayoutWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    // F2H22: franja para la Toolbar — ~100px en viewport 1280.
    // Botones de 72x36 (Mover/Rotar/Escala/Box/Cilindro/Cara) caben con
    // padding lateral.
    ImGuiID dockToolbar = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.08f, nullptr, &dockMain);
    // F2H23 polish: columna derecha unificada (Escena arriba +
    // Inspector abajo, dividida 50/50). Antes Escena vivia en columna
    // izquierda y consumia ancho que el dev no aprovechaba — ahora la
    // izquierda es solo la Toolbar y el Viewport ocupa todo el ancho
    // central. Pedido del dev: "Escena en la misma columna que
    // Inspector, arriba, y abajo inspector".
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(
        dockRight, ImGuiDir_Down, 0.50f, nullptr, &dockRight);
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);

    // F2H23: solo dockear los panels relevantes a ESTE workspace.
    // Antes (F2H22) dockeabamos TODOS los panels en cada workspace
    // — el dev veia "Script Editor" como tab al lado del Viewport en
    // Modelar y se confundia. Si el dev abre un panel ajeno desde
    // menu Ver, ImGui lo pone flotante (donde quiera) y el dev decide
    // si acoplarlo o no — el workspace no le impone una posicion.
    ImGui::DockBuilderDockWindow("Tools",         dockToolbar);
    ImGui::DockBuilderDockWindow("Escena",        dockRight);        // arriba der
    ImGui::DockBuilderDockWindow("Inspector",     dockRightBottom);  // abajo der
    ImGui::DockBuilderDockWindow("Viewport",      dockMain);
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
    ImGui::DockBuilderDockWindow("Escena",     dockLeft);
    ImGui::DockBuilderDockWindow("Inspector",     dockLeftBottom);
    ImGui::DockBuilderDockWindow("Viewport",      dockLeftBottom); // tab
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
    ImGui::DockBuilderDockWindow("Escena",       dockLeft);    // tab
}

/// @brief F2H28: workspace "Editor de mapas" — layout 2x2 inspirado en
///        Valve Hammer Editor. Top-left = vista Top (XY), top-right =
///        perspectiva 3D reusando el panel "Viewport" existente,
///        bottom-left = Front (XZ), bottom-right = Side (YZ). Sin
///        paneles auxiliares (Inspector/Escena) en el dockspace inicial
///        — el dev los abre flotantes desde menu Ver si los necesita.
///        F2H29 considerara agregar una columna lateral si emerge
///        necesidad real.
void buildMapEditorWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    // F2H30 Bloque C: columna lateral derecha (~10%) para "Map Tools"
    // — botones verticales tipo Hammer/Blender. Antes era top bar pero
    // los botones se aplastaban (issue del dev: "prefiero columna lado
    // derecho").
    ImGuiID dockRightBar = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.10f, nullptr, &dockMain);
    // 1) Split horizontal: arriba (dockMain) / abajo (dockBottom).
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Down, 0.50f, nullptr, &dockMain);
    // 2) Split vertical de la mitad de arriba: izq (dockMain) / der.
    ImGuiID dockTopRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.50f, nullptr, &dockMain);
    // 3) Split vertical de la mitad de abajo: izq (dockBottom) / der.
    ImGuiID dockBottomRight = ImGui::DockBuilderSplitNode(
        dockBottom, ImGuiDir_Right, 0.50f, nullptr, &dockBottom);

    // F2H28: nombres alineados con la convencion Y-up de MoodEngine:
    // Top muestra el plano XZ (cama mira -Y), Front el plano XY
    // (cam mira -Z), Side el plano ZY (cam mira -X).
    // F2H33: panel "Grupos" en la columna lateral derecha como tab al
    // lado de "Map Tools" — el dev alterna entre tools y la lista de
    // VisGroups sin cambiar el layout 4-viewport.
    ImGui::DockBuilderDockWindow("Map Tools",   dockRightBar);     // F2H30
    ImGui::DockBuilderDockWindow("Grupos",      dockRightBar);     // F2H33: tab al lado
    ImGui::DockBuilderDockWindow("Top (XZ)",    dockMain);         // top-left
    ImGui::DockBuilderDockWindow("Viewport",    dockTopRight);     // top-right
    ImGui::DockBuilderDockWindow("Front (XY)",  dockBottom);       // bottom-left
    ImGui::DockBuilderDockWindow("Side (ZY)",   dockBottomRight);  // bottom-right
}

/// @brief F2H46/F2H47/F2H48: workspace "Narrativa" — Sub-fase 2.5.
///        v1 (F2H46): Sandbox + Intro placeholder.
///        v2 (F2H47): Dialog Editor activo + Intro tabulado abajo.
///        v3 (F2H48): viewport 3D arriba, Dialog Editor abajo.
///        v3.1 (F2H48 polish): 3 columnas — viewport izq / Dialog Editor
///        centro / columna der dividida en Node Inspector arriba + Dialog
///        Browser abajo. Pedido del dev: *"prefiero dividamos en 3
///        columnas, una del 3D luego la narrativa, y otro dialog node
///        inspector ... arriba node inspector y abajo dialog browser"*.
///        Layout permite ver NPC + canvas de dialog + inspector
///        simultaneo sin tabs.
void buildNarrativeWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    // Col izq (viewport 3D): 30% del ancho.
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.30f, nullptr, &dockMain);
    // Col der (Inspector + Browser): 25% del ancho remanente.
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.30f, nullptr, &dockMain);
    // Col der: split vertical, Browser abajo 40%.
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(
        dockRight, ImGuiDir_Down, 0.40f, nullptr, &dockRight);

    ImGui::DockBuilderDockWindow("Viewport",                 dockLeft);
    ImGui::DockBuilderDockWindow("Dialog Editor",            dockMain);
    ImGui::DockBuilderDockWindow("Dialog Node Inspector",    dockRight);
    ImGui::DockBuilderDockWindow("Dialog Browser",           dockRightBottom);
}

/// @brief F2H51: workspace "Gameplay" — Sub-fase 2.5 Bloque 1 (Inventario).
///        3 columnas: Item Browser izq (25%) / Viewport 3D centro (45%) /
///        Property Editor + Inspector der (30%). Property Editor arriba,
///        Inspector abajo (tabuladear si el dev lo prefiere).
///        Permite ver simultaneo: lista de items + 3D del nivel +
///        edicion del item activo + entity inspector cuando se selecciona
///        una entidad con InventoryComponent en el viewport.
void buildGameplayWorkspace(ImGuiID dockspaceId) {
    ImGuiID dockMain = dockspaceId;
    // Col izq (Item Browser): 25%.
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Left, 0.25f, nullptr, &dockMain);
    // Col der (Item Property Editor + Inspector): ~33% del remanente.
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(
        dockMain, ImGuiDir_Right, 0.33f, nullptr, &dockMain);
    // Col der: split vertical, Inspector abajo 45%.
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(
        dockRight, ImGuiDir_Down, 0.45f, nullptr, &dockRight);

    ImGui::DockBuilderDockWindow("Item Browser",          dockLeft);
    ImGui::DockBuilderDockWindow("Viewport",              dockMain);
    ImGui::DockBuilderDockWindow("Item Property Editor",  dockRight);
    ImGui::DockBuilderDockWindow("Inspector",             dockRightBottom);
}

/// @brief Dispatcher: elige el builder segun el nombre del workspace
///        activo. F2H23: 3 workspaces — Layout / Programar / Materiales.
///        F2H28: + "Editor de mapas" 4-viewport.
///        Acepta nombres viejos (F2H7 originales: Scripting/Materials;
///        F2H22: Modelar) como alias defensivo, aunque la migracion en
///        `WorkspaceManager::setWorkspaces` los reemplaza al cargar.
///        El workspace "Optimizar"/"Profile" fue eliminado en F2H23 —
///        si llega un name asi (no deberia tras la migracion), cae al
///        layout default de Layout.
///        Default = Layout para nombres desconocidos.
void buildLayoutForWorkspace(const std::string& name, ImGuiID dockspaceId) {
    // F2H44: comparacion contra IDs ASCII (no labels visibles).
    if      (name == "map_editor") buildMapEditorWorkspace(dockspaceId);
    else if (name == "scripting" ) buildScriptingWorkspace(dockspaceId);
    else if (name == "materials" ) buildMaterialsWorkspace(dockspaceId);
    else if (name == "narrative" ) buildNarrativeWorkspace(dockspaceId);  // F2H46
    else if (name == "gameplay"  ) buildGameplayWorkspace(dockspaceId);   // F2H51
    else                            buildLayoutWorkspace(dockspaceId);
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
