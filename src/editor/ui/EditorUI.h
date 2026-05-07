#pragma once

// Coordinador de toda la UI ImGui del editor. Agrega el dockspace, la menu
// bar, los paneles acoplables y la status bar. No hace init/shutdown de
// ImGui ni gestiona input: eso vive en EditorApplication.

#include "editor/ui/Dockspace.h"
#include "editor/application/EditorMode.h"
#include "editor/ui/MenuBar.h"
#include "editor/ui/StatusBar.h"
#include "editor/ui/Toolbar.h"
#include "editor/commands/HistoryStack.h"
#include "editor/panels/assets/AssetBrowserPanel.h"
#include "editor/panels/debug/ConsolePanel.h"
#include "editor/panels/scene/HierarchyPanel.h"
#include "editor/panels/debug/LuaApiPanel.h"
#include "editor/panels/debug/PerformanceHudPanel.h"
#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/assets/MaterialEditorPanel.h"  // Hito 42
#include "editor/panels/assets/ScriptEditorPanel.h"
#include "editor/panels/scene/ViewportPanel.h"
#include "editor/selection/SelectionSet.h"  // F2H13
#include "editor/workspace/WorkspaceManager.h"
#include "engine/scene/core/Entity.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class Scene;

class EditorUI {
public:
    EditorUI();

    /// @brief Dibuja un frame completo de UI. Llamar entre ImGui::NewFrame()
    ///        y ImGui::Render().
    void draw(bool& requestQuit);

    /// @brief Actualiza el FPS mostrado en la status bar.
    void setFps(float fps) { m_statusBar.setFps(fps); }

    /// @brief Mensaje libre de la status bar (ej. "Guardado" tras un save).
    ///        Persiste hasta que se cambie de nuevo.
    void setStatusMessage(std::string msg) { m_statusBar.setMessage(std::move(msg)); }

    /// @brief Lista de paneles, expuesta para que el menu "Ver" pueda togglear
    ///        su visibilidad.
    const std::vector<IPanel*>& panels() const { return m_panels; }

    /// @brief Acceso al panel Viewport para inyectar el framebuffer desde
    ///        EditorApplication y leer el tamano deseado.
    ViewportPanel& viewport() { return m_viewport; }

    /// @brief Acceso al dockspace. MenuBar lo usa para pedir "Restablecer
    ///        layout" cuando el usuario lo elige desde el menu Ver.
    Dockspace& dockspace() { return m_dockspace; }

    /// @brief F2H7: manager de workspaces (Layout/Scripting/Profile/Materials).
    ///        ProjectSerializer lo lee/escribe; la UI dibuja los tabs.
    WorkspaceManager& workspaceManager() { return m_workspaceManager; }
    const WorkspaceManager& workspaceManager() const { return m_workspaceManager; }

    /// @brief F2H7: solicita cambiar de workspace al index pasado. Lo aplica
    ///        `applyPendingWorkspaceSwitch()` ANTES del proximo NewFrame
    ///        (LoadIniSettingsFromMemory no debe llamarse dentro de un frame
    ///        ImGui activo, segun la doc).
    void requestWorkspaceSwitch(int idx) { m_pendingWorkspaceSwitch = idx; }

    /// @brief F2H7: aplica el switch de workspace pendiente. Llamar en
    ///        `EditorApplication::beginFrame` ANTES de `ImGui::NewFrame`.
    ///        No-op si no hay switch pendiente.
    void applyPendingWorkspaceSwitch();

    /// @brief F2H22: setea `visible` de cada panel segun el workspace
    ///        de destino. Solo se aplica al primer activado del
    ///        workspace (cuando `iniLayout` esta vacio) o tras un
    ///        reset manual del layout. Si el dev abre un panel "extra"
    ///        en algun workspace, esa decision persiste en su
    ///        iniLayout custom y este metodo NO la sobreescribe.
    void applyDefaultVisibilityForWorkspace(const std::string& name);

    /// @brief F2H22: la Toolbar lateral pide al EditorApplication que
    ///        cambie el gizmo mode (0 = Translate, 1 = Rotate, 2 = Scale).
    ///        EditorApplication::run() consume cada frame y aplica al
    ///        `m_gizmoMode` interno. -1 = sin request.
    void requestGizmoMode(int mode) { m_gizmoModeRequested = mode; }
    int consumeGizmoModeRequest() {
        const int r = m_gizmoModeRequested;
        m_gizmoModeRequested = -1;
        return r;
    }

