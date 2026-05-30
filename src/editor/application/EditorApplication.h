#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "core/math/Plane.h"  // F2H30 Bloque B: snapshot pre/post de planos
#include "editor/application/Autosave.h"
#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/commands/HistoryStack.h"
#include "systems/physics/TriggerSystem.h"
#include "systems/physics/ForceFieldSystem.h"  // F2H72
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/pipeline/Fog.h"
#include "engine/scene/core/EditorCamera.h"
#include "engine/scene/core/FpsCamera.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/queries/ViewportPick.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/world/grid/GridMap.h"
#include "platform/Window.h"

#include <glm/vec2.hpp>  // F2H30 Bloque D: ModalShortcutState.mouseStart
#include <glm/mat4x4.hpp>  // F2H30 Bloque D: updateModalShortcut(vp,...)

#include <filesystem>
#include <memory>
#include <optional>
#include <string>  // F2H30 Bloque B: brushTag en OrthoVertexEditSession.
#include <utility>  // F2H29 Bloque B: std::pair en OrthoDragSession.
#include <vector>

namespace Mood {

class IFramebuffer;
class IMesh;
class MaterialPreviewRenderer;
class OpenGLDebugRenderer;  // F2H83: helpers de debug overlay
class MeshThumbnailRenderer;
class AnimationPreviewRenderer;
class SceneRenderer;
class ScriptSystem;
class AudioDevice;
class AudioSystem;
class PhysicsWorld;
class AnimationSystem;
class NavSystem;
class ParticleSystem;

class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();

    EditorApplication(const EditorApplication&) = delete;
    EditorApplication& operator=(const EditorApplication&) = delete;

    /// @brief Ejecuta el loop principal. Devuelve el codigo de salida para
    ///        retornar desde main().
    int run();

private:
    void processEvents();
    void beginFrame();
    void endFrame();

    /// @brief Procesa interacciones de viewport: click-to-select 3D +
    ///        face mode, polygon brush, clip tool, click-select ortho,
    ///        drag-edit en ortos. Definido en
    ///        `EditorApplication_RunInteractions.cpp` (split AUDIT-1).
    ///        Llamado una vez por frame desde `run()`. Al final invoca
    ///        `processOrthoToolModes()` (sub-split AUDIT-2).
    void processViewportInteractions();

    // break-B3: sub-pasos de processViewportInteractions, uno por
    // contexto de interaccion. Cada uno tiene sus propios guards en el
    // entry y es no-op si no aplica. Orden: el dispatcher los llama en
    // secuencia y la independencia entre handlers esta auditada (no hay
    // estado producido por uno consumido por otro dentro del mismo frame).
    void handlePerspectiveClickSelect();
    void handlePolygonDrawClicks();
    void handleClipToolClicks();
    void handleOrthoClickSelect();
    void handleOrthoDragEdit();

    /// F3H25: dibuja el modal one-shot "¿Recuperar última sesión?" si
    ///        `m_recoveryModalPending=true`. Botones: Restaurar (carga el
    ///        autosave + marca dirty), Descartar (borra el autosave).
    ///        En ambos casos resetea el flag para que el modal no
    ///        reaparezca en el frame siguiente.
    void processRecoveryModal();

    // break-B3: fases de run() extraidas (560 LOC -> dispatcher + 5 fases).
    // Cada fase asume las precondiciones documentadas en el caller. dt
    // viaja por parametro (no captura de loop locals).
    void tickHotReload(f32 dt);
    void tickFrameMetrics(f32 dt, f64 dtD);
    void pumpUiRequests();
    void pumpSpawnAndDropRequests();
    void tickSystems(f32 dt);

    /// @brief Modos de herramienta de los viewports ortograficos: block
    ///        tool, marquee select, vertex/edge edit. Definido en
    ///        `EditorApplication_RunInteractions_ToolModes.cpp` (sub-split
    ///        AUDIT-2 para mantener `_RunInteractions.cpp` bajo el hard
    ///        cap de 800 LOC — antes vivian en el mismo archivo con 1065
    ///        LOC). Invocado al final de `processViewportInteractions()`.
    void processOrthoToolModes();

    // break-B3: sub-pasos de processOrthoToolModes, uno por sub-modo.
    // Cada uno corre con sus propios guards (modo+scene+subMode+tool);
    // el caller `processOrthoToolModes()` los llama en secuencia y
    // ningun handler depende del estado producido por el anterior.
    void handleOrthoBlockToolDrag();
    void handleOrthoMarqueeSelect();
    void handleOrthoVertexEdgeEdit();

    /// @brief Renderiza la escena al framebuffer offscreen que muestra el
    ///        panel Viewport.
    void renderSceneToViewport(f32 dt);

    /// @brief Actualiza la camara activa leyendo input del panel Viewport
    ///        (Editor Mode) o de SDL en relative mouse mode (Play Mode).
    void updateCameras(f32 dt);

    // break-B3: sub-pasos de updateCameras separados por contexto.
    // Cada uno asume que el caller ya verifico el modo + early-returns.
    // F3H21: recibe dt para alimentar el lerp animator del EditorCamera.
    void updateEditorCamera(f32 dt);
    void processMountDismountToggle();
    void updateVehicleChaseCamera(f32 dt);
    void updateMountPromptHint();
    void updateOnFootCharController(f32 dt);

    void enterPlayMode();
    void exitPlayMode();

