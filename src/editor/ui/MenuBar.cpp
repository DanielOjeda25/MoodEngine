#include "editor/ui/MenuBar.h"

#include "core/Log.h"
#include "core/UserSettings.h"  // F2H43
#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"  // F2H43
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
        if (ImGui::BeginMenu((std::string(ICON_FA_CUBES_STACKED " ") + I18n::T("editor.menu.brush")).c_str(), ui.hasProject())) {
            // F2H11 + F2H14: primitivas CSG para el mapa actual.
            if (ImGui::BeginMenu(I18n::T("editor.menu.brush.add").c_str())) {
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.box").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddBoxBrush);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.cylinder").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddCylinderBrush);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.sphere").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddSphereBrush);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.pyramid").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddPyramidBrush);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.wedge").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddWedgeBrush);
                }
                ImGui::Separator();
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.prism_tri").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddPrismTriangularBrush);
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.brush.prism_hex").c_str())) {
                    ui.requestProjectAction(ProjectAction::AddPrismHexagonalBrush);
                }
                ImGui::EndMenu();
            }
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
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((std::string(ICON_FA_EYE " ") + I18n::T("editor.menu.view")).c_str())) {
            // F2H7: agrupar paneles por categoria (Scene/Assets/Debug/World).
            // Cada IPanel sobreescribe `category()` o usa el default "Scene".
            // El order de las categorias arriba es deliberado: Scene primero
            // (lo mas frecuente), Assets despues, Debug al final, World
            // placeholder hasta F2H10+ (CSG).
            const char* kCategories[] = {"Scene", "Assets", "Debug", "World"};
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
            // F2H43: selector de idioma (persiste en %APPDATA%\MoodEngine\settings.json).
            if (ImGui::BeginMenu(I18n::T("editor.menu.view.language").c_str())) {
                const auto current = I18n::currentLanguage();
                if (ImGui::MenuItem(I18n::T("editor.menu.view.language.english").c_str(),
                                     nullptr, current == I18n::Language::English)) {
                    if (I18n::setLanguage(I18n::Language::English)) {
                        UserSettings::setLanguage(I18n::Language::English);
                        UserSettings::save();
                    }
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.view.language.spanish").c_str(),
                                     nullptr, current == I18n::Language::Spanish)) {
                    if (I18n::setLanguage(I18n::Language::Spanish)) {
                        UserSettings::setLanguage(I18n::Language::Spanish);
                        UserSettings::save();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
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

        if (ImGui::BeginMenu((std::string(ICON_FA_CIRCLE_QUESTION " ") + I18n::T("editor.menu.help")).c_str())) {
            if (ImGui::MenuItem(I18n::T("editor.menu.help.about").c_str())) {
                m_showAboutPopup = true;
            }
            ImGui::Separator();
            if (ImGui::BeginMenu(I18n::T("editor.menu.help.demos").c_str(), ui.hasProject())) {
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.rotator").c_str())) {
                    ui.requestSpawnRotator();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.hud").c_str())) {
                    ui.requestSpawnHudDemo();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.enemy").c_str())) {
                    ui.requestSpawnEnemyDemo();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.audio").c_str())) {
                    ui.requestSpawnAudioSource();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.point_light").c_str())) {
                    ui.requestSpawnPointLight();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.physics_box").c_str())) {
                    ui.requestSpawnPhysicsBox();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.environment").c_str())) {
                    ui.requestSpawnEnvironment();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.shadow").c_str())) {
                    ui.requestSpawnShadowDemo();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.pbr_spheres").c_str())) {
                    ui.requestSpawnPbrSpheres();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.light_stress").c_str())) {
                    ui.requestSpawnLightStress();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.animated_char").c_str())) {
                    ui.requestSpawnAnimatedCharacter();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.fire_particles").c_str())) {
                    ui.requestSpawnFireParticles();
                }
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.trigger").c_str())) {
                    ui.requestSpawnTrigger();
                }
                // F2H47: demo de un .mooddialog pre-armado (3 nodos +
                // 2 choices). Lo abre directo en el DialogEditor sin
                // necesidad de crear un asset desde cero.
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.dialog").c_str())) {
                    ui.requestSpawnDialogDemo();
                }
                // F2H50 Bloque A: demo end-to-end de narrativa. Genera
                // (si falta) `assets/maps/narrative_demo.moodmap` con un
                // NPC armado con DialogComponent + Trigger + Animator,
                // y lo abre. El player puede entrar a Play Mode y hablar
                // con el NPC presionando E.
                if (ImGui::MenuItem(I18n::T("editor.menu.help.demo.narrative_map").c_str())) {
                    ui.requestGenerateNarrativeDemoMap();
                }

                ImGui::Separator();
                if (ImGui::BeginMenu(I18n::T("editor.menu.help.stress").c_str())) {
                    if (ImGui::MenuItem(I18n::T("editor.menu.help.stress.10k").c_str())) {
                        ui.requestSpawnStressTris(10000);
                    }
                    if (ImGui::MenuItem(I18n::T("editor.menu.help.stress.100k").c_str())) {
                        ui.requestSpawnStressTris(100000);
                    }
                    if (ImGui::MenuItem(I18n::T("editor.menu.help.stress.500k").c_str())) {
                        ui.requestSpawnStressTris(500000);
                    }
                    if (ImGui::MenuItem(I18n::T("editor.menu.help.stress.1m").c_str())) {
                        ui.requestSpawnStressTris(1000000);
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem(I18n::T("editor.menu.help.full_stress").c_str())) {
                    ui.requestSpawnFullStressScene();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        // F2H7: workspace tabs en la misma menu bar — estilo Blender.
        // Despues de los menus (Archivo/Editar/Ver/Ayuda) y antes del
        // boton Play. Buttons con highlight del activo, sin BeginTabBar
        // para evitar conflictos con state interno de ImGui.
        ImGui::Separator();
        {
            auto& wm = ui.workspaceManager();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            for (int i = 0; i < static_cast<int>(wm.count()); ++i) {
                const auto& ws = wm.workspaces()[i];
                const bool isActive = (i == wm.activeIndex());
                if (isActive) {
                    const ImVec4 hi = ImGui::GetStyleColorVec4(ImGuiCol_TabActive);
                    ImGui::PushStyleColor(ImGuiCol_Button, hi);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hi);
                } else {
                    const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
                    ImGui::PushStyleColor(ImGuiCol_Button, dim);
                }
                // F2H37+F2H44: prefijo icon + label visible traducido.
                // `ws.name` es el ID ASCII estable; el label muestra la
                // traduccion via `T("workspace.<id>")`.
                const std::string visibleLabel =
                    I18n::T("workspace." + ws.name);
                std::string label;
                label.reserve(visibleLabel.size() + 8);
                label += iconForWorkspace(ws.name);
                label += ' ';
                label += visibleLabel;
                if (ImGui::Button(label.c_str())) {
                    if (!isActive) ui.requestWorkspaceSwitch(i);
                }
                ImGui::PopStyleColor(isActive ? 2 : 1);
            }
            ImGui::PopStyleVar();
        }

        // Boton Play/Stop empujado a la derecha de la menu bar.
        const bool isPlay = ui.mode() == EditorMode::Play;
        const std::string playStopLabel = isPlay
            ? (std::string(ICON_FA_STOP " ") + I18n::T("editor.menu.stop"))
            : (std::string(ICON_FA_PLAY " ") + I18n::T("editor.menu.play"));
        const char* btnLabel = playStopLabel.c_str();
        const float btnWidth = 80.0f; // F2H37: bumped 64->80 px para acomodar icon
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
    // F2H43: titles de popups son IDs internos de ImGui — NO traducir
    // (cambiar el ID rompe BeginPopupModal). Se mantienen en codigo;
    // el usuario nunca los ve en el title bar (los modals son
    // AlwaysAutoResize sin titulo visible relevante).
    if (m_showAboutPopup) {
        ImGui::OpenPopup("##about_modal");
        m_showAboutPopup = false;
    }
    if (m_showNotImplementedPopup) {
        ImGui::OpenPopup("##notimpl_modal");
        m_showNotImplementedPopup = false;
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

    if (ImGui::BeginPopupModal("##notimpl_modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", I18n::T("editor.modal.notimpl.body").c_str());
        if (ImGui::Button(I18n::T("editor.modal.common.ok").c_str(), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Mood
