#include "editor/ui/MenuBar.h"

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "editor/panels/IPanel.h"

#include <imgui.h>

namespace Mood {

void MenuBar::draw(EditorUI& ui, bool& requestQuit) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Archivo")) {
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
            if (ImGui::MenuItem("Guardar como...", nullptr, false, ui.hasProject())) {
                ui.requestProjectAction(ProjectAction::SaveAs);
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

        if (ImGui::BeginMenu("Editar")) {
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

        if (ImGui::BeginMenu("Ver")) {
            for (IPanel* panel : ui.panels()) {
                ImGui::MenuItem(panel->name(), nullptr, &panel->visible);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Restablecer layout")) {
                ui.dockspace().requestResetToDefault();
                Log::editor()->info("Layout restablecido al default");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Ayuda")) {
            if (ImGui::MenuItem("Acerca de")) {
                m_showAboutPopup = true;
            }
            ImGui::Separator();
            // Demo Hito 8: spawnea una entidad flotante con ScriptComponent
            // que apunta a assets/scripts/rotator.lua. Util para validar
            // que el ScriptSystem engancha sin tocar nada del mapa.
            if (ImGui::MenuItem("Agregar rotador demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnRotator();
            }
            // Demo Hito 20 Bloque 5: spawnea una entidad invisible con
            // ScriptComponent apuntando a hud_demo.lua. En Play Mode el
            // script setea HP=75/Ammo=12 y dren a HP=0 -> auto-pausa.
            if (ImGui::MenuItem("Agregar HUD demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnHudDemo();
            }
            // Demo Hito 23 Bloque 3: enemigo NavAgent que persigue al
            // jugador en Play Mode. Mesh = Fox.glb caminando.
            if (ImGui::MenuItem("Agregar enemigo demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnEnemyDemo();
            }
            // Demo Hito 9: spawnea una entidad con AudioSourceComponent
            // (beep.wav loop 3D) en una esquina del mapa. En Play Mode,
            // acercarse debería subir el volumen por atenuacion 3D.
            if (ImGui::MenuItem("Agregar audio source demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnAudioSource();
            }
            // Demo Hito 11: spawnea una luz puntual blanca encima del centro
            // del mapa. Si no hay luces, la sala se ve casi negra (solo
            // ambient); con esta luz aparecen highlights y caras sombreadas.
            if (ImGui::MenuItem("Agregar luz puntual demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnPointLight();
            }
            // Demo Hito 12: caja fisica (rigid body dinamico) que cae por
            // gravedad. En Play Mode la caja se apoya en el suelo y puede
            // ser empujada por el jugador (colisiones AABB vs capsule
            // llegan en hitos posteriores).
            if (ImGui::MenuItem("Agregar caja fisica demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnPhysicsBox();
            }
            // Demo Hito 15: entidad "Environment" con EnvironmentComponent
            // default. Una vez creada, se puede editar desde el Inspector
            // (skybox path, fog mode/color/density, exposure, tonemap).
            if (ImGui::MenuItem("Agregar Environment", nullptr, false, ui.hasProject())) {
                ui.requestSpawnEnvironment();
            }
            // Demo Hito 16: piso plano + columna + sol direccional con
            // castShadows=true. Pensado para ver shadow mapping aislado sin
            // que el mapa de tiles tape la sombra contra los muros.
            if (ImGui::MenuItem("Agregar demo de sombras", nullptr, false, ui.hasProject())) {
                ui.requestSpawnShadowDemo();
            }
            // Demo Hito 17: 4 esferas con materiales PBR distintos (oro
            // pulido, cobre rugoso, plastico azul, blanco mate). Permite
            // ver el efecto del IBL specular + diffuse en una sola toma.
            if (ImGui::MenuItem("Agregar esferas PBR de prueba", nullptr, false, ui.hasProject())) {
                ui.requestSpawnPbrSpheres();
            }
            // Demo Hito 18: stress test del Forward+ — 64 point lights
            // (8x8 grid sobre y=2) con colores procedurales. Sin tile
            // culling el shader iterria 64 veces por fragment; con
            // culling solo procesa las que afectan cada tile.
            if (ImGui::MenuItem("Agregar stress test 64 luces", nullptr, false, ui.hasProject())) {
                ui.requestSpawnLightStress();
            }
            // Demo Hito 19: spawnea el personaje Fox.glb (CC0) con
            // animacion. Se ve en loop tanto en Editor como Play Mode.
            // Cambiar el clip activo desde el Inspector (Animator).
            if (ImGui::MenuItem("Agregar personaje animado", nullptr, false, ui.hasProject())) {
                ui.requestSpawnAnimatedCharacter();
            }
            // Demo Hito 29: spawnea un emisor de particulas preset
            // "fuego" en (0, 0.5, 0). Visible en Editor y Play.
            if (ImGui::MenuItem("Agregar particulas de fuego demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnFireParticles();
            }
            // Demo Hito 33: spawnea un trigger demo (AABB 2x2x2m) con
            // script que loguea enter/exit. Activo solo en Play Mode.
            if (ImGui::MenuItem("Agregar trigger demo", nullptr, false, ui.hasProject())) {
                ui.requestSpawnTrigger();
            }
            ImGui::EndMenu();
        }

        // Boton Play/Stop empujado a la derecha de la menu bar.
        const bool isPlay = ui.mode() == EditorMode::Play;
        const char* btnLabel = isPlay ? "Stop" : "Play";
        const float btnWidth = 64.0f;
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