    /// @brief Desplazamiento del tile (0,0) del mapa en el mundo. Usado
    ///        por el render y por el sistema de colisiones para trabajar
    ///        con el mapa centrado en el origen del mundo.
    glm::vec3 mapWorldOrigin() const;

    /// @brief Aspect ratio del framebuffer del viewport (LDR final). Lo
    ///        usan todos los pasos que necesitan armar projection matrix
    ///        (drops, raycast, render). Aislado en un helper para que el
    ///        callsite no dependa del owner del FB (SceneRenderer).
    f32 viewportAspect() const;

    /// @brief Reemplaza m_map con la sala 8x8 hardcodeada (perimetro grid,
    ///        columna central brick). Se usa al arrancar y al cerrar proyecto.
    void buildInitialTestMap();

    /// @brief Hito 12: crea/sincroniza rigid bodies con Jolt. Por frame:
    ///        - entidades con RigidBodyComponent sin body creado -> create.
    ///        - en Play Mode: tras step(), copia position del body al Transform.
    ///        En Editor Mode solo materializa bodies nuevos; no stepea.
    void updateRigidBodies(f32 dt);

    /// @brief Reconstruye `m_scene` desde cero a partir del estado actual de
    ///        `m_map`: una entidad por tile solido con Tag + Transform +
    ///        MeshRenderer. Se llama al cargar proyecto / cerrar proyecto /
    ///        buildInitialTestMap — cambios globales donde invalidar la
    ///        seleccion es aceptable.
    ///        Estrategia brute-force O(W*H). Para edits localizados (drop de
    ///        textura sobre un tile), usar `updateTileEntity` que conserva
    ///        handles y seleccion.
    void rebuildSceneFromMap();

    /// @brief Edit localizado para una sola tile. Si la entidad ya existe
    ///        (`Tile_X_Y`), le actualiza el MeshRenderer in-place; si no,
    ///        la crea con los mismos defaults que rebuildSceneFromMap.
    ///        Uso: drop de textura desde AssetBrowser (Hito 10 Bloque 4).
    ///        Preserva la seleccion en Hierarchy.
    void updateTileEntity(u32 tileX, u32 tileY, TextureAssetId texture);

    /// @brief Sincroniza el titulo de la ventana con el estado actual:
    ///        nombre del proyecto + indicador de dirty. Llamar cuando algo
    ///        de ese estado cambia (abrir / guardar / dirty).
    void updateWindowTitle();

    /// @brief Marca el proyecto como modificado. No-op si no hay proyecto
    ///        abierto (el mapa de prueba no se persiste).
    void markDirty();

    // Handlers de acciones del menu Archivo + modal Welcome.
    void handleNewProject();
    void handleOpenProject();
    void handleSave();
    void handleSaveAs();
    void handleCloseProject();
    /// @brief Hito 21 Bloque 5: empaqueta el proyecto activo en una
    ///        carpeta destino (file dialog). Pide guardar primero si
    ///        hay dirty. Muestra MessageBox con resultado.
    void handlePackageProject();

    // F2H8: gestion multi-mapa intra-proyecto.
    /// @brief Crea un mapa nuevo (16x16, vacio) en `<project.root>/maps/`.
    ///        Pide nombre via pfd::save_file. Si el actual esta dirty,
    ///        confirmDiscardChanges. Switch al nuevo map.
    void handleNewMap();
    /// @brief Guarda el mapa actual con otro nombre. pfd::save_file con
    ///        default `<currentName>_copy.moodmap`. Agrega al
    ///        project.maps + switch al nuevo.
    void handleSaveMapAs();
    /// @brief Carga un mapa especifico del proyecto (entre los `maps[]`).
    ///        Usado por el menu "Archivo > Mapa > Abrir mapa". Si actual
    ///        dirty, confirmDiscardChanges.
    void handleOpenMap(const std::filesystem::path& mapPath);
    /// @brief Marca el mapa actual como `defaultMap` del proyecto.
    void handleSetCurrentMapAsDefault();
    /// @brief Elimina el mapa actual del proyecto + del disco. Si era
    ///        el ultimo, popup error. Si era el default, reasigna.
    void handleDeleteCurrentMap();

    /// @brief F2H11: crea una entidad nueva con TransformComponent +
    ///        BrushComponent (Box 1x1x1) en el origen. Marca el mapa
    ///        como dirty. Selecciona la entidad nueva.
    void handleAddBoxBrush();

    /// @brief F2H14: spawn de primitivas extendidas. Misma estructura
    ///        que handleAddBoxBrush — crean entidad con tag unico
    ///        de su prefijo + Brush correspondiente.
    void handleAddCylinderBrush();
    void handleAddSphereBrush();
    void handleAddPyramidBrush();
    void handleAddWedgeBrush();
    void handleAddPrismTriangularBrush();
    void handleAddPrismHexagonalBrush();
    // F2H59: primitivas clasicas adicionales (plano / quad / cono / capsula).
    void handleAddPlaneBrush();    // box aplastada 10x0.05x10 -- suelo.
    void handleAddQuadBrush();     // box chica 1x0.05x1 -- billboard.
    void handleAddConeBrush();     // cono regular -- spotlights / sombreros.
    void handleAddCapsuleBrush();  // sphere estirada en Y -- pildora proxy.