    /// @brief F2H22: la Toolbar pide togglear Face/Object sub-mode.
    ///        EditorApplication consume y flippa `m_subMode`.
    void requestToggleFaceMode() { m_toggleFaceModeRequested = true; }
    bool consumeToggleFaceModeRequest() {
        const bool r = m_toggleFaceModeRequested;
        m_toggleFaceModeRequested = false;
        return r;
    }

    /// @brief Acceso al panel Asset Browser para inyectarle el AssetManager
    ///        desde EditorApplication y leer la seleccion actual.
    AssetBrowserPanel& assetBrowser() { return m_assetBrowser; }

    /// @brief Acceso al panel Hierarchy para inyectarle el Scene desde
    ///        EditorApplication.
    HierarchyPanel& hierarchy() { return m_hierarchy; }

    /// @brief Acceso al panel Inspector (se le inyecta el Scene para
    ///        conocer la entidad seleccionada y editar sus componentes).
    InspectorPanel& inspector() { return m_inspector; }

    /// @brief Hito 42: panel dedicado de edicion de materiales (drop
    ///        textures + sliders sin necesidad de tener entidad seleccionada).
    MaterialEditorPanel& materialEditor() { return m_materialEditor; }

    /// @brief F2H2: panel del Performance HUD (FPS / frame ms / draw calls /
    ///        triangulos / entity count). EditorApplication le pasa las
    ///        metricas cada frame con `setMetrics`.
    PerformanceHudPanel& performanceHud() { return m_performanceHud; }

    /// @brief Hito 27: HistoryStack del editor inyectado desde
    ///        EditorApplication. La MenuBar lo usa para los items
    ///        "Editar > Deshacer / Rehacer" (canUndo/canRedo + name del
    ///        comando activo). nullptr antes de la inicializacion.
    void setHistoryStack(HistoryStack* h) { m_history = h; }
    HistoryStack* historyStack() const { return m_history; }

    /// @brief F2H17: sub-modo del Editor (Object / Face). Sincronizado
    ///        por EditorApplication cada frame para que StatusBar y
    ///        InspectorPanel sepan el estado.
    void setSubMode(EditorSubMode m) { m_subMode = m; }
    EditorSubMode subMode() const { return m_subMode; }

    /// @brief Entidad seleccionada por el panel Hierarchy (compartida con
    ///        el Inspector). En F2H13 esto devuelve la `active` del
    ///        SelectionSet — la mayoria de los callsites del editor
    ///        operan sobre la active sin saber que hay multi-seleccion.
    ///        Entity default-constructed = sin seleccion.
    Entity selectedEntity() const { return m_selectionSet.active; }

    /// @brief F2H13: reemplaza el set por una sola entidad. Patron
    ///        "click plain" (sin Shift / Ctrl). Mantiene el contrato
    ///        de la API previa para callsites que solo conocen
    ///        seleccion singular.
    void setSelectedEntity(Entity e) {
        replaceWithSingle(m_selectionSet, e);
    }

    /// @brief F2H13: acceso al set completo. HierarchyPanel y
    ///        Viewport picking usan estos getters para aplicar
    ///        Shift/Ctrl semantics; SelectionOverlay para el
    ///        outline; drawBooleanOpMenu para iterar los brushes
    ///        seleccionados como A's en cascade.
    SelectionSet& selectionSet() { return m_selectionSet; }
    const SelectionSet& selectionSet() const { return m_selectionSet; }

    /// @brief F2H12: pointer non-owning al scene activo. Necesario
    ///        para que MenuBar pueda iterar brushes (drawBooleanOpMenu).
    ///        EditorApplication lo setea una vez al crear/cambiar
    ///        el scene.
    void setScene(Scene* s) { m_scene = s; }
    Scene* scene() const { return m_scene; }

    /// @brief F2H12: dibuja el submenu "Boolean" dentro de
    ///        "Archivo > Mapa". Se llama desde MenuBar::draw. Vive
    ///        aqui porque accede a m_scene + m_selectedEntity y
    ///        despacha via requestBooleanOp(). Externo a MenuBar
    ///        para no acoplar MenuBar al Scene.
    void drawBooleanOpMenu();

