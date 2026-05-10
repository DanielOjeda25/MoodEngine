#include "editor/ui/MenuBar.h"

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "editor/panels/IPanel.h"

#include <imgui.h>

#include <string>
#include <string_view>

namespace Mood {

namespace {

// F2H37: mapping de nombre de workspace a icono FA. Hardcoded porque
// los nombres son fijos en `WorkspaceManager.cpp` y un mapping
// generico requeriria que cada workspace declarara su icon — overkill.
const char* iconForWorkspace(const std::string& name) {
    if (name == "Layout")          return ICON_FA_TABLE_COLUMNS;
    if (name == "Programar")        return ICON_FA_CODE;
    if (name == "Materiales")       return ICON_FA_PALETTE;
    if (name == "Editor de mapas")  return ICON_FA_MAP;
    return ICON_FA_TABLE_COLUMNS; // fallback razonable
}

} // namespace

void MenuBar::draw(EditorUI& ui, bool& requestQuit) {
    if (ImGui::BeginMenuBar()) {
        // F2H18: reorganizacion. Archivo solo tiene file ops del
        // proyecto + asset ops globales (script/prefab) + Salir. El
        // submenu Mapa se promovio a top-level; geometria (Brush
        // primitivas + Boolean) salio a top-level "Brush".
        if (ImGui::BeginMenu(ICON_FA_FOLDER " Archivo")) {
            if (ImGui::MenuItem("Nuevo Proyecto")) {
                ui.requestProjectAction(ProjectAction::NewProject);
            }
            if (ImGui::MenuItem("Abrir Proyecto")) {
                ui.requestProjectAction(ProjectAction::OpenProject);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Guardar", "Ctrl+S", false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::Save);
            }
            if (ImGui::MenuItem("Cerrar proyecto", nullptr, false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::CloseProject);
            }
            ImGui::Separator();
            // Hito 21 Bloque 5: empaqueta el proyecto activo en una
            // carpeta autocontenida (MoodPlayer.exe + DLLs + assets +
            // shaders + project + game.json). Solo disponible con
            // proyecto guardado — si esta dirty obliga a Save primero.
            if (ImGui::MenuItem("Empaquetar proyecto...", nullptr, false,
                                ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::PackageProject);
            }
            ImGui::Separator();
            // Hito 22 Bloque 3: crea un .lua nuevo en assets/scripts/.
            // Los scripts son assets globales del repo (igual que prefabs),
            // asi que no se exige proyecto activo.
            if (ImGui::MenuItem("Nuevo Script...")) {
                ui.requestProjectAction(ProjectAction::NewScript);
            }
            // Hito 14: guardar la entidad seleccionada como prefab. Los
            // prefabs son assets globales del repo (`<cwd>/assets/prefabs/`),
            // no per-proyecto, asi que basta con tener una entidad
            // seleccionada — no se exige proyecto activo.
            const bool canSavePrefab = static_cast<bool>(ui.selectedEntity());
            if (ImGui::MenuItem("Guardar como prefab...", nullptr, false, canSavePrefab)) {
                ui.requestSavePrefabDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Salir", "Alt+F4")) {
                requestQuit = true;
            }
            ImGui::EndMenu();
        }

        // F2H18: top-level "Mapa". Antes era Archivo > Mapa. File ops
        // del mapa actual del proyecto activo (multi-mapa de F2H8).
        if (ImGui::BeginMenu(ICON_FA_MAP " Mapa", ui.hasProject())) {
            if (ImGui::MenuItem("Nuevo mapa")) {
                ui.requestProjectAction(ProjectAction::NewMap);
            }
            if (ImGui::BeginMenu("Abrir mapa", !ui.projectMaps().empty())) {
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
                    if (isDefault) label += "  [default]";
                    if (ImGui::MenuItem(label.c_str(), nullptr, isCurrent)) {
                        if (!isCurrent) ui.requestOpenMap(p);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Guardar mapa como...")) {
                ui.requestProjectAction(ProjectAction::SaveMapAs);
            }
            {
                const bool alreadyDefault =
                    ui.currentMapPath().generic_string() ==
                    ui.defaultMapPath().generic_string();
                if (ImGui::MenuItem("Establecer como default", nullptr,
                                      alreadyDefault, !alreadyDefault)) {
                    ui.requestProjectAction(ProjectAction::SetCurrentMapAsDefault);
                }
            }
            {
                const bool canDelete = ui.projectMaps().size() > 1u;
                if (ImGui::MenuItem("Eliminar mapa actual", nullptr,
                                      false, canDelete)) {
                    ui.requestProjectAction(ProjectAction::DeleteCurrentMap);
                }
            }
            // F2H20: compilacion brush -> mesh estatica + export OBJ.
            ImGui::Separator();
            if (ImGui::MenuItem("Compilar mapa (stats)")) {
                ui.requestProjectAction(ProjectAction::CompileMap);
            }
            if (ImGui::MenuItem("Exportar OBJ...")) {
                ui.requestProjectAction(ProjectAction::ExportObj);
            }
            ImGui::EndMenu();
        }

        // F2H18: top-level "Brush". Geometria (primitivas + booleanos).
        // Antes vivia anidada como Archivo > Mapa > {Anadir Brush, Boolean}.
        if (ImGui::BeginMenu(ICON_FA_CUBES_STACKED " Brush", ui.hasProject())) {
            // F2H11 + F2H14: primitivas CSG para el mapa actual.
            if (ImGui::BeginMenu("Anadir")) {
                if (ImGui::MenuItem("Box")) {
                    ui.requestProjectAction(ProjectAction::AddBoxBrush);
                }
                if (ImGui::MenuItem("Cylinder")) {
                    ui.requestProjectAction(ProjectAction::AddCylinderBrush);
                }
                if (ImGui::MenuItem("Sphere (poliedrica)")) {
                    ui.requestProjectAction(ProjectAction::AddSphereBrush);
                }
                if (ImGui::MenuItem("Pyramid")) {
                    ui.requestProjectAction(ProjectAction::AddPyramidBrush);
                }
                if (ImGui::MenuItem("Wedge (rampa)")) {
                    ui.requestProjectAction(ProjectAction::AddWedgeBrush);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Prism Triangular")) {
                    ui.requestProjectAction(ProjectAction::AddPrismTriangularBrush);
                }
                if (ImGui::MenuItem("Prism Hexagonal")) {
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

        if (ImGui::BeginMenu(ICON_FA_PEN_TO_SQUARE " Editar")) {
            // Hito 27: cableado a HistoryStack inyectado por EditorApplication.
            // Hasta que el ctor termine, m_history puede ser nullptr — evitamos
            // crash deshabilitando los items.
            HistoryStack* h = ui.historyStack();
            const bool canUndo = (h != nullptr && h->canUndo());
            const bool canRedo = (h != nullptr && h->canRedo());
            const std::string undoLabel = canUndo
                ? ("Deshacer '" + h->undoName() + "'") : std::string("Deshacer");
            const std::string redoLabel = canRedo
                ? ("Rehacer '" + h->redoName() + "'") : std::string("Rehacer");
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo)) {
                h->undo();
            }
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo)) {
                h->redo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FA_EYE " Ver")) {
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
                        ImGui::TextDisabled("(vacio)");
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Restablecer layout del workspace activo")) {
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

        if (ImGui::BeginMenu(ICON_FA_CIRCLE_QUESTION " Ayuda")) {
            if (ImGui::MenuItem("Acerca de")) {
                m_showAboutPopup = true;
            }
            ImGui::Separator();
            // F2H18: los demos viven en submenu Ayuda > Demos. No son
            // ayuda al usuario, son para validar features rapidamente
            // — pero tampoco merecen top-level (son secundarios al
            // flow de mapping serio). El stress test de poligonos
            // queda dentro del mismo submenu (es un demo mas).
            if (ImGui::BeginMenu("Demos", ui.hasProject())) {
                // Demo Hito 8: spawnea una entidad flotante con ScriptComponent
                // que apunta a assets/scripts/rotator.lua. Util para validar
                // que el ScriptSystem engancha sin tocar nada del mapa.
                if (ImGui::MenuItem("Agregar rotador demo")) {
                    ui.requestSpawnRotator();
                }
                // Demo Hito 20 Bloque 5: spawnea una entidad invisible con
                // ScriptComponent apuntando a hud_demo.lua. En Play Mode el
                // script setea HP=75/Ammo=12 y dren a HP=0 -> auto-pausa.
                if (ImGui::MenuItem("Agregar HUD demo")) {
                    ui.requestSpawnHudDemo();
                }
                // Demo Hito 23 Bloque 3: enemigo NavAgent que persigue al
                // jugador en Play Mode. Mesh = Fox.glb caminando.
                if (ImGui::MenuItem("Agregar enemigo demo")) {
                    ui.requestSpawnEnemyDemo();
                }
                // Demo Hito 9: spawnea una entidad con AudioSourceComponent
                // (beep.wav loop 3D) en una esquina del mapa. En Play Mode,
                // acercarse debería subir el volumen por atenuacion 3D.
                if (ImGui::MenuItem("Agregar audio source demo")) {
                    ui.requestSpawnAudioSource();
                }
                // Demo Hito 11: spawnea una luz puntual blanca encima del centro
                // del mapa. Si no hay luces, la sala se ve casi negra (solo
                // ambient); con esta luz aparecen highlights y caras sombreadas.
                if (ImGui::MenuItem("Agregar luz puntual demo")) {
                    ui.requestSpawnPointLight();
                }
                // Demo Hito 12: caja fisica (rigid body dinamico) que cae por
                // gravedad. En Play Mode la caja se apoya en el suelo y puede
                // ser empujada por el jugador (colisiones AABB vs capsule
                // llegan en hitos posteriores).
                if (ImGui::MenuItem("Agregar caja fisica demo")) {
                    ui.requestSpawnPhysicsBox();
                }
                // Demo Hito 15: entidad "Environment" con EnvironmentComponent
                // default. Una vez creada, se puede editar desde el Inspector
                // (skybox path, fog mode/color/density, exposure, tonemap).
                if (ImGui::MenuItem("Agregar Environment")) {
                    ui.requestSpawnEnvironment();
                }
                // Demo Hito 16: piso plano + columna + sol direccional con
                // castShadows=true. Pensado para ver shadow mapping aislado sin
                // que el mapa de tiles tape la sombra contra los muros.
                if (ImGui::MenuItem("Agregar demo de sombras")) {
                    ui.requestSpawnShadowDemo();
                }
                // Demo Hito 17: 4 esferas con materiales PBR distintos (oro
                // pulido, cobre rugoso, plastico azul, blanco mate). Permite
                // ver el efecto del IBL specular + diffuse en una sola toma.
                if (ImGui::MenuItem("Agregar esferas PBR de prueba")) {
                    ui.requestSpawnPbrSpheres();
                }
                // Demo Hito 18: stress test del Forward+ — 64 point lights
                // (8x8 grid sobre y=2) con colores procedurales. Sin tile
                // culling el shader iterria 64 veces por fragment; con
                // culling solo procesa las que afectan cada tile.
                if (ImGui::MenuItem("Agregar stress test 64 luces")) {
                    ui.requestSpawnLightStress();
                }
                // Demo Hito 19: spawnea el personaje Fox.glb (CC0) con
                // animacion. Se ve en loop tanto en Editor como Play Mode.
                // Cambiar el clip activo desde el Inspector (Animator).
                if (ImGui::MenuItem("Agregar personaje animado")) {
                    ui.requestSpawnAnimatedCharacter();
                }
                // Demo Hito 29: spawnea un emisor de particulas preset
                // "fuego" en (0, 0.5, 0). Visible en Editor y Play.
                if (ImGui::MenuItem("Agregar particulas de fuego demo")) {
                    ui.requestSpawnFireParticles();
                }
                // Demo Hito 33: spawnea un trigger demo (AABB 2x2x2m) con
                // script que loguea enter/exit. Activo solo en Play Mode.
                if (ImGui::MenuItem("Agregar trigger demo")) {
                    ui.requestSpawnTrigger();
                }

                // F2H2: stress tests de poligonos. Spawnean grids 3D de cubos
                // hasta el target de tris (cubo = 12 tris). Para benchmark de
                // FPS, draw calls y identificacion de cuellos de botella.
                ImGui::Separator();
                if (ImGui::BeginMenu("Stress test poligonos")) {
                    if (ImGui::MenuItem("Spawn 10K tris (cubos)")) {
                        ui.requestSpawnStressTris(10000);
                    }
                    if (ImGui::MenuItem("Spawn 100K tris (cubos)")) {
                        ui.requestSpawnStressTris(100000);
                    }
                    if (ImGui::MenuItem("Spawn 500K tris (cubos)")) {
                        ui.requestSpawnStressTris(500000);
                    }
                    if (ImGui::MenuItem("Spawn 1M tris (cubos)")) {
                        ui.requestSpawnStressTris(1000000);
                    }
                    ImGui::EndMenu();
                }

                // F2H42: scene completa para baseline measurement de
                // optimizacion runtime — dispara TODOS los demos juntos
                // (cubos + 64 luces + esferas PBR + sombras + Fox +
                // CesiumMan + particulas + trigger). Click una vez,
                // queda escena lista para profilear con Tracy +
                // Performance panel snapshot CSV.
                ImGui::Separator();
                if (ImGui::MenuItem("Spawn FULL STRESS SCENE (F2H42)")) {
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
                // F2H37: prefijo icon segun el nombre del workspace.
                std::string label;
                label.reserve(ws.name.size() + 8);
                label += iconForWorkspace(ws.name);
                label += ' ';
                label += ws.name;
                if (ImGui::Button(label.c_str())) {
                    if (!isActive) ui.requestWorkspaceSwitch(i);
                }
                ImGui::PopStyleColor(isActive ? 2 : 1);
            }
            ImGui::PopStyleVar();
        }

        // Boton Play/Stop empujado a la derecha de la menu bar.
        const bool isPlay = ui.mode() == EditorMode::Play;
        const char* btnLabel = isPlay ? (ICON_FA_STOP " Stop") : (ICON_FA_PLAY " Play");
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
        ImGui::Text("Version 0.3.0 (Hito 3)");
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