    // F2H60 polish iter2: spawn de luz como entidad nueva (sin mesh).
    // Tag + Transform + LightComponent del tipo correspondiente.
    void handleAddDirectionalLight(); // dir=(-0.3,-1.0,-0.2), castShadows=true.
    void handleAddPointLight();       // pos=(0,1,0), radius=10m, color calido.
    void handleAddDummy();            // F4H1: cubo + HealthComponent.
    void handleAddPlayer();           // F4H2 Bloque B: Tag=player + WeaponComponent auto.

    /// @brief F2H86: spawn entidad con `Tag + Transform + EnvironmentComponent`
    ///        (sin MeshRenderer — es config global de escena, no geometria).
    ///        Guard: si ya existe un Environment en la escena, no-op + log.
    void handleAddEnvironment();

    /// @brief F3H22: garantiza que la scene tenga el singleton Environment.
    ///        Si no existe, lo crea con defaults SIN push history ni
    ///        markDirty (auto-spawn, no es accion explicita del dev).
    ///        Llamado tras cargar proyecto o crear scene nueva — Blender
    ///        World Properties pattern.
    void ensureEnvironmentExists();

    /// @brief F2H20: compila los brushes del mapa actual a una mesh
    ///        estatica unificada (weld + cull caras internas + merge
    ///        por material) y muestra dialog con stats.
    void handleCompileMap();

    /// @brief F2H20: corre `handleCompileMap` y escribe `.obj` + `.mtl`
    ///        al destino elegido por `pfd::save_file`.
    void handleExportObj();

    /// @brief F2H13: aplica una operacion booleana en cascada sobre
    ///        el SelectionSet. La `active` es el "tool brush" B; las
    ///        demas selected son las A's. Para cada A: aplica
    ///        `op(A, B)` (subtract / unionOp / intersectOp), destruye
    ///        A, crea entidades para cada brush resultante. B se
    ///        preserva. Pushea un BooleanOpCommand por cada A al
    ///        HistoryStack — undo es por cada A individual (N Ctrl+Z
    ///        para revertir todo el cascade). CompoundCommand para
    ///        agrupar es hito futuro.
    void handleBooleanOp(EditorUI::BooleanOpRequestKind kind);

    /// @brief F2H8: sincroniza el snapshot de mapas del proyecto al
    ///        EditorUI (para que MenuBar pueda dibujar el submenu).
    ///        Llamar despues de cada operacion que cambie m_project.maps,
    ///        m_project.defaultMap o m_currentMapPath. No-op si no hay
    ///        proyecto activo (limpia el snapshot).
    void syncMapsSnapshot();

    /// @brief Hito 22 Bloque 3: crea un .lua nuevo en
    ///        `assets/scripts/<nombre>.lua` con un template, y refresca
    ///        el Asset Browser para que aparezca en la lista.
    void handleNewScript();

    /// @brief Intenta abrir el proyecto en `moodprojPath`. Devuelve true si
    ///        quedo activo; false si fallo (loguea la causa).
    bool tryOpenProjectPath(const std::filesystem::path& moodprojPath);

    /// @brief Agrega un path al tope de la lista de proyectos recientes,
    ///        deduplicando y limitando el tamano.
    void addToRecentProjects(const std::filesystem::path& moodprojPath);

    /// @brief Si hay cambios sin guardar, pregunta al usuario que hacer
    ///        (guardar / descartar / cancelar). Devuelve `true` si la accion
    ///        puede proceder, `false` si hay que abortarla.
    bool confirmDiscardChanges();

    // Handlers de spawn (stress tests del menu Debug) + drops del viewport.
    // Implementaciones en `DemoSpawners_Stress.cpp` y `_Drop.cpp`. Cada uno
    // hace su propio `consume*Request` y, si hay request pendiente, crea la
    // entidad correspondiente. No-op si la condicion no se cumple.
    //
    // post-v2.0.2 cleanup: borrados 10 demos historicos (Rotator, HudDemo,
    // EnemyDemo, ShadowDemo, PbrSpheres, AnimatedCharacter, FireParticles,
    // DialogDemo, NarrativeDemoMap, FullStressScene) y 5 spawners legacy
    // huérfanos (PointLight, Environment, PhysicsBox, AudioSource, Trigger
    // — todos reemplazados por el flow "+ Crear Entidad" del HierarchyPanel
    // + Add Component en Inspector).
    void processSpawnLightStressRequest();       // menu Debug
    void processSpawnStressTrisRequest();        // menu Debug (10K/100K/500K/1M)

    /// @brief Hito 28: empaqueta una creacion (spawn / drop) como
    ///        `CreateEntityCommand` y la empuja al `m_history`. El
    ///        callsite ya creo las entidades; este helper captura el
    ///        snapshot para que Ctrl+Z las destruya. `markDirty()` se
    ///        invoca aca tambien para no duplicarlo en cada path.
    void pushCreatedEntities(std::vector<Entity> created, std::string label);
    void processSavePrefabRequest();
    void processCreateEntityFromModelRequest();  // F2H57
    void processCreateEntityPlaceholderRequest(); // F2H57 followup
    void renderPickFromLoadedMeshesModal();       // F2H57 followup
    void renderConvertEntityModal();             // F2H57 Bloque D
    void processViewportTextureDrop();
    void processViewportMeshDrop();
    void processViewportPrefabDrop();
    void processViewportMaterialDrop();
    void processViewportScriptDrop(); // Hito 22 Bloque 2
    void processViewportItemDrop();   // F2H52 Bloque D
    void processViewportVehicleDrop(); // F2H70.3 Bloque F