    /// @brief Demo del Hito 8: request para crear una entidad con
    ///        ScriptComponent apuntando a `scripts/rotator.lua`.
    ///        `EditorApplication` la consume despues de `draw()`.
    void requestSpawnRotator() { m_spawnRotatorRequested = true; }
    bool consumeSpawnRotatorRequest() {
        const bool r = m_spawnRotatorRequested;
        m_spawnRotatorRequested = false;
        return r;
    }

    /// @brief Hito 23 Bloque 3: spawnea un enemigo demo con
    ///        NavAgentComponent que persigue al jugador en Play Mode.
    void requestSpawnEnemyDemo() { m_spawnEnemyDemoRequested = true; }
    bool consumeSpawnEnemyDemoRequest() {
        const bool r = m_spawnEnemyDemoRequested;
        m_spawnEnemyDemoRequested = false;
        return r;
    }

    /// @brief Demo del Hito 20 Bloque 5: request para crear una entidad
    ///        con ScriptComponent apuntando a `scripts/hud_demo.lua`,
    ///        que ejercita la tabla `hud` (setHp/setAmmo/setPaused).
    void requestSpawnHudDemo() { m_spawnHudDemoRequested = true; }
    bool consumeSpawnHudDemoRequest() {
        const bool r = m_spawnHudDemoRequested;
        m_spawnHudDemoRequested = false;
        return r;
    }

    /// @brief Demo del Hito 9: request para crear una entidad con
    ///        AudioSourceComponent (beep.wav loop + 3D) en una esquina del
    ///        mapa. `EditorApplication` la consume despues de `draw()`.
    void requestSpawnAudioSource() { m_spawnAudioSourceRequested = true; }
    bool consumeSpawnAudioSourceRequest() {
        const bool r = m_spawnAudioSourceRequested;
        m_spawnAudioSourceRequested = false;
        return r;
    }

    /// @brief Demo del Hito 11: request para crear una entidad con
    ///        LightComponent (Point, blanco, intensidad 1, radius 12)
    ///        suspendida sobre el centro del mapa.
    void requestSpawnPointLight() { m_spawnPointLightRequested = true; }
    bool consumeSpawnPointLightRequest() {
        const bool r = m_spawnPointLightRequested;
        m_spawnPointLightRequested = false;
        return r;
    }

    /// @brief Demo del Hito 12: request para spawnear una caja fisica
    ///        (Dynamic, Box, 1 m x 1 m x 1 m, 5 kg) a 6 m de altura. Cae
    ///        por gravedad hasta apoyarse en el suelo / tiles solidos.
    void requestSpawnPhysicsBox() { m_spawnPhysicsBoxRequested = true; }
    bool consumeSpawnPhysicsBoxRequest() {
        const bool r = m_spawnPhysicsBoxRequested;
        m_spawnPhysicsBoxRequested = false;
        return r;
    }

    /// @brief Hito 14: request para abrir el dialogo "Guardar como prefab"
    ///        sobre la entidad seleccionada en el Hierarchy.
    ///        EditorApplication lo consume y dispara `pfd::save_file` con
    ///        filtro `*.moodprefab`.
    void requestSavePrefabDialog() { m_savePrefabRequested = true; }
    bool consumeSavePrefabRequest() {
        const bool r = m_savePrefabRequested;
        m_savePrefabRequested = false;
        return r;
    }

    /// @brief Hito 15: request para crear una entidad "Environment" con
    ///        un `EnvironmentComponent` default. Util para empezar a editar
    ///        sky/fog/post-process desde el Inspector.
    void requestSpawnEnvironment() { m_spawnEnvironmentRequested = true; }
    bool consumeSpawnEnvironmentRequest() {
        const bool r = m_spawnEnvironmentRequested;
        m_spawnEnvironmentRequested = false;
        return r;
    }

