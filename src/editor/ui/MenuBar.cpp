#include "editor/ui/MenuBar.h"

#include "core/Log.h"
#include "core/UserSettings.h"  // F2H43
#include "editor/ui/EditorUI.h"
#include "editor/ui/EditorThemes.h"  // F2H76
#include "core/i18n/I18n.h"  // F2H43
#include "editor/ui/IconsFontAwesome6.h"
#include "editor/panels/IPanel.h"

#include <imgui.h>

#include <string>
#include <string_view>

namespace Mood {

namespace {

// F2H37: mapping de ID de workspace a icono FA. Hardcoded porque los
// IDs son fijos en `WorkspaceManager.cpp` y un mapping generico
// requeriria que cada workspace declarara su icon — overkill.
// F2H44: comparacion contra IDs ASCII (no labels visibles, que ahora
// vienen de `T("workspace.<id>")` y cambian con idioma).
const char* iconForWorkspace(const std::string& id) {
    if (id == "layout"    ) return ICON_FA_TABLE_COLUMNS;
    if (id == "scripting" ) return ICON_FA_CODE;
    if (id == "materials" ) return ICON_FA_PALETTE;
    if (id == "map_editor") return ICON_FA_MAP;
    return ICON_FA_TABLE_COLUMNS; // fallback razonable
}

} // namespace

void MenuBar::draw(EditorUI& ui, bool& requestQuit) {
    if (ImGui::BeginMenuBar()) {
        // F2H18: reorganizacion. Archivo solo tiene file ops del
        // proyecto + asset ops globales (script/prefab) + Salir. El
        // submenu Mapa se promovio a top-level; geometria (Brush
        // primitivas + Boolean) salio a top-level "Brush".
        if (ImGui::BeginMenu((std::string(ICON_FA_FOLDER " ") + I18n::T("editor.menu.file")).c_str())) {
            if (ImGui::MenuItem(I18n::T("editor.menu.file.new").c_str())) {
                ui.requestProjectAction(ProjectAction::NewProject);
            }
            if (ImGui::MenuItem(I18n::T("editor.menu.file.open").c_str())) {
                ui.requestProjectAction(ProjectAction::OpenProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.menu.file.save").c_str(), "Ctrl+S", false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::Save);
            }
            if (ImGui::MenuItem(I18n::T("editor.menu.file.close").c_str(), nullptr, false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::CloseProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.menu.file.package").c_str(), nullptr, false,
                                ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::PackageProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.menu.file.new_script").c_str())) {
                ui.requestProjectAction(ProjectAction::NewScript);
            }
            const bool canSavePrefab = static_cast<bool>(ui.selectedEntity());
            if (ImGui::MenuItem(I18n::T("editor.menu.file.save_prefab").c_str(), nullptr, false, canSavePrefab)) {
                ui.requestSavePrefabDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.menu.file.exit").c_str(), "Alt+F4")) {
                requestQuit = true;
            }
            ImGui::EndMenu();
        }

        // F2H18: top-level "Mapa". Antes era Archivo > Mapa. File ops
        // del mapa actual del proyecto activo (multi-mapa de F2H8).
        if (ImGui::BeginMenu((std::string(ICON_FA_MAP " ") + I18n::T("editor.menu.map")).c_str(), ui.hasProject())) {
            if (ImGui::MenuItem(I18n::T("editor.menu.map.new").c_str())) {
                ui.requestProjectAction(ProjectAction::NewMap);
            }
            if (ImGui::BeginMenu(I18n::T("editor.menu.map.open").c_str(), !ui.projectMaps().empty())) {
                for (const auto& p : ui.projectMaps()) {
                    const std::string display = p.filename().generic_string();
                    const bool isCurrent =
                        p.generic_string() == ui.currentMapPath().generic_string();
                    const bool isDefault =
                        p.generic_string() == ui.defaultMapPath().generic_string();
                    // Marca visual: bullet • si es current; estrella ★ si es default.
                    std::string label;
                    if (isCurrent) label += "* ";
                    label += display;
                    if (isDefault) { label += "  "; label += I18n::T("editor.menu.map.default_marker"); }
                    if (ImGui::MenuItem(label.c_str(), nullptr, isCurrent)) {
                        if (!isCurrent) ui.requestOpenMap(p);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(I18n::T("editor.menu.map.save_as").c_str())) {
                ui.requestProjectAction(ProjectAction::SaveMapAs);
            }
            {
                const bool alreadyDefault =
                    ui.currentMapPath().generic_string() ==
                    ui.defaultMapPath().generic_string();
                if (ImGui::MenuItem(I18n::T("editor.menu.map.set_default").c_str(), nullptr,
                                      alreadyDefault, !alreadyDefault)) {
                    ui.requestProjectAction(ProjectAction::SetCurrentMapAsDefault);
                }
            }
            {
                const bool canDelete = ui.projectMaps().size() > 1u;
                if (ImGui::MenuItem(I18n::T("editor.menu.map.delete").c_str(), nullptr,
                                      false, canDelete)) {
                    ui.requestProjectAction(ProjectAction::DeleteCurrentMap);
                }
            }
            // F2H20: compilacion brush -> mesh estatica + export OBJ.
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.menu.map.compile").c_str())) {
                ui.requestProjectAction(ProjectAction::CompileMap);
            }
            if (ImGui::MenuItem(I18n::T("editor.menu.map.export_obj").c_str())) {
                ui.requestProjectAction(ProjectAction::ExportObj);
            }
            ImGui::EndMenu();
        }

        // F2H18: top-level "Brush". Geometria (primitivas + booleanos).
        // Antes vivia anidada como Archivo > Mapa > {Anadir Brush, Boolean}.
        // F2H59: submenu "Anadir" removido -- las primitivas ahora viven
        // en el modal "+ Crear Entidad" del panel Escena (tab "Primitivas").
        // Workflow Hammer/SFM: un solo punto de entrada para spawnear
        // geometria, sea mesh importado o primitiva procedural. Si emerge
        // demanda de "agregar primitiva sin abrir modal", re-evaluar.
        if (ImGui::BeginMenu((std::string(ICON_FA_CUBES_STACKED " ") + I18n::T("editor.menu.brush")).c_str(), ui.hasProject())) {
            // F2H12: operaciones booleanas entre brushes.
            // A = entidad seleccionada (debe tener BrushComponent);
            // B se elige del submenu listando los demas brushes.
            ui.drawBooleanOpMenu();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((std::string(ICON_FA_PEN_TO_SQUARE " ") + I18n::T("editor.menu.edit")).c_str())) {
            // Hito 27: cableado a HistoryStack inyectado por EditorApplication.
            // Hasta que el ctor termine, m_history puede ser nullptr — evitamos
            // crash deshabilitando los items.
            HistoryStack* h = ui.historyStack();
            const bool canUndo = (h != nullptr && h->canUndo());
            const bool canRedo = (h != nullptr && h->canRedo());
            const std::string undoLabel = canUndo
                ? I18n::T("editor.menu.edit.undo_named", h->undoName())
                : I18n::T("editor.menu.edit.undo");
            const std::string redoLabel = canRedo
                ? I18n::T("editor.menu.edit.redo_named", h->redoName())
                : I18n::T("editor.menu.edit.redo");
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo)) {
                h->undo();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo)) {
                h->redo();
            }
            ImGui::Separator();
            // F2H76: Preferencias (tema + idioma). Casa de los ajustes del
            // editor; futuros hitos de 2.7 suman atajos / escala de UI.
            if (ImGui::MenuItem(I18n::T("editor.menu.edit.preferences").c_str())) {
                m_showPreferencesPopup = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((std::string(ICON_FA_EYE " ") + I18n::T("editor.menu.view")).c_str())) {
            // F2H7: agrupar paneles por categoria (Scene/Assets/Debug/World).
            // Cada IPanel sobreescribe `category()` o usa el default "Scene".
            // El order de las categorias arriba es deliberado: Scene primero
            // (lo mas frecuente), Assets despues, Debug al final, World
            // placeholder hasta F2H10+ (CSG).
            const char* kCategories[] = {"Scene", "Assets", "Narrative",
                                          "Gameplay", "Debug"};
            for (const char* cat : kCategories) {
                if (ImGui::BeginMenu(cat)) {
                    bool any = false;
                    for (IPanel* panel : ui.panels()) {
                        if (std::string_view(panel->category()) == cat) {
                            ImGui::MenuItem(panel->name(), nullptr, &panel->visible);
                            any = true;
                        }
                    }
                    if (!any) {
                        ImGui::TextDisabled("%s", I18n::T("editor.menu.view.empty").c_str());
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            // F2H76: el selector de idioma se movio a Editar -> Preferencias
            // (un solo lugar para todos los ajustes).
            if (ImGui::MenuItem(I18n::T("editor.menu.view.reset_layout").c_str())) {
                // F2H22: el reset re-aplica tanto el dock layout (via
                // DockBuilder en el proximo frame) como la visibility
                // default de los panels — sin esto, los panels que el
                // dev abrio en su iniLayout custom quedaban visibles
                // tras el reset.
                ui.applyDefaultVisibilityForWorkspace(
                    ui.workspaceManager().activeWorkspace().name);
                ui.dockspace().requestResetToDefault();
                Log::editor()->info("Layout restablecido al default");
            }
            ImGui::EndMenu();
        }

        // Menu Debug: stress tests para benchmarkear perf cuando se toca
        // un subsistema (Forward+ light grid, scene iteration, draw-call
        // cost). post-v2.0.2: agregado para reemplazar el entry point
        // perdido cuando F2H57 eliminó el submenú "Demos" del menu Ayuda.
        if (ImGui::BeginMenu((std::string(ICON_FA_BUG " ") + I18n::T("editor.menu.debug")).c_str(),
                              ui.hasProject())) {
            if (ImGui::MenuItem(I18n::T("editor.menu.debug.light_stress").c_str())) {
                ui.requestSpawnLightStress();
            }
            if (ImGui::BeginMenu(I18n::T("editor.menu.debug.stress_tris").c_str())) {
                if (ImGui::MenuItem(I18n::T("editor.menu.debug.stress_tris.10k").c_str())) {
                    ui.requestSpawnStressTris(10000);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.debug.stress_tris.100k").c_str())) {
                    ui.requestSpawnStressTris(100000);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.debug.stress_tris.500k").c_str())) {
                    ui.requestSpawnStressTris(500000);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.debug.stress_tris.1m").c_str())) {
                    ui.requestSpawnStressTris(1000000);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((std::string(ICON_FA_CIRCLE_QUESTION " ") + I18n::T("editor.menu.help")).c_str())) {
            if (ImGui::MenuItem(I18n::T("editor.menu.help.about").c_str())) {
                m_showAboutPopup = true;
            }
            ImGui::EndMenu();
        }

        // F2H79: barra estilo Unity — Play CENTRADO + selector de workspace
        // (dropdown a la derecha). Antes los workspace tabs vivian aca
        // (estilo Blender), pero el Play pegado a ellos se disfrazaba de tab.
        ImGui::Separator();

        // --- Play / Stop centrado en la barra ---
        const bool isPlay = ui.mode() == EditorMode::Play;
        const std::string playStopLabel = isPlay
            ? (std::string(ICON_FA_STOP " ") + I18n::T("editor.menu.stop"))
            : (std::string(ICON_FA_PLAY " ") + I18n::T("editor.menu.play"));
        const float playWidth = 90.0f;
        const float playX = ImGui::GetWindowWidth() * 0.5f - playWidth * 0.5f;
        if (playX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(playX);
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
        if (ImGui::Button(playStopLabel.c_str(), ImVec2(playWidth, 0.0f))) {
            ui.requestTogglePlay();
        }
        ImGui::PopStyleColor(3);

        // --- Selector de workspace (boton hamburguesa a la derecha) ---
        // F2H79: el dropdown con el nombre del workspace activo abria el popup
        // ENCIMA del propio boton (tapaba "Layout"). Un boton hamburguesa fijo
        // (icono ☰) deja el popup caer limpio debajo y no cambia de ancho.
        {
            auto& wm = ui.workspaceManager();
            const float dropW = 34.0f;
            const float dropX = ImGui::GetWindowWidth() - dropW - 8.0f;
            if (dropX > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(dropX);
            }
            if (ImGui::Button(ICON_FA_BARS, ImVec2(dropW, 0.0f))) {
                ImGui::OpenPopup("##workspace_selector");
            }
            // Posiciona el popup justo debajo del boton, alineado a su borde
            // derecho (pivote arriba-derecha) en vez de encima del cursor.
            const ImVec2 btnMax = ImGui::GetItemRectMax();
            ImGui::SetNextWindowPos(ImVec2(btnMax.x, btnMax.y), ImGuiCond_Appearing,
                                    ImVec2(1.0f, 0.0f));
            if (ImGui::BeginPopup("##workspace_selector")) {
                for (int i = 0; i < static_cast<int>(wm.count()); ++i) {
                    const auto& ws = wm.workspaces()[i];
                    const std::string label =
                        std::string(iconForWorkspace(ws.name)) + "  "
                        + I18n::T("workspace." + ws.name);
                    const bool isActive = (i == wm.activeIndex());
                    if (ImGui::MenuItem(label.c_str(), nullptr, isActive)) {
                        if (!isActive) ui.requestWorkspaceSwitch(i);
                    }
                }
                ImGui::EndPopup();
            }
        }

        ImGui::EndMenuBar();
    }

    // --- Popups ---
    // F2H43: titles de popups son IDs internos de ImGui — NO traducir
    // (cambiar el ID rompe BeginPopupModal). Se mantienen en codigo;
    // el usuario nunca los ve en el title bar (los modals son
    // AlwaysAutoResize sin titulo visible relevante).
    if (m_showAboutPopup) {
        ImGui::OpenPopup("##about_modal");
        m_showAboutPopup = false;
    }
    if (m_showPreferencesPopup) {
        // F2H79: usamos "###preferences_modal" → el ID de ImGui es estable
        // ("preferences_modal") aunque el titulo visible (i18n) cambie de
        // idioma. m_prefsOpen habilita el boton X del titlebar.
        m_prefsOpen = true;
        ImGui::OpenPopup("###preferences_modal");
        m_showPreferencesPopup = false;
    }

    if (ImGui::BeginPopupModal("##about_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", I18n::T("editor.modal.about.title").c_str());
        ImGui::Text("%s", I18n::T("editor.modal.about.version").c_str());
        ImGui::Separator();
        ImGui::Text("%s", I18n::T("editor.modal.about.description").c_str());
        ImGui::Text("%s", I18n::T("editor.modal.about.repo").c_str());
        ImGui::Separator();
        if (ImGui::Button(I18n::T("editor.modal.common.close").c_str(), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // F2H76: modal de Preferencias (Tema + Idioma). Aplica/persiste live al
    // cambiar cada combo — settings.json es chico, sin boton OK/Cancel.
    // F2H79: titulo en el titlebar (no en el body) + boton X de cerrar (via
    // p_open). "###preferences_modal" mantiene el ID estable entre idiomas.
    const std::string prefsTitle =
        I18n::T("editor.modal.preferences.title") + "###preferences_modal";
    if (ImGui::BeginPopupModal(prefsTitle.c_str(), &m_prefsOpen,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        // --- Tema ---
        const auto& themes = EditorThemes::available();
        const std::string& curTheme = UserSettings::theme();
        int curThemeIdx = 0;
        for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
            if (themes[i].id == curTheme) { curThemeIdx = i; break; }
        }
        // Nombre legible (i18n) del tema actual para el preview del combo.
        const std::string curThemeLabel = I18n::T(themes[curThemeIdx].i18nKey);
        const std::string themeLabel =
            I18n::T("editor.preferences.theme") + "##pref_theme";
        if (ImGui::BeginCombo(themeLabel.c_str(), curThemeLabel.c_str())) {
            for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
                const bool sel = (i == curThemeIdx);
                if (ImGui::Selectable(I18n::T(themes[i].i18nKey).c_str(), sel)) {
                    UserSettings::setTheme(themes[i].id);
                    EditorThemes::apply(themes[i].id);  // preview live
                    UserSettings::save();
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- Idioma (centralizado aca desde View -> Language) ---
        const auto curLang = I18n::currentLanguage();
        const std::string curLangLabel = I18n::T(
            curLang == I18n::Language::English
                ? "editor.menu.view.language.english"
                : "editor.menu.view.language.spanish");
        const std::string langLabel =
            I18n::T("editor.menu.view.language") + "##pref_lang";
        if (ImGui::BeginCombo(langLabel.c_str(), curLangLabel.c_str())) {
            const bool isEn = (curLang == I18n::Language::English);
            if (ImGui::Selectable(
                    I18n::T("editor.menu.view.language.english").c_str(), isEn)) {
                if (I18n::setLanguage(I18n::Language::English)) {
                    UserSettings::setLanguage(I18n::Language::English);
                    UserSettings::save();
                }
            }
            const bool isEs = (curLang == I18n::Language::Spanish);
            if (ImGui::Selectable(
                    I18n::T("editor.menu.view.language.spanish").c_str(), isEs)) {
                if (I18n::setLanguage(I18n::Language::Spanish)) {
                    UserSettings::setLanguage(I18n::Language::Spanish);
                    UserSettings::save();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        if (ImGui::Button(I18n::T("editor.modal.common.close").c_str(),
                           ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Mood