    /// @brief Lee `<cwd>/.mood/editor_state.json` si existe:
    ///        - preferencias (debugDraw, etc.)
    ///        - lista de proyectos recientes
    ///        - auto-abre el mas reciente (convencion Unity/Godot)
    void loadEditorState();

    /// @brief Escribe el estado actual a `<cwd>/.mood/editor_state.json`.
    ///        Silencioso si falla (no queremos que un shutdown se rompa por
    ///        un archivo de preferencias).
    void saveEditorState() const;

    std::unique_ptr<Window> m_window;

    // Hito 21 Bloque 2: todo el pipeline de render (FBs, shaders PBR,
    // skybox, shadow, post-process, IBL, light grid + SSBOs, debug
    // renderer) vive en SceneRenderer — compartido con MoodPlayer.
    // El editor solo le pasa scene + assets + camara + tamano del
    // panel cada frame, y agrega su debug 3D entre `renderScene` y
    // `endFrame`.
    std::unique_ptr<SceneRenderer> m_sceneRenderer;

    /// @brief F2H21: renderer del preview esferico del Material Editor.
    ///        FBO 256x256 off-screen + reusa shader PBR + IBL del
    ///        SceneRenderer. Inyectado al MaterialEditorPanel. Solo
    ///        dibuja si el panel esta visible (cost ~0 cuando cerrado).
    std::unique_ptr<MaterialPreviewRenderer> m_materialPreview;

    /// @brief F2H80: renderer de miniaturas 3D de meshes (cache por mesh,
    ///        render-once). Lo usan el modal "+ Crear Entidad" (tab Meshes,
    ///        vive en EditorApplication) y el Asset Browser (inyectado). IBL
    ///        del SceneRenderer, igual que m_materialPreview.
    std::unique_ptr<MeshThumbnailRenderer> m_meshThumbnails;

    /// @brief F3H14: ultimo `UserSettings::editor().thumbnailResolution` que
    ///        usamos al construir m_meshThumbnails. Si en el siguiente
    ///        frame difiere (dev movio el slider en User Preferences),
    ///        recreamos m_meshThumbnails con el nuevo size + reinyectamos
    ///        IBL + reinyectamos diskCacheRoot + Asset Browser. La cache
    ///        memoria se pierde, la cache disco persiste (filename incluye
    ///        size, no chocan).
    int m_lastThumbnailResolution = 128;

    /// @brief F2H81: preview de animaciones (NPC skinneado posado por un clip,
    ///        render por frame). IBL del SceneRenderer; inyectado al Asset
    ///        Browser. Solo dibuja cuando el tab Animations tiene un clip
    ///        seleccionado (cost ~0 si no).
    std::unique_ptr<AnimationPreviewRenderer> m_animPreview;

    std::unique_ptr<ScriptSystem> m_scriptSystem;
    std::unique_ptr<AudioDevice> m_audioDevice;
    std::unique_ptr<AudioSystem> m_audioSystem;
    std::unique_ptr<AnimationSystem> m_animationSystem; // Hito 19
    std::unique_ptr<NavSystem>       m_navSystem;       // Hito 23
    std::unique_ptr<ParticleSystem>  m_particleSystem;  // Hito 29
    TriggerSystem                    m_triggerSystem;   // Hito 33: stateless
    ForceFieldSystem                 m_forceFieldSystem; // F2H72: stateless

    // Hito 20: HUD del juego (HP / Ammo / crosshair) y menu de pausa.
    // Implementado via el OverlayDraw del ViewportPanel — drawlist de
    // ImGui. Visible solo en `EditorMode::Play`. El estado vive en
    // `engine/game/GameState` (singleton) para que los scripts Lua
    // puedan leerlo/mutarlo via la tabla `hud`. Esc togglea
    // `GameState::paused()`; mientras esta activo el FpsCamera no
    // procesa input (gameplay congelado).
    // `m_pausedLastFrame` permite detectar transiciones de
    // `GameState::paused()` y sincronizar `SDL_SetRelativeMouseMode`
    // (cursor visible cuando pausado, atrapado durante gameplay). Es
    // unica fuente de verdad para el cursor en Play Mode — los
    // handlers de Esc y "Continuar" solo tocan el flag.
    bool m_pausedLastFrame = false;

    /// @brief Dibuja el HUD del juego + menu de pausa (cuando aplica)
    ///        sobre el panel Viewport via drawlist de ImGui. Llamado
    ///        desde el `OverlayDraw` registrado en el ctor cuando el
    ///        editor esta en Play Mode. Implementado en
    ///        `EditorPlayMode.cpp`.
    void drawGameOverlay(struct ImDrawList* dl,
                         float x0, float y0, float w, float h);

    /// @brief Dibuja iconos de entidades (Light/Audio), halo de seleccion
    ///        y gizmo (translate/rotate/scale) sobre el viewport via
    ///        drawlist de ImGui. Tambien atiende hotkeys W/E/R/Period y
    ///        Delete/Backspace para borrar entidad. Llamado desde el
    ///        `OverlayDraw` registrado en el ctor cuando el editor esta
    ///        en Editor Mode. Implementado en `EditorOverlay.cpp`.
    void drawEditorOverlay(struct ImDrawList* dl,
                           float x0, float y0, float w, float h);

