#include "editor/ui/EditorUI.h"

#include "editor/ui/IconsFontAwesome6.h"  // F2H79: ICON_FA_CIRCLE_XMARK (welcome)
#include "core/Log.h"
#include "core/i18n/I18n.h"  // F2H43
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
                &m_nodeGraphSandbox,  // F2H46
                &m_narrativeIntro,    // F2H46
                &m_dialogBrowser,     // F2H47
                &m_dialogEditor,      // F2H47
                &m_dialogInspector,   // F2H47
                &m_itemBrowser,       // F2H51
                &m_itemPropertyEditor,// F2H51
                &m_questBrowser,        // F2H53
                &m_questPropertyEditor, // F2H53
                &m_scriptEditor, &m_materialEditor,
                &m_shaderGraphEditor,  // F2H62 Bloque C
                // F2H59: Toolbar removido del dockspace -- las herramientas
                // (Mover/Rotar/Escala/Cara) viven ahora como overlay flotante
                // sobre el viewport, estilo Blender. El struct `m_toolbar`
                // queda como dead code linkeable (no inicializado por panels)
                // mientras no se reactive como panel independiente.
                // F2H28: paneles del workspace "Editor de mapas".
                // Arrancan ocultos; se hacen visibles via
                // applyDefaultVisibilityForWorkspace.
                &m_orthoTop, &m_orthoFront, &m_orthoSide,
                // F2H30 Bloque C: top toolbar del mismo workspace.
                &m_mapEditorTopBar,
                // F2H33: panel de VisGroups del mismo workspace.
                &m_visGroupsPanel};
    m_orthoTop.visible = false;
    m_orthoFront.visible = false;
    m_orthoSide.visible = false;
    m_mapEditorTopBar.visible = false;
    m_visGroupsPanel.visible = false;  // F2H33
    // F2H59: Toolbar como panel independiente queda inactivo (no en m_panels).
    // El ViewportPanel pinta las herramientas overlay sobre la imagen del
    // viewport directamente; recibe el puntero a UI aca.
    m_viewport.setEditorUi(this);
    m_mapEditorTopBar.setEditorUi(this);  // F2H30 Bloque C
    m_visGroupsPanel.setEditorUi(this);  // F2H33
    m_nodeGraphSandbox.setEditorUi(this);  // F2H46
    m_dialogBrowser.setEditorUi(this);     // F2H47
    m_dialogEditor.setEditorUi(this);      // F2H47
    m_dialogInspector.setEditorUi(this);   // F2H47
    m_itemBrowser.setEditorUi(this);       // F2H51
    m_itemPropertyEditor.setEditorUi(this);// F2H51
    m_questBrowser.setEditorUi(this);         // F2H53
    m_questPropertyEditor.setEditorUi(this);  // F2H53
    m_shaderGraphEditor.setEditorUi(this);    // F2H62 Bloque C

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

    // F2H29 fix lateral: forzar rebuild fresh del dockspace al ctor
    // para que el `imgui_layout_vN.ini` auto-loadeado de la sesion
    // previa NO muestre windows en posiciones stale durante la
    // pantalla Welcome. Sin esto, el dev veia el dockspace en
    // posiciones del ultimo workspace de la sesion previa mientras
    // el Welcome modal pedia abrir/crear proyecto. Cuando el dev
    // carga un proyecto, `setWorkspaces` restaura los iniLayout
    // custom por-workspace que vienen del `.moodproj` y este rebuild
    // queda overriden — sin perdida de personalizacion real.
    m_dockspace.requestRebuildForCurrentWorkspace();
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
    // F2H23 polish: SIEMPRE aplicar visibility default al cambiar
    // workspace, no solo la primera vez. Pedido del dev: "cuando me
    // muevo entre tabs los paneles se mezclan / quedan bugeados".
    // Aplicar default SIEMPRE da estado predecible — el dev abrir un
    // panel ajeno temporalmente y al cambiar de workspace y volver lo
    // encuentra cerrado. Es el sacrificio del feature de
    // "personalizacion por workspace" a cambio de "predecible".
    applyDefaultVisibilityForWorkspace(nextWs.name);
    if (!nextWs.iniLayout.empty()) {
        // ImGui restaura la posicion + tamano + dock state desde el ini.
        // El visible flag de cada panel manda igual — un panel con
        // visible=false no se renderea aunque el ini tenga su window
        // guardada.
        ImGui::LoadIniSettingsFromMemory(nextWs.iniLayout.c_str(),
                                          nextWs.iniLayout.size());
    } else {
        // Workspace nunca activado en este proyecto: rebuild via
        // DockBuilder para construir el layout default.
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
    // F2H28: cada rama oculta los 3 ortos (Top/Front/Side) explicitamente
    // para que al volver a un workspace no-Hammer queden cerrados. Solo
    // la rama "Editor de mapas" los activa.
    auto hideOrthoPanels = [&]() {
        setVisible("Top (XZ)",   false);
        setVisible("Front (XY)", false);
        setVisible("Side (ZY)",  false);
        setVisible("Map Tools",  false);  // F2H30 Bloque C: top toolbar
        setVisible("Grupos",     false);  // F2H33: VisGroups panel
    };
    // F2H46: en cualquier workspace que NO sea "narrative", ocultamos
    // el Node Graph Sandbox (queda accesible desde Ver > Debug).
    auto hideNarrativePanels = [&]() {
        setVisible("Node Graph Sandbox",       false);  // F2H46
        setVisible("Narrative Intro",          false);  // F2H46
        setVisible("Dialog Browser",           false);  // F2H47
        setVisible("Dialog Editor",            false);  // F2H47
        setVisible("Dialog Node Inspector",    false);  // F2H47
    };
    // F2H51: en cualquier workspace que NO sea "gameplay", ocultamos
    // los paneles de inventario.
    auto hideGameplayPanels = [&]() {
        setVisible("Item Browser",          false);  // F2H51
        setVisible("Item Property Editor",  false);  // F2H51
    };

    // F2H44: comparacion contra IDs ASCII (no labels visibles).
    if (name == "scripting") {
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
        hideOrthoPanels();
        hideNarrativePanels();
        hideGameplayPanels();
    } else if (name == "materials") {
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
        hideOrthoPanels();
        hideNarrativePanels();
        hideGameplayPanels();
    } else if (name == "narrative") {
        // F2H46: workspace "Narrativa" para Sub-fase 2.5.
        // F2H47: Dialog Editor + Browser + Node Inspector default.
        // F2H48: + Viewport 3D para ver NPCs reales y posicionar
        // triggers de dialog en la escena (pedido del dev: *"vamos a
        // necesitar un area 3D, yo reemplazaria el narrative intro por
        // un viewport 3D"*). Inspector/Escena se abren flotantes via
        // Ver si emerge necesidad — el workspace Narrativa se enfoca
        // en escena 3D + dialog editing.
        setVisible("Viewport",            true);   // F2H48: 3D activo
        setVisible("Escena",              false);
        setVisible("Inspector",           false);
        setVisible("Asset Browser",       false);
        setVisible("Console",             false);
        setVisible("Lua API",             false);
        setVisible("Performance",         false);
        setVisible("Script Editor",       false);
        setVisible("Material Editor",     false);
        setVisible("Tools",               false);
        setVisible("Node Graph Sandbox",      false);
        setVisible("Narrative Intro",         false);  // F2H48: removido del default
        setVisible("Dialog Browser",          true);  // F2H47
        setVisible("Dialog Editor",           true);  // F2H47
        setVisible("Dialog Node Inspector",   true);  // F2H47
        hideOrthoPanels();
        hideGameplayPanels();
    } else if (name == "gameplay") {
        // F2H51: workspace "Gameplay" para Sub-fase 2.5 Bloque 1 (Inventario).
        // Item Browser izq + Viewport 3D centro + Item Property Editor + Inspector der.
        // El Inspector cuando hay InventoryComponent muestra la seccion de inventario.
        setVisible("Viewport",              true);
        setVisible("Escena",                false);
        setVisible("Inspector",             true);
        setVisible("Asset Browser",         false);
        setVisible("Console",               false);
        setVisible("Lua API",               false);
        setVisible("Performance",           false);
        setVisible("Script Editor",         false);
        setVisible("Material Editor",       false);
        setVisible("Tools",                 false);
        setVisible("Item Browser",          true);   // F2H51
        setVisible("Item Property Editor",  true);   // F2H51
        hideOrthoPanels();
        hideNarrativePanels();
    } else if (name == "map_editor") {
        // F2H28: workspace 4-viewport inspirado en Valve Hammer Editor.
        // Viewport (perspectiva en top-right) + 3 ortos. Inspector y
        // Escena ocultos por default — el dev los abre flotantes desde
        // menu Ver si los necesita.
        // F2H30 Bloque C: + top toolbar "Map Tools" con sub-modo
        // buttons (Objeto/Vertex/Edge/Cara/Pincel).
        setVisible("Viewport",        true);
        setVisible("Top (XZ)",        true);
        setVisible("Front (XY)",      true);
        setVisible("Side (ZY)",       true);
        setVisible("Map Tools",       true);
        setVisible("Grupos",          true);  // F2H33: VisGroups panel
        setVisible("Escena",          false);
        setVisible("Inspector",       false);
        setVisible("Asset Browser",   false);
        setVisible("Console",         false);
        setVisible("Lua API",         false);
        setVisible("Performance",     false);
        setVisible("Script Editor",   false);
        setVisible("Material Editor", false);
        setVisible("Tools",           false);
        hideNarrativePanels();
        hideGameplayPanels();
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
        hideOrthoPanels();
        hideNarrativePanels();
        hideGameplayPanels();
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
    if (!ImGui::IsPopupOpen("MoodEngine - bienvenida")) {
        ImGui::OpenPopup("MoodEngine - bienvenida");
    }

    // Centrar el modal cada frame usando WorkPos + WorkSize/2 (excluye
    // menubar). ImGuiCond_Always en vez de Appearing: en pantallas >720p
    // el viewport del primer frame todavia no tiene el tamano final y el
    // centrado inicial queda off-center; recalculando cada frame el modal
    // sigue centrado aunque la ventana del OS se redimensione.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // F2H79: remake estilo Blender (splash de "nuevo archivo"): banner con
    // nombre+version arriba, y debajo dos columnas — EMPEZAR (acciones) a la
    // izquierda, RECIENTES (lista) a la derecha. Tamano fijo (sin
    // AlwaysAutoResize) para que el layout de 2 columnas no "respire".
    ImGui::SetNextWindowSize(ImVec2(640.0f, 380.0f), ImGuiCond_Appearing);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;

    if (ImGui::BeginPopupModal("MoodEngine - bienvenida", nullptr, flags)) {
        // --- Banner: nombre grande + version, sobre una banda mas oscura ---
        const float bannerH = 78.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 1.0f));
        ImGui::BeginChild("##welcome_banner", ImVec2(0.0f, bannerH), false);
        ImGui::SetCursorPos(ImVec2(20.0f, 16.0f));
        ImGui::SetWindowFontScale(1.9f);
        ImGui::TextUnformatted("MoodEngine");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPosX(22.0f);
        ImGui::TextDisabled("%s", I18n::T("editor.modal.about.version").c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // El cuerpo de 2 columnas usa toda la altura restante menos el hint.
        const float footerH = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
        const float bodyH = ImGui::GetContentRegionAvail().y - footerH;
        const float leftW = 210.0f;

        // --- Columna izquierda: EMPEZAR ---
        ImGui::BeginChild("##welcome_start", ImVec2(leftW, bodyH), false);
        ImGui::TextDisabled("%s", I18n::T("editor.welcome.start_header").c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        const float btnW = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(I18n::T("editor.welcome.new_project").c_str(),
                          ImVec2(btnW, 36.0f))) {
            requestProjectAction(ProjectAction::NewProject);
            ImGui::CloseCurrentPopup();
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button(I18n::T("editor.welcome.open_project").c_str(),
                          ImVec2(btnW, 36.0f))) {
            requestProjectAction(ProjectAction::OpenProject);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- Columna derecha: RECIENTES (la propia child scrollea) ---
        ImGui::BeginChild("##welcome_recents", ImVec2(0.0f, bodyH), false);
        ImGui::TextDisabled("%s", I18n::T("editor.welcome.recents_header").c_str());
        ImGui::Separator();

        if (m_recentProjects.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TextDisabled("%s", I18n::T("editor.welcome.no_recent").c_str());
        } else {
            std::filesystem::path toErase; // diferido hasta despues del loop
            for (const auto& path : m_recentProjects) {
                ImGui::PushID(path.generic_string().c_str());

                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                const std::string label = path.stem().generic_string() +
                                          "  -  " + path.generic_string() +
                                          (exists ? std::string{}
                                                  : std::string("  ") +
                                                    I18n::T("editor.welcome.missing_marker"));

                const float rowAvail = ImGui::GetContentRegionAvail().x;
                const float xButtonW = 30.0f; // margen suficiente para no recortar

                if (!exists) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        ImVec4(0.7f, 0.4f, 0.4f, 1.0f));
                }
                if (ImGui::Selectable(label.c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick,
                                       ImVec2(rowAvail - xButtonW - 6.0f, 0))) {
                    if (exists) {
                        m_openProjectPath = path;
                        ImGui::CloseCurrentPopup();
                    } else {
                        toErase = path;
                    }
                }
                if (!exists) ImGui::PopStyleColor();

                // Boton de quitar: icono ⊗ sin fondo, con hover gris sutil
                // (no la cajita azul) y margen al borde para que no se recorte.
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.40f, 0.45f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.50f, 0.55f, 0.70f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.60f, 0.60f, 0.64f, 1.0f));
                if (ImGui::SmallButton(ICON_FA_CIRCLE_XMARK)) {
                    toErase = path;
                }
                ImGui::PopStyleColor(4);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s",
                        I18n::T("editor.welcome.remove_from_recents").c_str());
                }
                ImGui::PopID();
            }
            if (!toErase.empty()) {
                eraseRecent(toErase);
            }
        }
        ImGui::EndChild();

        // --- Footer: "Limpiar inexistentes" a la derecha (si hay recientes) ---
        // F2H79: sin texto de bloqueo (ruido innecesario; el modal ya impide
        // interactuar con el editor). El boton lleva padding propio para no
        // verse apretado.
        if (!m_recentProjects.empty()) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));
            const std::string cleanLabel = I18n::T("editor.welcome.clean_missing");
            const float cleanBtnW = ImGui::CalcTextSize(cleanLabel.c_str()).x + 24.0f;
            const float availX = ImGui::GetContentRegionAvail().x;
            if (availX > cleanBtnW) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availX - cleanBtnW);
            }
            if (ImGui::Button(cleanLabel.c_str())) {
                pruneMissingRecents();
            }
            ImGui::PopStyleVar();
        }

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

    if (!ImGui::BeginMenu(I18n::T("editor.menu.boolean").c_str(), ready)) {
        return;
    }

    // Header informativo: cuantos brushes y cual es la "tool".
    const std::string activeTag = (m_selectionSet.active &&
                                    m_selectionSet.active.hasComponent<TagComponent>())
        ? m_selectionSet.active.getComponent<TagComponent>().name
        : std::string{"?"};
    ImGui::TextDisabled("%s",
        I18n::T("editor.menu.boolean.brushes_selected", brushCount).c_str());
    ImGui::TextDisabled("%s",
        I18n::T("editor.menu.boolean.tool_brush", activeTag).c_str());
    ImGui::Separator();

    if (ImGui::MenuItem(I18n::T("editor.menu.boolean.subtract").c_str())) {
        requestBooleanOp(BooleanOpRequestKind::Subtract);
    }
    if (ImGui::MenuItem(I18n::T("editor.menu.boolean.union").c_str())) {
        requestBooleanOp(BooleanOpRequestKind::Union);
    }
    if (ImGui::MenuItem(I18n::T("editor.menu.boolean.intersect").c_str())) {
        requestBooleanOp(BooleanOpRequestKind::Intersect);
    }

    ImGui::EndMenu();
}

} // namespace Mood