    /// @brief Hito 16: request para spawnear un mini-set de demo de sombras
    ///        (piso plano + columna + luz direccional con castShadows=true)
    ///        que permita ver el resultado del shadow mapping de forma clara
    ///        sin tener que armar manualmente la escena.
    void requestSpawnShadowDemo() { m_spawnShadowDemoRequested = true; }
    bool consumeSpawnShadowDemoRequest() {
        const bool r = m_spawnShadowDemoRequested;
        m_spawnShadowDemoRequested = false;
        return r;
    }

    /// @brief Hito 17 Bloque 3: request para spawnear 4 esferas con
    ///        materiales PBR distintos (oro pulido, cobre rugoso, plastico
    ///        azul, blanco mate). Showcase visual del shader PBR + IBL.
    void requestSpawnPbrSpheres() { m_spawnPbrSpheresRequested = true; }
    bool consumeSpawnPbrSpheresRequest() {
        const bool r = m_spawnPbrSpheresRequested;
        m_spawnPbrSpheresRequested = false;
        return r;
    }

    /// @brief Hito 18: request para spawnear 64 point lights de prueba
    ///        en grid 8x8 sobre el plano del demo de sombras. Stress test
    ///        del Forward+ — sin tile culling el shader iterria 64 veces
    ///        por fragment, con culling solo las que realmente afectan
    ///        cada tile (~3-8 tipicas).
    void requestSpawnLightStress() { m_spawnLightStressRequested = true; }
    bool consumeSpawnLightStressRequest() {
        const bool r = m_spawnLightStressRequested;
        m_spawnLightStressRequested = false;
        return r;
    }

    /// @brief Hito 19: request para spawnear `assets/meshes/Fox.glb`
    ///        (CC0, glTF Sample Assets) con AnimatorComponent + clip
    ///        default ("Survey"/"Walk"/"Run" segun viene en el archivo).
    ///        Se ve animado tanto en Editor como en Play Mode (el sistema
    ///        no condiciona por modo).
    void requestSpawnAnimatedCharacter() { m_spawnAnimatedCharacterRequested = true; }
    bool consumeSpawnAnimatedCharacterRequest() {
        const bool r = m_spawnAnimatedCharacterRequested;
        m_spawnAnimatedCharacterRequested = false;
        return r;
    }

    /// @brief Hito 29 Bloque 3: request para spawnear un emisor de
    ///        partículas preset "fuego" (rate=60, lifetime=1-1.5s, color
    ///        naranja->rojo transparente, blend aditivo, textura
    ///        `particle_fire.png`). Se ve en Editor y Play Mode.
    void requestSpawnFireParticles() { m_spawnFireParticlesRequested = true; }
    bool consumeSpawnFireParticlesRequest() {
        const bool r = m_spawnFireParticlesRequested;
        m_spawnFireParticlesRequested = false;
        return r;
    }

    /// @brief Hito 33 Bloque 4: request para spawnear una entidad demo con
    ///        TriggerComponent (AABB 2x2x2m) y un script default que
    ///        loguea on_trigger_enter / on_trigger_exit. Visible en
    ///        Editor (sin colision solida) + activo en Play Mode.
    void requestSpawnTrigger() { m_spawnTriggerRequested = true; }
    bool consumeSpawnTriggerRequest() {
        const bool r = m_spawnTriggerRequested;
        m_spawnTriggerRequested = false;
        return r;
    }

    /// @brief F2H2: request para spawnear un grid 3D de cubos hasta el
    ///        target de triangulos indicado (cubo = 12 tris). Valores
    ///        validos: 10000, 100000, 500000, 1000000. 0 = sin request.
    ///        EditorApplication consume con `consumeSpawnStressTrisRequest`
    ///        y crea las entidades.
    void requestSpawnStressTris(int targetTris) {
        m_spawnStressTrisRequested = targetTris;
    }
    int consumeSpawnStressTrisRequest() {
        const int n = m_spawnStressTrisRequested;
        m_spawnStressTrisRequested = 0;
        return n;
    }

    /// @brief Modo actual del editor. EditorApplication es quien lo cambia;
    ///        la UI lo consulta para mostrarlo en la status bar y el label
    ///        del boton Play/Stop en la menu bar.
    EditorMode mode() const { return m_mode; }
    void setMode(EditorMode m) { m_mode = m; }