    /// @brief F2H24: handles + drag-state del gizmo
    ///        translate/rotate/scale del overlay 2D. Extraido de
    ///        `drawEditorOverlay` para mantener cada archivo bajo el
    ///        cap de LOC. Recibe `vp` + viewport rect + selected ya
    ///        validado. Implementado en `EditorOverlay_Gizmo.cpp`.
    void drawEditorOverlayGizmo(struct ImDrawList* dl,
                                  float x0, float y0, float w, float h,
                                  const glm::mat4& vp,
                                  Entity selected, float osx, float osy);

    /// @brief Elimina la entidad actualmente seleccionada del scene.
    ///        No-op si no hay seleccion o si la entidad es un tile del
    ///        mapa (`Tile_X_Y`, vienen del GridMap y reaparecen al
    ///        rebuild). Limpia side-effects (Jolt body) antes del
    ///        destroy. Llamado desde el handler de tecla Delete/Backspace
    ///        en `processEvents`.
    void deleteSelectedEntity();

    /// @brief F2H85: duplica las entidades del `SelectionSet` con un
    ///        offset chico en X (Blender-style Shift+D). Para cada entity
    ///        seleccionada, `serializeEntityToJson` -> `parseEntityFromJson`
    ///        para obtener un `SavedEntity`, mutamos su `position` + tag
    ///        para distinguirla, y `SceneLoader::applyOneEntity` crea la
    ///        copia. Wrappeado en `CreateEntityCommand` via
    ///        `pushCreatedEntities` -> undoable. La nueva entity queda
    ///        seleccionada (multi: la ultima como active). Skip silencioso
    ///        de Tile_X_Y (vienen del GridMap, no se duplican). Llamado
    ///        desde el handler Shift+D en `processEvents`.
    void duplicateSelectedEntities();

    /// @brief F3H27: Ctrl+G estilo Blender. Toma el SelectionSet y crea
    ///        un Empty `Group_<N>` centrado en el centroide del AABB
    ///        combinado de los selectos; los reparenta como hijos
    ///        preservando world-space. No-op si selección < 2.
    void groupSelectedEntities();

    /// @brief F3H27: Shift+Ctrl+G estilo Blender. Para cada selecto que
    ///        tenga padre, lo des-parenta (parent=null) preservando
    ///        world-space. No-op si ningún selecto tiene padre.
    void ungroupSelectedEntities();

    /// @brief Editor-side overlay 3D que se dibuja DENTRO del scene FB
    ///        entre `SceneRenderer::renderScene` y `endFrame`. Usa el
    ///        debug renderer del SceneRenderer para acumular: tile
    ///        picking + drag highlights (cyan/yellow), OBB de la
    ///        entidad seleccionada (naranja), F1 debug AABBs (tiles +
    ///        player). Implementado en `EditorRenderPass_Overlay.cpp`.
    void drawEditorScene3DOverlay(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const glm::vec3& worldOrigin);

    /// @brief F2H83: helpers F1-debug por feature, extraidos de
    ///        `drawEditorScene3DOverlay` para mantener el orquestador
    ///        bajo el cap de LOC. Cada uno solo lee `m_scene` + opcional
    ///        `m_assetManager` / `m_physicsWorld`, y dibuja con `dbg`.
    ///        Llamados desde el block `if (m_debugDraw && m_scene)`.
    ///        Implementacion en `EditorRenderPass_Overlay_Debug.cpp`.
    void drawTriggersDebugOverlay(OpenGLDebugRenderer& dbg);
    void drawJointsDebugOverlay(OpenGLDebugRenderer& dbg);
    void drawForceFieldsDebugOverlay(OpenGLDebugRenderer& dbg);
    void drawRagdollsDebugOverlay(OpenGLDebugRenderer& dbg);
    void drawVehiclesDebugOverlay(OpenGLDebugRenderer& dbg);

    std::unique_ptr<PhysicsWorld> m_physicsWorld;

    // AssetManager: owner de todas las texturas cargadas. Se destruye ANTES
    // del contexto GL (ver destructor).
    std::unique_ptr<AssetManager> m_assetManager;
    TextureAssetId m_wallTextureId = 0; // grid.png cargado al arrancar

    // Escala SI: 1 unidad = 1 metro (ver DECISIONS.md). Mapa 8x8 con
    // tileSize=3 ocupa 24x24 m; diagonal ~34 m. Cam orbital con radius=30
    // deja el mapa entero en cuadro. Cam FPS en tile interior (2,6):
    // world = origen_mapa + ((2+0.5)*3, 1.6, (6+0.5)*3) = (-4.5, 1.6, 7.5).
    // F2H23 polish: radio default reducido de 30m a 12m. Antes el mapa
    // era 48x48 (16 tiles x 3m) y radio=30 daba vista de "todo el mapa
    // desde lejos". Ahora con mapa 12x12 (8 tiles x 1.5m) un radio chico
    // pone la camara al borde del piso, ideal para empezar a editar
    // brushes 1m.
    EditorCamera m_editorCamera{45.0f, 30.0f, 12.0f};
    // F2H41: spawn al centro del mapa (0,1.6,0). Pre-F2H41 era
    // (-4.5,1.6,7.5) legacy de un demo. Convencion: mapa centrado
    // en world (0,0,0) — Floor en (0,-0.05,0). Spawnear en (0,1.6,0)
    // pone al player en el centro por default.
    FpsCamera m_playCamera{glm::vec3(0.0f, 1.6f, 0.0f), -90.0f, 0.0f};
    EditorMode m_mode = EditorMode::Editor;
    /// @brief F2H17: sub-modo del Editor estilo Blender. Toggle con
    ///        tecla 3 (Face). 1 (vertex) y 2 (edge) reservadas para
    ///        hitos futuros. Esc vuelve a Object Mode.
    EditorSubMode m_subMode = EditorSubMode::Object;

