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
    ///        Persiste hasta que se cambie de nuevo.
    void setStatusMessage(std::string msg) { m_statusBar.setMessage(std::move(msg)); }

    /// @brief Lista de paneles, expuesta para que el menu "Ver" pueda togglear
    ///        su visibilidad.
    const std::vector<IPanel*>& panels() const { return m_panels; }

    /// @brief Acceso al panel Viewport para inyectar el framebuffer desde
    ///        EditorApplication y leer el tamano deseado.
    ViewportPanel& viewport() { return m_viewport; }

    /// @brief F2H28: paneles ortograficos del workspace "Editor de mapas".
    ///        EditorApplication los inyecta con sus FBOs respectivos al
    ///        renderear cada vista.
    OrthoViewportPanel& orthoTop()   { return m_orthoTop; }
    OrthoViewportPanel& orthoFront() { return m_orthoFront; }
    OrthoViewportPanel& orthoSide()  { return m_orthoSide; }

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

    /// @brief F2H30 Bloque C: la top toolbar del workspace "Editor de
    ///        mapas" pide setear sub-mode (Object/Vertex/Edge/Face) o
    ///        togglear el pincel poligonal. EditorApplication consume.
    void requestSubMode(EditorSubMode mode) {
        m_subModeRequested = mode;
        m_hasSubModeRequest = true;
    }
    bool consumeSubModeRequest(EditorSubMode& outMode) {
        if (!m_hasSubModeRequest) return false;
        outMode = m_subModeRequested;
        m_hasSubModeRequest = false;
        return true;
    }
    void requestTogglePolygonDraw() { m_togglePolygonDrawRequested = true; }
    bool consumeTogglePolygonDrawRequest() {
        const bool r = m_togglePolygonDrawRequested;
        m_togglePolygonDrawRequested = false;
        return r;
    }
    /// @brief F2H30 Bloque C: state read-only del pincel poligonal,
    ///        seteado por EditorApplication cada frame para que la
    ///        top toolbar pueda highlight el boton activo.
    void setPolygonDrawActive(bool v) { m_polygonDrawActive = v; }
    bool polygonDrawActive() const { return m_polygonDrawActive; }

    /// @brief F2H31 Bloque B: el toolbar "Map Tools" pide setear el tool
    ///        activo (Select/CreateBlock/Pincel). EditorApplication consume
    ///        cada frame y propaga a `m_mapTool`. Setea Pincel via
    ///        toggle-style (cancela si ya estaba); Select/Block son
    ///        idempotentes.
    void requestMapTool(MapTool tool) {
        m_mapToolRequested = tool;
        m_hasMapToolRequest = true;
    }
    bool consumeMapToolRequest(MapTool& outTool) {
        if (!m_hasMapToolRequest) return false;
        outTool = m_mapToolRequested;
        m_hasMapToolRequest = false;
        return true;
    }
    /// @brief F2H31 Bloque B: state read-only del MapTool activo, seteado
    ///        por EditorApplication cada frame para highlight del boton.
    void setMapTool(MapTool t) { m_mapTool = t; }
    MapTool mapTool() const { return m_mapTool; }

    /// @brief F2H31 Bloque C: el toolbar pide togglear el snap-to-vertex
    ///        (tecla V tambien lo dispara desde EditorOverlay).
    void requestToggleSnapToVertex() { m_toggleSnapToVertexRequested = true; }
    bool consumeToggleSnapToVertexRequest() {
        const bool r = m_toggleSnapToVertexRequested;
        m_toggleSnapToVertexRequested = false;
        return r;
    }
    void setSnapToVertexEnabled(bool v) { m_snapToVertexEnabled = v; }
    bool snapToVertexEnabled() const { return m_snapToVertexEnabled; }

    /// @brief F2H35 Bloque E: toggle de labels arriba de point entities
    ///        (Light/Audio/Trigger/Camera/Particle) en el viewport
    ///        perspectivo y en los 3 ortos. Default ON. El toolbar
    ///        del workspace "Editor de mapas" tiene el boton "Nombres"
    ///        para flipear. Persistido en `.moodproj` por proyecto.
    void setShowEntityLabels(bool v) { m_showEntityLabels = v; }
    bool showEntityLabels() const { return m_showEntityLabels; }
    void requestToggleEntityLabels() { m_toggleEntityLabelsRequested = true; }
    bool consumeToggleEntityLabelsRequest() {
        const bool r = m_toggleEntityLabelsRequested;
        m_toggleEntityLabelsRequested = false;
        return r;
    }

    /// @brief F2H32 Bloque C: el toolbar pide ejecutar carve sobre el
    ///        brush activo (Hammer-style boolean subtract automatico
    ///        contra todos los brushes que intersecten el AABB del
    ///        active). Sin keyboard shortcut por seguridad
    ///        (operacion destructiva).
    void requestCarve() { m_carveRequested = true; }
    bool consumeCarveRequest() {
        const bool r = m_carveRequested;
        m_carveRequested = false;
        return r;
    }

    /// @brief F2H30 Bloque C: acceso a la top toolbar.
    MapEditorTopBar& mapEditorTopBar() { return m_mapEditorTopBar; }

    /// @brief F2H33: panel de VisGroups del editor de mapas. EditorApplication
    ///        le inyecta scene + history al crearlo.
    VisGroupsPanel& visGroupsPanel() { return m_visGroupsPanel; }

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

    /// @brief F2H47: accessors a los panels de la sub-fase 2.5
    ///        (Narrativa). Inspector contextual + Browser hablan con
    ///        Editor via estos getters.
    DialogEditorPanel&         dialogEditor()    { return m_dialogEditor; }
    DialogBrowserPanel&        dialogBrowser()   { return m_dialogBrowser; }
    DialogNodeInspectorPanel&  dialogInspector() { return m_dialogInspector; }

    /// @brief F2H51: accessor al Item Browser. El Property Editor + el
    ///        Inspector section consumen la seleccion via `selectedPath()`.
    ItemBrowserPanel&          itemBrowser()     { return m_itemBrowser; }
    ItemPropertyEditorPanel&   itemPropertyEditor() { return m_itemPropertyEditor; }

    /// @brief F2H53: accessor al Quest Browser. El Property Editor (mismo
    ///        bloque) consume la seleccion via `selectedPath()`.
    QuestBrowserPanel&         questBrowser()       { return m_questBrowser; }
    QuestPropertyEditorPanel&  questPropertyEditor(){ return m_questPropertyEditor; }

    /// @brief Hito 27: HistoryStack del editor inyectado desde
    ///        EditorApplication. La MenuBar lo usa para los items
    ///        "Editar > Deshacer / Rehacer" (canUndo/canRedo + name del
    ///        comando activo). nullptr antes de la inicializacion.
    void setHistoryStack(HistoryStack* h) {
        m_history = h;
        m_visGroupsPanel.setHistoryStack(h);  // F2H33
    }
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
    void setScene(Scene* s) {
        m_scene = s;
        m_visGroupsPanel.setScene(s);  // F2H33
    }
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

    /// @brief F2H57: request para crear una entidad nueva a partir de
    ///        un modelo elegido por el dev via file picker. Workflow
    ///        Hammer Editor: click "+ Crear Entidad" -> file picker
    ///        del SO -> spawnea entidad con Transform + MeshRenderer
    ///        apuntando al modelo importado. EditorApplication consume
    ///        el request, abre `pfd::open_file` con filtro de modelos
    ///        (.fbx/.obj/.gltf/.glb) y dispara la creacion via
    ///        AssetManager::loadMesh + Scene::createEntity.
    void requestCreateEntityFromModel() { m_createEntityFromModelRequested = true; }
    bool consumeCreateEntityFromModelRequest() {
        const bool r = m_createEntityFromModelRequested;
        m_createEntityFromModelRequested = false;
        return r;
    }

    /// @brief F2H57 Bloque C: request para abrir el modal "Convertir
    ///        entidad" sobre la entidad pasada. El modal muestra los
    ///        kits disponibles (NPC con dialogo / Player / Item /
    ///        Luz puntual / etc.) y aplica los componentes
    ///        correspondientes. EditorApplication consume el request
    ///        en el frame loop y abre el modal ImGui.
    void requestEntityConvertModal(entt::entity e) {
        m_convertModalRequested = true;
        m_convertModalTarget    = e;
    }
    bool consumeEntityConvertModalRequest(entt::entity& outTarget) {
        if (!m_convertModalRequested) return false;
        m_convertModalRequested = false;
        outTarget = m_convertModalTarget;
        return true;
    }

    /// @brief F2H57 Bloque C: request para borrar la entidad activa de
    ///        la seleccion. Mismo flow que la tecla Delete pero
    ///        disparado desde el menu contextual del panel Escena.
    void requestDeleteSelectedEntity() { m_deleteSelectedRequested = true; }
    bool consumeDeleteSelectedRequest() {
        const bool r = m_deleteSelectedRequested;
        m_deleteSelectedRequested = false;
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

    /// @brief F2H44: request del Welcome modal para "demo onboarding".
    ///        Dispara NewProject (sincrono via dialog) + si el dev no
    ///        cancela, encadena requestSpawnAnimatedCharacter para que
    ///        la primera vista del editor sea el Fox animado caminando.
    void requestOpenDemoMap() { m_openDemoMapRequested = true; }
    bool consumeOpenDemoMapRequest() {
        const bool r = m_openDemoMapRequested;
        m_openDemoMapRequested = false;
        return r;
    }

    /// @brief F2H50 Bloque C: Welcome modal — crea proyecto fresco +
    ///        carga la escena narrative_demo.moodmap (genera si falta).
    ///        Equivalente al boton F2H44 pero apuntando al demo narrativo
    ///        (NPC Y Bot + Dialog + Trigger) en vez del stress scene.
    void requestOpenNarrativeDemo() { m_openNarrativeDemoRequested = true; }
    bool consumeOpenNarrativeDemoRequest() {
        const bool r = m_openNarrativeDemoRequested;
        m_openNarrativeDemoRequested = false;
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

    /// @brief F2H47: request para cargar un .mooddialog demo (3 nodos
    ///        + 2 choices) y abrirlo en el DialogEditor. Si el archivo
    ///        ya existe en assets/dialogs/, lo abre; si no, lo genera
    ///        programaticamente en disk + lo abre.
    void requestSpawnDialogDemo() { m_spawnDialogDemoRequested = true; }
    bool consumeSpawnDialogDemoRequest() {
        const bool r = m_spawnDialogDemoRequested;
        m_spawnDialogDemoRequested = false;
        return r;
    }

    /// @brief F2H50 Bloque A: request para generar (si falta) + abrir el
    ///        mapa demo de narrativa (`assets/maps/narrative_demo.moodmap`).
    ///        El handler arma 1 NPC con MeshRenderer + Animator + Dialog
    ///        + Trigger apuntando al `demo_intro.mooddialog`. La idea es
    ///        que el dev pueda ver el flujo dialog end-to-end con un char
    ///        real out-of-the-box. Editable como cualquier mapa.
    void requestGenerateNarrativeDemoMap() { m_generateNarrativeDemoMapRequested = true; }
    bool consumeGenerateNarrativeDemoMapRequest() {
        const bool r = m_generateNarrativeDemoMapRequested;
        m_generateNarrativeDemoMapRequested = false;
        return r;
    }

    /// @brief F2H42: request para spawnear UNA escena completa de stress
    ///        (cubos + 64 luces + esferas PBR + sombras + 2 chars
    ///        animados + particulas + trigger). El handler setea todos
    ///        los flags individuales de spawn — el frame loop los procesa
    ///        en orden al frame siguiente. Para baseline measurement de
    ///        F2H42 (optimizacion runtime) y futuras mediciones.
    void requestSpawnFullStressScene() { m_spawnFullStressSceneRequested = true; }
    bool consumeSpawnFullStressSceneRequest() {
        const bool r = m_spawnFullStressSceneRequested;
        m_spawnFullStressSceneRequested = false;
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
    Toolbar m_toolbar;  // F2H22: tools de edicion (gizmo modes + brushes + face)
    MapEditorTopBar m_mapEditorTopBar;  // F2H30 Bloque C: top toolbar del workspace "Editor de mapas"
    VisGroupsPanel m_visGroupsPanel;  // F2H33: panel "Grupos"
    // F2H28: 3 paneles ortograficos del workspace "Editor de mapas".
    // Inicializados con la vista correspondiente; arrancan invisibles
    // (los hace visibles applyDefaultVisibilityForWorkspace cuando se
    // activa el workspace "Editor de mapas").
    OrthoViewportPanel m_orthoTop  {OrthoView::TopXZ};
    OrthoViewportPanel m_orthoFront{OrthoView::FrontXY};
    OrthoViewportPanel m_orthoSide {OrthoView::SideZY};
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
    bool m_createEntityFromModelRequested = false;  // F2H57
    bool m_convertModalRequested = false;            // F2H57 Bloque C
    entt::entity m_convertModalTarget{entt::null};   // F2H57 Bloque C
    bool m_deleteSelectedRequested = false;          // F2H57 Bloque C
    bool m_spawnEnvironmentRequested = false;
    bool m_spawnShadowDemoRequested = false; // Hito 16
    bool m_spawnPbrSpheresRequested = false; // Hito 17
    bool m_spawnLightStressRequested = false; // Hito 18
    bool m_spawnAnimatedCharacterRequested = false; // Hito 19
    bool m_openDemoMapRequested = false; // F2H44: Welcome modal demo
    bool m_openNarrativeDemoRequested = false; // F2H50 Bloque C
    bool m_spawnFireParticlesRequested = false;     // Hito 29
    bool m_spawnTriggerRequested = false;           // Hito 33
    bool m_spawnDialogDemoRequested = false;        // F2H47
    bool m_generateNarrativeDemoMapRequested = false; // F2H50 Bloque A
    int  m_spawnStressTrisRequested = 0;             // F2H2 (target tris)
    bool m_spawnFullStressSceneRequested = false;   // F2H42
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

    // F2H30 Bloque C: requests de la top toolbar.
    EditorSubMode m_subModeRequested = EditorSubMode::Object;
    bool m_hasSubModeRequest = false;
    bool m_togglePolygonDrawRequested = false;
    bool m_polygonDrawActive = false;

    // F2H31 Bloque B: tool selector del toolbar lateral.
    MapTool m_mapToolRequested = MapTool::Select;
    bool m_hasMapToolRequest = false;
    MapTool m_mapTool = MapTool::Select;

    // F2H31 Bloque C: snap-to-vertex toggle.
    bool m_toggleSnapToVertexRequested = false;
    bool m_snapToVertexEnabled = false;

    // F2H32 Bloque C: carve action button.
    bool m_carveRequested = false;

    // F2H35 Bloque E: toggle labels point entities. Default ON.
    bool m_toggleEntityLabelsRequested = false;
    bool m_showEntityLabels = true;
};

} // namespace Mood