    /// @brief Solicitud de toggle del modo emitida por la UI (boton Play).
    ///        EditorApplication la consume con `consumeTogglePlayRequest`
    ///        luego de `draw()`.
    void requestTogglePlay() { m_togglePlayRequested = true; }
    bool consumeTogglePlayRequest() {
        const bool r = m_togglePlayRequested;
        m_togglePlayRequested = false;
        return r;
    }

    /// @brief Accion de proyecto solicitada desde el menu (Nuevo, Abrir,
    ///        Guardar, etc.). Solo puede haber una por frame.
    void requestProjectAction(ProjectAction a) { m_projectAction = a; }
    ProjectAction consumeProjectAction() {
        const auto a = m_projectAction;
        m_projectAction = ProjectAction::None;
        return a;
    }

    /// @brief Estado actual del proyecto (lo setea EditorApplication). La UI
    ///        lo usa para grayar items del menu cuando no hay proyecto
    ///        abierto y para decidir si mostrar el modal Welcome.
    bool hasProject() const { return m_hasProject; }
    void setHasProject(bool v) { m_hasProject = v; }

    /// @brief Lista de proyectos recientes (mostrada en el modal Welcome).
    ///        `path.stem()` se usa como label del boton.
    void setRecentProjects(std::vector<std::filesystem::path> paths) {
        m_recentProjects = std::move(paths);
    }

    /// @brief Quita un path de la lista de recientes (boton "X" del modal).
    ///        Marca dirty para que EditorApplication persista al state file.
    void eraseRecent(const std::filesystem::path& path);

    /// @brief Quita todos los recientes cuyo archivo ya no existe en disco
    ///        (boton "Limpiar inexistentes"). Marca dirty.
    void pruneMissingRecents();

    /// @brief `true` una unica vez tras editar la lista de recientes.
    ///        EditorApplication consume y llama a `saveEditorState`.
    bool consumeRecentsDirty() {
        const bool r = m_recentsDirty;
        m_recentsDirty = false;
        return r;
    }

    /// @brief Acceso de lectura para que `EditorApplication::saveEditorState`
    ///        sirva la lista actualizada.
    const std::vector<std::filesystem::path>& recentProjects() const {
        return m_recentProjects;
    }

    /// @brief Si el modal Welcome solicito abrir un proyecto especifico
    ///        (click en un reciente), el path queda aca. `EditorApplication`
    ///        lo consume despues de `draw()`.
    std::optional<std::filesystem::path> consumeOpenProjectPath() {
        auto p = std::move(m_openProjectPath);
        m_openProjectPath.reset();
        return p;
    }

    /// @brief F2H8: el menu "Archivo > Mapa > Abrir mapa" pide abrir un
    ///        mapa especifico del proyecto. EditorApplication lo consume.
    void requestOpenMap(std::filesystem::path p) { m_openMapRequest = std::move(p); }
    std::optional<std::filesystem::path> consumeOpenMapRequest() {
        auto p = std::move(m_openMapRequest);
        m_openMapRequest.reset();
        return p;
    }

    /// @brief F2H13: request de operacion booleana en cascada.
    ///        El callsite (drawBooleanOpMenu) NO pasa B explicito —
    ///        EditorApplication lee el SelectionSet completo y aplica
    ///        la op contra cada selected != active, usando active
    ///        como "tool brush" B.
    ///
    ///        Reemplaza el flujo de F2H12 que pedia brushB en submenu
    ///        cascading (requeria seleccion singular + combobox de B).
    enum class BooleanOpRequestKind { Subtract, Union, Intersect };
    void requestBooleanOp(BooleanOpRequestKind kind) {
        m_booleanOpRequest = kind;
    }
    std::optional<BooleanOpRequestKind> consumeBooleanOpRequest() {
        auto r = std::move(m_booleanOpRequest);
        m_booleanOpRequest.reset();
        return r;
    }