    /// @brief F2H31 Bloque B: tool del workspace "Editor de mapas". Default
    ///        Select (marquee al drag en empty space — Hammer-style). El
    ///        dev cambia a CreateBlock para spawnear brushes via drag.
    ///        Pincel se setea via togglePolygonDrawMode (que ya maneja el
    ///        m_polyDraw); este enum mantiene la coherencia visual del
    ///        toolbar (los 3 botones Select/Block/Pincel mutually exclusive).
    MapTool m_mapTool = MapTool::Select;

    /// @brief F2H31 Bloque C: si esta activo, los snaps de pincel /
    ///        block tool prueban primero contra los vertices de brushes
    ///        existentes (threshold 8 px screen-space) y solo caen al
    ///        grid del workspace si ninguno gana. Toggle con tecla `V`
    ///        o boton "Snap V" del toolbar lateral. Default false (Hammer
    ///        clasico solo snapea al grid).
    bool m_snapToVertexEnabled = false;

    /// @brief F2H31 Bloque C: snap helper compartido por pincel y block
    ///        tool. Si `m_snapToVertexEnabled` y hay un vertex de algun
    ///        brush dentro de threshold ~8 px screen-space del orto,
    ///        devuelve la pos world de ese vertex; sino snap al grid
    ///        del workspace (`m_hammerSnapStep`). Si snap-to-vertex
    ///        esta off, snap al grid directo.
    glm::vec3 snapToVertexOrGrid(const glm::vec3& worldPt,
                                  const struct OrthoCamera& cam,
                                  f32 oAspect);

    // F3H20: el experimento de vertex snap en el perspectivo (helpers
    // snapToVertexInScene + findSnapTargetForGizmo +
    // closestVertexOnEntityToWorld) se descarto en iter 4. Resulto "medio
    // raro" comparado a grid snap estilo Hammer — los marcadores yellow
    // confundian mas de lo que ayudaban. El perspectivo usa grid snap
    // (snapGridEnabled / snapGridStep) integrado en EditorOverlay_Gizmo +
    // EditorOverlay_Modal. El vertex snap orto (workspace "Editor de
    // mapas") sigue intacto en snapToVertexOrGrid.

    /// @brief F2H28 Bloque G: snap step (en world units) de los viewports
    ///        ortograficos del workspace "Editor de mapas". Cycleable con
    ///        Ctrl + + / Ctrl + - en valores [1, 2, 4, 8, 16, 32, 64, 128].
    ///        Default 16 (convencion Hammer). Solo aplica cuando el
    ///        workspace activo es "Editor de mapas"; otros workspaces no
    ///        consumen el atajo.
    u32 m_hammerSnapStep = 16u;

    // F2H83: definiciones de las structs de session/state (OrthoDragSession,
    // OrthoBlockToolSession, OrthoMarqueeSession, ClipToolSession,
    // OrthoVertexEditSession, PolygonDrawSession, ModalShortcutEntry/State,
    // GizmoKeyTapState, GizmoMode, GizmoDragState) movidas a archivo aparte
    // para que el header se mantenga bajo el cap de LOC. El preprocesador
    // las inserta como tipos anidados de EditorApplication — sin cambio
    // semantico vs el inline previo. Ver
    // [docs/hitos/F2H83.md](../../docs/hitos/F2H83.md).
    #include "editor/application/EditorApplication_Sessions.inl"

    OrthoDragSession m_orthoDragSession;
    OrthoBlockToolSession m_orthoBlockSession;
    OrthoMarqueeSession m_orthoMarquee;
    ClipToolSession m_clipTool;

    /// @brief F2H32 Bloque B: ejecuta el clip de los brushes selectos
    ///        usando el plano armado del `m_clipTool`. Spawnea los
    ///        brushes resultado, destruye los originales, pushea el
    ///        ClipBrushesCommand para Ctrl+Z agrupado. Llamado al
    ///        confirmar con Enter o boton del toolbar.
    void confirmClipTool();
    /// @brief Cancela la sesion del clip sin spawnear (Esc).
    void cancelClipTool();
    /// @brief Cycle Front -> Back -> Both -> Front. Llamado por tecla T
    ///        durante la sesion activa.
    void cycleClipKeepMode();

    /// @brief F2H32 Bloque C: carve sobre el brush activo. Resta del
    ///        active todos los brushes que intersectan su AABB world.
    ///        Reusa Csg::subtract iterativo + BooleanOpCommand
    ///        (kind=Subtract con bSnapshot vacio porque hay multiples
    ///        carvers — todos preservados; solo el active se reemplaza
    ///        por sus fragmentos resultado). Llamado por el boton del
    ///        toolbar lateral.
    void handleCarve();

    /// @brief F2H29 Bloque C: spawnea un Box brush con `transform`
    ///        especificado (en lugar del `mat4(1.0f)` que usan las
    ///        primitivas del menu Brush > Anadir > Box). Usado por el
    ///        block tool del workspace "Editor de mapas".
    void spawnBoxBrushAt(const glm::mat4& transform);

    OrthoVertexEditSession m_orthoVertexEdit;

    PolygonDrawSession m_polyDraw;

