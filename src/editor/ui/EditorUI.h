#pragma once

// Coordinador de toda la UI ImGui del editor. Agrega el dockspace, la menu
// bar, los paneles acoplables y la status bar. No hace init/shutdown de
// ImGui ni gestiona input: eso vive en EditorApplication.
//
// AUDIT-3: los inline bodies de los pares request/consume viven en
// archivos `EditorUI_*.inl` sibling (Spawn / Tools / Entity / Project).
// La declaracion sigue aca; el body fuera para mantener el header bajo
// el SOFT cap de 500 LOC sin pImpl invasivo.

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
#include "editor/panels/debug/NodeGraphSandboxPanel.h"  // F2H46
#include "editor/panels/debug/PerformanceHudPanel.h"
#include "editor/panels/narrative/NarrativeIntroPanel.h"  // F2H46
#include "editor/panels/narrative/DialogBrowserPanel.h"    // F2H47
#include "editor/panels/narrative/DialogEditorPanel.h"     // F2H47
#include "editor/panels/narrative/DialogNodeInspectorPanel.h"  // F2H47
#include "editor/panels/inventory/ItemBrowserPanel.h"          // F2H51
#include "editor/panels/inventory/ItemPropertyEditorPanel.h"   // F2H51
#include "editor/panels/quest/QuestBrowserPanel.h"             // F2H53
#include "editor/panels/quest/QuestPropertyEditorPanel.h"      // F2H53
#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/assets/MaterialEditorPanel.h"  // Hito 42
#include "editor/panels/assets/ScriptEditorPanel.h"
#include "editor/panels/assets/ShaderGraphEditorPanel.h"  // F2H62 Bloque C
#include "editor/panels/scene/OrthoViewportPanel.h"  // F2H28
#include "editor/panels/scene/ViewportPanel.h"
#include "editor/panels/scene/VisGroupsPanel.h"  // F2H33
#include "editor/ui/MapEditorTopBar.h"  // F2H30 Bloque C
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
    void setStatusMessage(std::string msg) { m_statusBar.setMessage(std::move(msg)); }

    /// @brief Lista de paneles, expuesta para que el menu "Ver" pueda togglear
    ///        su visibilidad.
    const std::vector<IPanel*>& panels() const { return m_panels; }

    // ============================================================
    // Panel accessors — inyeccion de scene/assets/history desde
    // EditorApplication. Todos non-owning.
    // ============================================================

    ViewportPanel& viewport() { return m_viewport; }
    OrthoViewportPanel& orthoTop()   { return m_orthoTop; }   // F2H28
    OrthoViewportPanel& orthoFront() { return m_orthoFront; } // F2H28
    OrthoViewportPanel& orthoSide()  { return m_orthoSide; }  // F2H28
    Dockspace& dockspace() { return m_dockspace; }
    MapEditorTopBar& mapEditorTopBar() { return m_mapEditorTopBar; }      // F2H30
    VisGroupsPanel& visGroupsPanel()   { return m_visGroupsPanel; }       // F2H33
    AssetBrowserPanel& assetBrowser()  { return m_assetBrowser; }
    HierarchyPanel& hierarchy()        { return m_hierarchy; }
    InspectorPanel& inspector()        { return m_inspector; }
    MaterialEditorPanel& materialEditor()    { return m_materialEditor; }     // Hito 42
    ShaderGraphEditorPanel& shaderGraphEditor() { return m_shaderGraphEditor; }// F2H62
    PerformanceHudPanel& performanceHud()    { return m_performanceHud; }     // F2H2
    DialogEditorPanel&         dialogEditor()    { return m_dialogEditor; }    // F2H47
    DialogBrowserPanel&        dialogBrowser()   { return m_dialogBrowser; }   // F2H47
    DialogNodeInspectorPanel&  dialogInspector() { return m_dialogInspector; } // F2H47
    ItemBrowserPanel&          itemBrowser()         { return m_itemBrowser; }         // F2H51
    ItemPropertyEditorPanel&   itemPropertyEditor()  { return m_itemPropertyEditor; }  // F2H51
    QuestBrowserPanel&         questBrowser()        { return m_questBrowser; }        // F2H53
    QuestPropertyEditorPanel&  questPropertyEditor() { return m_questPropertyEditor; } // F2H53

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

    // ============================================================
    // Tool / mode state — lectura via getters, escritura via
    // EditorApplication tras consumir el request correspondiente.
    // Bodies de los request/consume en EditorUI_Tools.inl.
    // ============================================================

    /// @brief F2H22: gizmo mode (0=Translate, 1=Rotate, 2=Scale).
    void requestGizmoMode(int mode);
    int  consumeGizmoModeRequest();

    /// @brief F2H22: toggle Face/Object sub-mode.
    void requestToggleFaceMode();
    bool consumeToggleFaceModeRequest();

    /// @brief F2H30 Bloque C: sub-mode (Object/Vertex/Edge/Face).
    void requestSubMode(EditorSubMode mode);
    bool consumeSubModeRequest(EditorSubMode& outMode);

    /// @brief F2H30 Bloque C: toggle del pincel poligonal.
    void requestTogglePolygonDraw();
    bool consumeTogglePolygonDrawRequest();
    void setPolygonDrawActive(bool v) { m_polygonDrawActive = v; }
    bool polygonDrawActive() const { return m_polygonDrawActive; }

    /// @brief F2H31 Bloque B: MapTool (Select/CreateBlock/Pincel).
    void requestMapTool(MapTool tool);
    bool consumeMapToolRequest(MapTool& outTool);
    void setMapTool(MapTool t) { m_mapTool = t; }
    MapTool mapTool() const { return m_mapTool; }

    /// @brief F2H31 Bloque C: snap-to-vertex (tambien tecla V).
    void requestToggleSnapToVertex();
    bool consumeToggleSnapToVertexRequest();
    void setSnapToVertexEnabled(bool v) { m_snapToVertexEnabled = v; }
    bool snapToVertexEnabled() const { return m_snapToVertexEnabled; }

    /// @brief F2H35 Bloque E: labels arriba de point entities.
    void setShowEntityLabels(bool v) { m_showEntityLabels = v; }
    bool showEntityLabels() const { return m_showEntityLabels; }
    void requestToggleEntityLabels();
    bool consumeToggleEntityLabelsRequest();

    /// @brief F2H32 Bloque C: carve (Hammer-style boolean subtract).
    void requestCarve();
    bool consumeCarveRequest();

    // ============================================================
    // State setters / queries (Scene + History + Selection + Mode).
    // ============================================================

    /// @brief Hito 27: HistoryStack inyectado por EditorApplication.
    void setHistoryStack(HistoryStack* h) {
        m_history = h;
        m_visGroupsPanel.setHistoryStack(h);  // F2H33
    }
    HistoryStack* historyStack() const { return m_history; }

    /// @brief F2H17: sub-modo (Object / Face). Sincronizado cada frame
    ///        para que StatusBar e InspectorPanel sepan el estado.
    void setSubMode(EditorSubMode m) { m_subMode = m; }
    EditorSubMode subMode() const { return m_subMode; }

    /// @brief Entidad seleccionada (`active` del SelectionSet en F2H13).
    Entity selectedEntity() const { return m_selectionSet.active; }
    void setSelectedEntity(Entity e) { replaceWithSingle(m_selectionSet, e); }

    /// @brief F2H13: acceso al set de seleccion completo.
    SelectionSet& selectionSet() { return m_selectionSet; }
    const SelectionSet& selectionSet() const { return m_selectionSet; }

    /// @brief F2H12: scene activo (non-owning).
    void setScene(Scene* s) {
        m_scene = s;
        m_visGroupsPanel.setScene(s);  // F2H33
    }
    Scene* scene() const { return m_scene; }

    /// @brief F2H12: dibuja submenu "Boolean" desde MenuBar::draw.
    void drawBooleanOpMenu();

    /// @brief Modo actual del editor (Editor / PlayInEditor).
    EditorMode mode() const { return m_mode; }
    void setMode(EditorMode m) { m_mode = m; }

    /// @brief F2H13: request de operacion booleana en cascada.
    enum class BooleanOpRequestKind { Subtract, Union, Intersect };
    void requestBooleanOp(BooleanOpRequestKind kind);
    std::optional<BooleanOpRequestKind> consumeBooleanOpRequest();

    // ============================================================
    // Entity create / convert / delete (bodies en EditorUI_Entity.inl).
    // ============================================================

    /// @brief Hito 14: dialogo "Guardar como prefab".
    void requestSavePrefabDialog();
    bool consumeSavePrefabRequest();

    /// @brief F2H57: workflow estilo Hammer — file picker + spawn.
    void requestCreateEntityFromModel();
    bool consumeCreateEntityFromModelRequest();

    /// @brief F2H57 followup: spawn placeholder cube (sin file picker).
    void requestCreateEntityPlaceholder();
    bool consumeCreateEntityPlaceholderRequest();

    /// @brief F2H57 followup: modal SFM-style con meshes ya cargados.
    void requestPickFromLoadedMeshes();
    bool consumePickFromLoadedMeshesRequest();

    /// @brief F2H57 Bloque C: modal "Convertir entidad" (kits NPC/Item/Luz).
    void requestEntityConvertModal(entt::entity e);
    bool consumeEntityConvertModalRequest(entt::entity& outTarget);

    /// @brief F2H57 Bloque C: delete entidad activa (= tecla Delete).
    void requestDeleteSelectedEntity();
    bool consumeDeleteSelectedRequest();

    // ============================================================
    // Spawn demos (Hito 8-19, F2H42-F2H53). Bodies en EditorUI_Spawn.inl.
    // ============================================================

    /// @brief Hito 8 — ScriptComponent(`scripts/rotator.lua`).
    void requestSpawnRotator();
    bool consumeSpawnRotatorRequest();

    /// @brief Hito 23 Bloque 3 — enemigo demo con NavAgent.
    void requestSpawnEnemyDemo();
    bool consumeSpawnEnemyDemoRequest();

    /// @brief Hito 20 Bloque 5 — `scripts/hud_demo.lua`.
    void requestSpawnHudDemo();
    bool consumeSpawnHudDemoRequest();

    /// @brief Hito 9 — AudioSource (beep.wav loop + 3D).
    void requestSpawnAudioSource();
    bool consumeSpawnAudioSourceRequest();

    /// @brief Hito 11 — Point light (blanca, radius 12).
    void requestSpawnPointLight();
    bool consumeSpawnPointLightRequest();

    /// @brief Hito 12 — caja fisica Dynamic 1x1x1 5kg desde 6m.
    void requestSpawnPhysicsBox();
    bool consumeSpawnPhysicsBoxRequest();

    /// @brief Hito 15 — entidad "Environment" con sky/fog/post-process default.
    void requestSpawnEnvironment();
    bool consumeSpawnEnvironmentRequest();

    /// @brief Hito 16 — piso + columna + dir light castShadows=true.
    void requestSpawnShadowDemo();
    bool consumeSpawnShadowDemoRequest();

    /// @brief Hito 17 Bloque 3 — 4 esferas PBR (gold/copper/plastic/matte).
    void requestSpawnPbrSpheres();
    bool consumeSpawnPbrSpheresRequest();

    /// @brief Hito 18 — 64 point lights en grid 8x8 (stress Forward+).
    void requestSpawnLightStress();
    bool consumeSpawnLightStressRequest();

    /// @brief Hito 19 — Fox.glb con Animator (Survey/Walk/Run).
    void requestSpawnAnimatedCharacter();
    bool consumeSpawnAnimatedCharacterRequest();

    /// @brief Hito 29 Bloque 3 — emisor partículas "fuego".
    void requestSpawnFireParticles();
    bool consumeSpawnFireParticlesRequest();

    /// @brief Hito 33 Bloque 4 — TriggerComponent (AABB 2x2x2) + script demo.
    void requestSpawnTrigger();
    bool consumeSpawnTriggerRequest();

    /// @brief F2H47 — .mooddialog demo (3 nodos + 2 choices).
    void requestSpawnDialogDemo();
    bool consumeSpawnDialogDemoRequest();

    /// @brief F2H50 Bloque A — narrative_demo.moodmap (NPC + Animator + Dialog + Trigger).
    void requestGenerateNarrativeDemoMap();
    bool consumeGenerateNarrativeDemoMapRequest();

    /// @brief F2H42 — escena stress completa (cubos + luces + esferas + sombras + chars + particles + trigger).
    void requestSpawnFullStressScene();
    bool consumeSpawnFullStressSceneRequest();

    /// @brief F2H2 — grid de cubos hasta el target (10k / 100k / 500k / 1M tris).
    void requestSpawnStressTris(int targetTris);
    int  consumeSpawnStressTrisRequest();

    /// @brief F2H44 — Welcome modal "demo onboarding" (NewProject + spawn animated char).
    void requestOpenDemoMap();
    bool consumeOpenDemoMapRequest();

    /// @brief F2H50 Bloque C — Welcome modal narrative demo (NPC Y Bot + Dialog + Trigger).
    void requestOpenNarrativeDemo();
    bool consumeOpenNarrativeDemoRequest();

    // ============================================================
    // Project state / recents / maps (bodies en EditorUI_Project.inl).
    // ============================================================

    /// @brief Solicitud de toggle Play/Stop (boton de la menu bar).
    void requestTogglePlay();
    bool consumeTogglePlayRequest();

    /// @brief Project action solicitada desde el menu (Nuevo, Abrir, etc).
    void requestProjectAction(ProjectAction a);
    ProjectAction consumeProjectAction();

    /// @brief Estado actual del proyecto. La UI lo usa para grayar items
    ///        cuando no hay proyecto y para decidir mostrar Welcome modal.
    bool hasProject() const { return m_hasProject; }
    void setHasProject(bool v) { m_hasProject = v; }

    /// @brief Lista de proyectos recientes (mostrada en Welcome modal).
    void setRecentProjects(std::vector<std::filesystem::path> paths);
    void eraseRecent(const std::filesystem::path& path);
    void pruneMissingRecents();
    bool consumeRecentsDirty();
    const std::vector<std::filesystem::path>& recentProjects() const {
        return m_recentProjects;
    }
    std::optional<std::filesystem::path> consumeOpenProjectPath();

    /// @brief F2H8: open map request del menu "Archivo > Mapa > Abrir".
    void requestOpenMap(std::filesystem::path p);
    std::optional<std::filesystem::path> consumeOpenMapRequest();

    /// @brief F2H8: snapshot info mapas del proyecto (para que MenuBar
    ///        dibuje el submenu sin acoplarse al `Project`).
    void setProjectMapsSnapshot(std::vector<std::filesystem::path> maps,
                                  std::filesystem::path currentMap,
                                  std::filesystem::path defaultMap);
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
    NodeGraphSandboxPanel m_nodeGraphSandbox;  // F2H46
    NarrativeIntroPanel       m_narrativeIntro;    // F2H46
    DialogBrowserPanel        m_dialogBrowser;     // F2H47
    DialogEditorPanel         m_dialogEditor;      // F2H47
    DialogNodeInspectorPanel  m_dialogInspector;   // F2H47
    ItemBrowserPanel          m_itemBrowser;       // F2H51
    ItemPropertyEditorPanel   m_itemPropertyEditor;// F2H51
    QuestBrowserPanel         m_questBrowser;        // F2H53
    QuestPropertyEditorPanel  m_questPropertyEditor; // F2H53
    ScriptEditorPanel m_scriptEditor;  // Hito 28 F
    MaterialEditorPanel m_materialEditor;  // Hito 42
    ShaderGraphEditorPanel m_shaderGraphEditor;  // F2H62 Bloque C
    Toolbar m_toolbar;  // F2H22
    MapEditorTopBar m_mapEditorTopBar;  // F2H30
    VisGroupsPanel m_visGroupsPanel;  // F2H33
    // F2H28: 3 paneles orto. Inician invisibles; los hace visibles
    // applyDefaultVisibilityForWorkspace al activar el workspace de mapas.
    OrthoViewportPanel m_orthoTop  {OrthoView::TopXZ};
    OrthoViewportPanel m_orthoFront{OrthoView::FrontXY};
    OrthoViewportPanel m_orthoSide {OrthoView::SideZY};
    SelectionSet m_selectionSet;  // F2H13
    Scene* m_scene = nullptr;  // F2H12: non-owning, set by EditorApplication

    // Hito 27: punter inyectado por EditorApplication.
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
    bool m_createEntityFromModelRequested = false;   // F2H57
    bool m_createEntityPlaceholderRequested = false; // F2H57 followup
    bool m_pickFromLoadedMeshesRequested = false;    // F2H57 followup
    bool m_convertModalRequested = false;            // F2H57 Bloque C
    entt::entity m_convertModalTarget{entt::null};   // F2H57 Bloque C
    bool m_deleteSelectedRequested = false;          // F2H57 Bloque C
    bool m_spawnEnvironmentRequested = false;
    bool m_spawnShadowDemoRequested = false; // Hito 16
    bool m_spawnPbrSpheresRequested = false; // Hito 17
    bool m_spawnLightStressRequested = false; // Hito 18
    bool m_spawnAnimatedCharacterRequested = false; // Hito 19
    bool m_openDemoMapRequested = false; // F2H44
    bool m_openNarrativeDemoRequested = false; // F2H50 Bloque C
    bool m_spawnFireParticlesRequested = false;     // Hito 29
    bool m_spawnTriggerRequested = false;           // Hito 33
    bool m_spawnDialogDemoRequested = false;        // F2H47
    bool m_generateNarrativeDemoMapRequested = false; // F2H50 Bloque A
    int  m_spawnStressTrisRequested = 0;            // F2H2 (target tris)
    bool m_spawnFullStressSceneRequested = false;   // F2H42
    bool m_recentsDirty = false;
    std::vector<std::filesystem::path> m_recentProjects;
    std::optional<std::filesystem::path> m_openProjectPath;
    std::optional<std::filesystem::path> m_openMapRequest;  // F2H8
    std::optional<BooleanOpRequestKind> m_booleanOpRequest; // F2H13 (kind only)
    std::vector<std::filesystem::path> m_projectMaps;       // F2H8
    std::filesystem::path m_currentMapPath;
    std::filesystem::path m_defaultMapPath;

    /// @brief Dibuja un modal bloqueante con [Nuevo Proyecto] [Abrir Proyecto]
    ///        + lista de recientes. SOLO cuando `m_hasProject == false`.
    void drawWelcomeModal();

    // F2H7: workspaces.
    WorkspaceManager m_workspaceManager;
    /// `-1` = no hay switch pendiente. Otro valor = index objetivo.
    int m_pendingWorkspaceSwitch{-1};

    // F2H22: requests del Toolbar lateral.
    int  m_gizmoModeRequested = -1;
    bool m_toggleFaceModeRequested = false;

    // F2H30 Bloque C: requests de la top toolbar.
    EditorSubMode m_subModeRequested = EditorSubMode::Object;
    bool m_hasSubModeRequest = false;
    bool m_togglePolygonDrawRequested = false;
    bool m_polygonDrawActive = false;

    // F2H31 Bloque B: tool selector.
    MapTool m_mapToolRequested = MapTool::Select;
    bool m_hasMapToolRequest = false;
    MapTool m_mapTool = MapTool::Select;

    // F2H31 Bloque C: snap-to-vertex toggle.
    bool m_toggleSnapToVertexRequested = false;
    bool m_snapToVertexEnabled = false;

    // F2H32 Bloque C: carve.
    bool m_carveRequested = false;

    // F2H35 Bloque E: labels point entities.
    bool m_toggleEntityLabelsRequested = false;
    bool m_showEntityLabels = true;
};

} // namespace Mood

// AUDIT-3: inline bodies de los pares request/consume.
#include "editor/ui/EditorUI_Spawn.inl"
#include "editor/ui/EditorUI_Tools.inl"
#include "editor/ui/EditorUI_Entity.inl"
#include "editor/ui/EditorUI_Project.inl"