    /// @brief F2H8: snapshot de la info de mapas del proyecto para que
    ///        MenuBar pueda dibujar el submenu sin acoplarse al `Project`
    ///        directamente. EditorApplication lo sincroniza tras cada
    ///        operacion de mapas (open project, NewMap, SaveMapAs, etc).
    void setProjectMapsSnapshot(std::vector<std::filesystem::path> maps,
                                  std::filesystem::path currentMap,
                                  std::filesystem::path defaultMap) {
        m_projectMaps = std::move(maps);
        m_currentMapPath = std::move(currentMap);
        m_defaultMapPath = std::move(defaultMap);
    }
    const std::vector<std::filesystem::path>& projectMaps() const {
        return m_projectMaps;
    }
    const std::filesystem::path& currentMapPath() const { return m_currentMapPath; }
    const std::filesystem::path& defaultMapPath() const { return m_defaultMapPath; }

private:
    Dockspace m_dockspace;
    MenuBar m_menuBar;
    StatusBar m_statusBar;

    ViewportPanel m_viewport;
    HierarchyPanel m_hierarchy;
    InspectorPanel m_inspector;
    AssetBrowserPanel m_assetBrowser;
    ConsolePanel m_console;
    LuaApiPanel m_luaApi;
    PerformanceHudPanel m_performanceHud;  // F2H2
    ScriptEditorPanel m_scriptEditor;  // Hito 28 F
    MaterialEditorPanel m_materialEditor;  // Hito 42
    Toolbar m_toolbar;  // F2H22: tools de edicion (gizmo modes + brushes + face)
    SelectionSet m_selectionSet;  // F2H13: reemplaza m_selectedEntity
    Scene* m_scene = nullptr;  // F2H12: non-owning, set by EditorApplication

    // Hito 27: punter inyectado por EditorApplication para que MenuBar
    // pueda dibujar los items Deshacer/Rehacer con el estado correcto.
    HistoryStack* m_history = nullptr;

    std::vector<IPanel*> m_panels;

    EditorMode m_mode = EditorMode::Editor;
    EditorSubMode m_subMode = EditorSubMode::Object;  // F2H17
    bool m_togglePlayRequested = false;

    ProjectAction m_projectAction = ProjectAction::None;
    bool m_hasProject = false;
    bool m_spawnRotatorRequested = false;
    bool m_spawnHudDemoRequested = false;
    bool m_spawnEnemyDemoRequested = false;
    bool m_spawnAudioSourceRequested = false;
    bool m_spawnPointLightRequested = false;
    bool m_spawnPhysicsBoxRequested = false;
    bool m_savePrefabRequested = false;
    bool m_spawnEnvironmentRequested = false;
    bool m_spawnShadowDemoRequested = false; // Hito 16
    bool m_spawnPbrSpheresRequested = false; // Hito 17
    bool m_spawnLightStressRequested = false; // Hito 18
    bool m_spawnAnimatedCharacterRequested = false; // Hito 19
    bool m_spawnFireParticlesRequested = false;     // Hito 29
    bool m_spawnTriggerRequested = false;           // Hito 33
    int  m_spawnStressTrisRequested = 0;             // F2H2 (target tris)
    bool m_recentsDirty = false; // Hito 15 polish: edicion manual de la lista de recientes
    std::vector<std::filesystem::path> m_recentProjects;
    std::optional<std::filesystem::path> m_openProjectPath;
    std::optional<std::filesystem::path> m_openMapRequest;  // F2H8
    std::optional<BooleanOpRequestKind> m_booleanOpRequest; // F2H13 (kind only)
    // F2H8: snapshot de la info de mapas del proyecto.
    std::vector<std::filesystem::path> m_projectMaps;
    std::filesystem::path m_currentMapPath;
    std::filesystem::path m_defaultMapPath;

    /// @brief Dibuja un modal bloqueante con [Nuevo Proyecto] [Abrir Proyecto]
    ///        + lista de recientes. Se ejecuta SOLO cuando `m_hasProject` es
    ///        false. Bloquea toda interaccion con el editor hasta que el
    ///        usuario elija.
    void drawWelcomeModal();

    // F2H7: workspaces.
    WorkspaceManager m_workspaceManager;
    /// `-1` = no hay switch pendiente. Otra valor = index objetivo, lo aplica
    /// `applyPendingWorkspaceSwitch()` antes de `NewFrame`.
    int m_pendingWorkspaceSwitch{-1};

    /// F2H22: requests del Toolbar lateral. EditorApplication consume
    /// cada frame en `run()`.
    int  m_gizmoModeRequested = -1;     // -1 = sin request, 0/1/2 = T/R/S
    bool m_toggleFaceModeRequested = false;
};

} // namespace Mood