    /// @brief F2H30 Bloque C: activa el modo pincel (toggle). Si ya
    ///        habia una sesion en progreso, la cancela.
    void togglePolygonDrawMode();
    /// @brief Cierra el polígono (Enter): valida, spawnea brush.
    void closePolygonDraw();
    /// @brief Cancela la sesion (Esc) sin spawnear.
    void cancelPolygonDraw();

    ModalShortcutState m_modalShortcut;

    GizmoKeyTapState m_gizmoKeyTap;

    /// @brief Activa el modal G/R/S. `field` = 0 (Position/G), 1 (Rotation/R),
    ///        2 (Scale/S). Conditions: workspace != Programar, mouse sobre
    ///        viewport perspectivo, hay seleccion, !WantTextInput, !pincel.
    ///        Si ya hay un modal activo, lo cancela primero.
    void startModalShortcut(int field);
    /// @brief Llamado cada frame por drawEditorOverlay. Lee mouse + teclas
    ///        X/Y/Z + click + Esc. Aplica delta o cierra/cancela.
    void updateModalShortcut(const glm::mat4& vp, float vx0, float vy0,
                              float vw, float vh);
    /// @brief Confirma: revert a startValue + push MultiEditTransformCommand.
    void confirmModalShortcut();
    /// @brief Cancela: revert a startValue, sin push.
    void cancelModalShortcut();

    // Dimensiones del AABB del jugador (0.6 x 1.8 x 0.6 m). Centrado en la
    // posicion de la camara FPS. Escala SI realista: una persona promedio.
    // El half-extent 0.3 m es muy superior al near clipping plane (0.1 m)
    // asi que no hay riesgo de que el frustum atraviese los muros al pegarse.
    // Static constexpr en el header para que tanto `updateCameras` como
    // `renderSceneToViewport` (en archivos separados) lo compartan.
    static constexpr glm::vec3 k_playerHalfExtents{0.3f, 0.9f, 0.3f};

    // Hito 30: character controller del player en Play Mode. 0 = no creado.
    // Se crea lazy en EditorPlayMode al entrar y se destruye al salir.
    u32 m_playerCharId = 0;
    f32 m_jumpCooldown = 0.0f;
    // Hito 34 C: coyote time + jump buffer. `m_coyoteTimer` cuenta hacia
    // 0 desde k_coyoteWindow cada vez que el char esta on-ground; permite
    // saltar hasta ~100ms despues de dejar el suelo. `m_jumpBufferTimer`
    // arranca en k_jumpBufferWindow al apretar Space; permite que un input
    // hecho hasta ~150ms ANTES de tocar el suelo igual gatille el salto.
    // `m_spacePrevFrame` es para detectar flanco up->down (no hold).
    f32 m_coyoteTimer = 0.0f;
    f32 m_jumpBufferTimer = 0.0f;
    bool m_spacePrevFrame = false;
    // F2H48: flanco up->down de tecla E para interactuar con NPCs/
    // triggers de dialog. Se actualiza en el frame loop antes del
    // DialogInteractSystem::tick.
    bool m_ePlayPrevFrame    = false;
    bool m_ePlayJustPressed  = false;
    // F2H52 H: prev-frame de Tab para detectar el flanco up->down y
    // togglear el widget `inventory_panel`.
    bool m_tabPrevFrame      = false;
    // F2H53 G: prev-frame de J para togglear `quest_log_panel`.
    bool m_jPrevFrame        = false;
    // F2H48: prev-frame state de las teclas 1..9 para detectar el digito
    // recien presionado durante un dialog activo. Indice 0..8 = teclas
    // 1..9 (`SDL_SCANCODE_1` + i).
    bool m_digitPrevFrame[9] = {false, false, false, false, false, false, false, false, false};
    bool m_crouching = false;
    // Hito 31 D: crouch lerp visual (1 = crouched, 0 = standing). El
    // shape de Jolt sigue siendo binario — solo la altura del eye se
    // interpola para que la transicion no salte de golpe.
    f32 m_crouchVisualT = 0.0f;
    // Headbob: time accumulator que avanza solo cuando el player se
    // mueve horizontalmente y esta on-ground. Se usa para sin(t*freq).
    f32 m_headbobTime = 0.0f;
    // Hito 34 D: velocidad horizontal del frame, normalizada a 0..1
    // contra k_walkSpeed. La consume el sync de la camara en
    // EditorScene::updateRigidBodies para escalar la amplitud del bob.
    f32 m_horizSpeed01 = 0.0f;

    // F2H67 Bloque F: estado de mount/dismount del player a un vehiculo.
    // - m_playerMountedVehicleEntity: raw entt::entity handle (u32) de la
    //   entity con VehicleComponent que el player esta conduciendo. 0 = on
    //   foot (default). Mientras !=0: WASD escribe input al vehicle en
    //   vez de moverse el char controller; la camara opera en chase-cam
    //   orbital alrededor del chassis.
    // - m_fEventPressed: latch del evento SDL_KEYDOWN de F (con repeat=0)
    //   capturado en processEvents. updateCameras lo consume y resetea.
    //   Se usa event-driven (no polling con prev-frame) para evitar perder
    //   el press si el frame del editor se demora y el usuario apreta+suelta
    //   F dentro de un tick — el repeat=0 del SDL event ya da edge.
    // - m_chaseDistance: distancia de la cam al chassis en chase mode. Se
    //   ajusta con scroll wheel (futuro polish). SA-default: 5 m atras.
    // - m_chaseHeightOffset: cuanto arriba mirar respecto al centro del
    //   chassis (apunta al techo del auto, no al piso). SA-feel: 1.5 m.
    u32 m_playerMountedVehicleEntity = 0;
    bool m_fEventPressed = false;
    f32 m_chaseDistance     = 5.0f;
    f32 m_chaseHeightOffset = 1.5f;

    // F2H70.2 fix S-key: edge-stick del modo brake-vs-reverse. Cuando S
    // transiciona released->pressed, decidimos UNA SOLA VEZ si entra como
    // "brake" (auto yendo adelante) o "reverse" (parado o yendo atras).
    // Mientras S siga pressed, mantenemos el modo aunque la speed cruce
    // el umbral — sin esto, soltar W + apretar S inmediatamente te tira
    // a reverse en cuanto el damping baja la speed debajo del umbral, lo
    // cual NO matchea ningun juego (GTA/Forza/etc.: S = freno hasta detenerse,
    // y solo despues de soltar+volver-a-apretar entra a reverse). Se
    // resetea al soltar S.
    bool m_sWasPressed  = false;  // edge tracking previo frame
    bool m_sBrakingMode = false;  // true=brake-mode (sticky), false=reverse-mode

    // Mapa jugable (Hito 4). Se renderiza centrado en el origen del mundo;
    // tileSize=3m (escala SI realista, Hito 5 Bloque 0). Se reemplaza al
    // abrir proyectos y se resetea al mapa de prueba al cerrar.
    GridMap m_map{8u, 8u, 3.0f};

    GizmoMode m_gizmoMode = GizmoMode::Translate;

    GizmoDragState m_gizmo;

    /// @brief Hito 27: al soltar el drag de un gizmo, captura el valor
    ///        final del Transform y empuja un EditTransformCommand al
    ///        history (si el delta no es trivial). Llamado desde el
    ///        overlay justo antes de resetear m_gizmo.active = false.
    void finalizeGizmoDrag();

    // Hito 27 Bloque 1: pila undo/redo de comandos del editor. Se vacia
    // en handleNewProject/handleOpenProject/handleCloseProject porque
    // los entity handles a los que apuntan los commands quedan invalidos.
    HistoryStack m_history;
    // True si el click del frame fue consumido por el gizmo; el loop
    // principal lo consulta para descartar el ClickSelect fantasma que
    // ViewportPanel emite si el gesto fue un micro-click.
    bool m_gizmoConsumedClick = false;

    // Escena ECS (Hito 7). Por ahora es una VISTA derivada de `m_map`:
    // cada tile solido es una entidad. El render sigue haciendo el loop
    // por el grid; Scene existe para que Hierarchy/Inspector trabajen
    // sobre entidades. La migracion a Scene-driven render viene en
    // hitos posteriores cuando haya geometria no-grid (assimp en Hito 10).
    std::unique_ptr<Scene> m_scene;

    // Proyecto activo (Hito 6 Bloque 4).
    // Cuando es nullopt (arranque sin recientes, o tras cerrar proyecto),
    // el editor muestra el modal Welcome y bloquea el resto de la UI hasta
    // que el usuario elija abrir o crear uno. Convencion Unity/Godot.
    std::optional<Project> m_project;

    // Proyectos recientes (max 8). El tope es el mas reciente.
    std::vector<std::filesystem::path> m_recentProjects;
    // Path del mapa activo, relativo a `m_project->root`.
    std::filesystem::path m_currentMapPath;
    // F2H57 Bloque D: handle de la entidad sobre la que esta abierto el
    // modal "Convertir entidad". `entt::null` cuando el modal no esta
    // activo. Latched cuando se consume el request del context menu.
    entt::entity m_convertModalActiveTarget{entt::null};
    // F2H57 followup: flag del modal "Elegir mesh del proyecto" estilo SFM.
    bool m_pickMeshModalActive = false;
    // True cuando se modifico el estado desde el ultimo save. El titulo de
    // la ventana muestra " *" en ese caso.
    bool m_projectDirty = false;

    // Toggle del debug draw de AABBs (tile colliders + AABB del jugador en
    // Play Mode). Controlado con F1. Default off para no ensuciar la escena.
    bool m_debugDraw = false;

    // Tile bajo el cursor en Editor Mode (recalculado cada frame via
    // ViewportPick). hit=false si no hay hover o el rayo no cae dentro del
    // mapa. Se usa para hover highlight y para drag & drop futuro.
    TilePickResult m_hoveredTile{};

    EditorUI m_ui;

    Timer m_deltaTimer;
    FpsCounter m_fpsCounter;

    // F3H25: autosave timer + escritura atómica al `.autosave/`. setup()
    // se llama en tryOpenProjectPath/handleNewProject; teardown() en
    // handleCloseProject. clearOnDisk() tras un handleSave manual.
    Autosave m_autosave;
    // F3H25: flag latched para mostrar el modal de recuperación en el
    // próximo frame. Lo seteamos al detectar lock huérfano + autosave
    // más reciente que el .moodmap canónico. Se consume en pumpUiRequests.
    bool m_recoveryModalPending = false;
    // F3H25: path al `.autosave/<map>.moodmap` que el modal ofrece restaurar.
    // Vacío si no hay recovery activa.
    std::filesystem::path m_recoveryAutosavePath;

    bool m_running = true;
};

} // namespace Mood
