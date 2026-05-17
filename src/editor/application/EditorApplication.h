#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "core/math/Plane.h"  // F2H30 Bloque B: snapshot pre/post de planos
#include "editor/application/EditorMode.h"
#include "editor/ui/EditorUI.h"
#include "editor/commands/HistoryStack.h"
#include "systems/physics/TriggerSystem.h"
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

    /// @brief Renderiza la escena al framebuffer offscreen que muestra el
    ///        panel Viewport.
    void renderSceneToViewport(f32 dt);

    /// @brief Actualiza la camara activa leyendo input del panel Viewport
    ///        (Editor Mode) o de SDL en relative mouse mode (Play Mode).
    void updateCameras(f32 dt);

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

    // Hito 16 refactor: handlers de spawn demos (menu Ayuda) y drops
    // del viewport. Implementaciones en `DemoSpawners.cpp`. Cada uno
    // hace su propio `consume*Request` y, si hay request pendiente,
    // crea/edita la entidad correspondiente. No-op si la condicion no
    // se cumple (sin scene, etc.).
    void processSpawnRotatorRequest();
    void processSpawnHudDemoRequest(); // Hito 20 Bloque 5
    void processSpawnEnemyDemoRequest(); // Hito 23 Bloque 3
    void processSpawnAudioSourceRequest();
    void processSpawnPointLightRequest();
    void processSpawnPhysicsBoxRequest();
    void processSpawnEnvironmentRequest();
    void processSpawnShadowDemoRequest();
    void processSpawnPbrSpheresRequest();
    void processSpawnLightStressRequest();
    void processSpawnAnimatedCharacterRequest(); // Hito 19
    void processSpawnFireParticlesRequest();     // Hito 29
    void processSpawnTriggerRequest();           // Hito 33
    void processSpawnDialogDemoRequest();        // F2H47
    void processGenerateNarrativeDemoMapRequest(); // F2H50 Bloque A
    void processSpawnStressTrisRequest();        // F2H2
    void processSpawnFullStressSceneRequest();   // F2H42

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

    std::unique_ptr<ScriptSystem> m_scriptSystem;
    std::unique_ptr<AudioDevice> m_audioDevice;
    std::unique_ptr<AudioSystem> m_audioSystem;
    std::unique_ptr<AnimationSystem> m_animationSystem; // Hito 19
    std::unique_ptr<NavSystem>       m_navSystem;       // Hito 23
    std::unique_ptr<ParticleSystem>  m_particleSystem;  // Hito 29
    TriggerSystem                    m_triggerSystem;   // Hito 33: stateless

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

    /// @brief Editor-side overlay 3D que se dibuja DENTRO del scene FB
    ///        entre `SceneRenderer::renderScene` y `endFrame`. Usa el
    ///        debug renderer del SceneRenderer para acumular: tile
    ///        picking + drag highlights (cyan/yellow), OBB de la
    ///        entidad seleccionada (naranja), F1 debug AABBs (tiles +
    ///        player). Implementado en `EditorSceneOverlay.cpp`.
    void drawEditorScene3DOverlay(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const glm::vec3& worldOrigin);

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

    /// @brief F2H28 Bloque G: snap step (en world units) de los viewports
    ///        ortograficos del workspace "Editor de mapas". Cycleable con
    ///        Ctrl + + / Ctrl + - en valores [1, 2, 4, 8, 16, 32, 64, 128].
    ///        Default 16 (convencion Hammer). Solo aplica cuando el
    ///        workspace activo es "Editor de mapas"; otros workspaces no
    ///        consumen el atajo.
    u32 m_hammerSnapStep = 16u;

    /// @brief F2H29 Bloque B: sesion de drag-edit en orto. Una sesion =
    ///        1 LMB-down sobre brush + drag (>4 px) + LMB-up. Captura
    ///        las posiciones iniciales de TODAS las entidades del
    ///        SelectionSet con TransformComponent al arrancar el drag,
    ///        para poder pushear un MultiEditTransformCommand con el
    ///        before/after preciso al cerrar. Solo 1 sesion activa a
    ///        la vez (un orto a la vez); los otros ortos se ignoran
    ///        mientras hay sesion activa.
    struct OrthoDragSession {
        bool active = false;
        int  orthoIdx = -1; // 0=Top, 1=Front, 2=Side
        std::vector<std::pair<Entity, glm::vec3>> startPositions;
    };
    OrthoDragSession m_orthoDragSession;

    /// @brief F2H29 Bloque C: sesion de block tool en orto. Activada
    ///        cuando el LMB-down inicia el drag en EMPTY space (sin
    ///        brush bajo el cursor). Durante el drag dibuja un AABB
    ///        cyan via debugRenderer (visible en perspectiva 3D); al
    ///        soltar, si el rectangulo supera `m_hammerSnapStep` en
    ///        ambos ejes del view, materializa un Box brush con esas
    ///        dimensiones + altura default (`snap * 4`) sobre el eje
    ///        perpendicular.
    struct OrthoBlockToolSession {
        bool active = false;
        int  orthoIdx = -1;
        // F2H29 Bloque C: AABB del preview cyan (en world space)
        // computado cada frame en el run loop. EditorRenderPass lo
        // re-encola en el debugRenderer antes de cada renderOrthoView
        // para que el preview sea visible en los 3 ortos + perspectiva.
        // valid=false antes del primer frame de drag (ds.ndcCur ==
        // ds.ndcStart -> AABB degenerado).
        glm::vec3 previewMin{0.0f};
        glm::vec3 previewMax{0.0f};
        bool      previewValid = false;
    };
    OrthoBlockToolSession m_orthoBlockSession;

    /// @brief F2H31 Bloque B: sesion de marquee select en orto. Activada
    ///        cuando el LMB-down inicia drag en EMPTY space + m_mapTool
    ///        == Select. Cada frame el panel reporta `dragState().ndcCur`
    ///        que se usa para dibujar el rectangulo amarillo (4 lineas
    ///        en world space sobre el plano del view, via debugRenderer).
    ///        Al soltar, hit-test cada entidad seleccionable: AABB world
    ///        proyectado a ndc; si CUALQUIER corner cae dentro del rect
    ///        del marquee, hit. Aplica modifiers Shift/Ctrl al SelectionSet.
    struct OrthoMarqueeSession {
        bool active = false;
        int  orthoIdx = -1;
    };
    OrthoMarqueeSession m_orthoMarquee;

    /// @brief F2H32 Bloque B: sesion del clip tool. Activa cuando
    ///        m_mapTool == Clip + el dev hace primer click en orto.
    ///        Captura p1; segundo click captura p2 -> plano definido.
    ///        Tecla T durante la sesion cycle keepMode. Enter confirma
    ///        (splitea brushes seleccionados); Esc cancela.
    struct ClipToolSession {
        bool active = false;
        int  orthoIdx = -1;  // -1 hasta el primer click
        bool hasP1 = false;
        bool hasP2 = false;
        glm::vec3 p1World{0.0f};
        glm::vec3 p2World{0.0f};
        ClipKeepMode keepMode = ClipKeepMode::Front;
    };
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

    /// @brief F2H30 Bloque B: sesion de vertex/edge edit en orto.
    ///        Activa cuando el LMB-down en Vertex/Edge sub-mode
    ///        impacta un vertex/edge del brush active. Captura los
    ///        planos pre + lista de planos a mutar (3 para vertex,
    ///        2 para edge). Cada frame aplica delta_world a los
    ///        planos via `d_new = d_old - dot(n, delta_local)` con
    ///        `delta_local = R^-1 * delta_world` (inverso de la
    ///        rotacion del worldMatrix). Al soltar pushea
    ///        EditBrushGeometryCommand.
    struct OrthoVertexEditSession {
        bool active = false;
        int  orthoIdx = -1;
        Entity brush;                    // entidad con BrushComponent
        std::string brushTag;             // para el command (robusto a remap)
        std::vector<Plane> planesBefore;  // snapshot de TODAS las caras
        std::vector<u32>   incidentPlanes; // que planos mutar
        glm::vec3 tfPosBefore{0.0f};      // snapshot del transform.position
        // F2H30 Bloque B: pivot inicial del vertex/edge en LOCAL space
        // del brush. Para Vertex es la posicion del vertex; para Edge
        // es el midpoint del edge. Usado para snap ABSOLUTO al grid:
        // pivotNew = round((pivotStart + delta_local) / snap) * snap.
        glm::vec3 pivotLocalStart{0.0f};
    };
    OrthoVertexEditSession m_orthoVertexEdit;

    /// @brief F2H30 Bloque C: sesion del "pincel poligonal". Activado
    ///        con tecla `B` desde el workspace "Editor de mapas".
    ///        Cada click LMB en una orto agrega un punto (snappeado
    ///        al grid) al polígono. Enter cierra (valida convex CCW
    ///        + crea prisma); Esc cancela. Locked a 1 sola orto
    ///        (la primera donde se clickea) para mantener coplanaridad.
    struct PolygonDrawSession {
        bool active = false;
        int  orthoIdx = -1;          // -1 hasta el primer click
        u32  axisIndex = 1;           // eje perpendicular (0=X, 1=Y, 2=Z)
        std::vector<glm::vec3> pointsWorld;
    };
    PolygonDrawSession m_polyDraw;

    /// @brief F2H30 Bloque C: activa el modo pincel (toggle). Si ya
    ///        habia una sesion en progreso, la cancela.
    void togglePolygonDrawMode();
    /// @brief Cierra el polígono (Enter): valida, spawnea brush.
    void closePolygonDraw();
    /// @brief Cancela la sesion (Esc) sin spawnear.
    void cancelPolygonDraw();

    /// @brief F2H30 Bloque D: state del modal G/R/S estilo Blender. El
    ///        dev presiona `G`/`R`/`S` con un brush selecto (y mouse
    ///        sobre el viewport perspectivo, fuera de un text input);
    ///        eso captura el snapshot del Transform de cada entidad
    ///        seleccionada y arranca un drag virtual. Mover el cursor
    ///        actualiza Position (G) / Rotation (R) / Scale (S) en
    ///        vivo respecto al pivote = centroide del SelectionSet.
    ///        Click izq confirma (push MultiEditTransformCommand);
    ///        Esc cancela (revert al startValue de cada entidad).
    ///        Tecla X/Y/Z durante el modal lockea/destrabea el axis
    ///        constraint (alineado con Blender). Convive con el gizmo
    ///        de flechas — ambos terminan llamando al mismo command.
    struct ModalShortcutEntry {
        Entity   entity;
        glm::vec3 startValue{0.0f};
    };
    struct ModalShortcutState {
        bool active = false;
        // Field se reusa de EditTransformCommand pero como int para
        // evitar pull del header desde EditorApplication.h. 0=Position,
        // 1=Rotation, 2=Scale. Mismo orden que el enum del command.
        int  field = 0;
        // axisLock: -1 = sin lock (libre), 0/1/2 = X/Y/Z.
        int  axisLock = -1;
        glm::vec2 mouseStart{0.0f};
        // Pivote en world. Para G es el delta del cursor en plano de
        // camara; para R/S es el centro respecto al cual rotamos/escalamos.
        glm::vec3 worldCenter{0.0f};
        std::vector<ModalShortcutEntry> entries;
    };
    ModalShortcutState m_modalShortcut;

    /// @brief F2H30 Bloque D iter 3: state para detectar double-tap de
    ///        E / R. Single-tap E -> GizmoMode::Scale; double-tap E ->
    ///        modal Scale uniforme. Single-tap R -> GizmoMode::Rotate;
    ///        double-tap R -> modal Rotate libre. Window 0.4s
    ///        (default Windows double-click time). Pedido del dev:
    ///        "si aprieto E una ves... si aprieto 2 veces que escale
    ///        uniformemente, asi nos desacemos de la S".
    struct GizmoKeyTapState {
        int   lastKey = -1;        // -1 = none, 'E', 'R'
        f32   lastPressTime = -1.0f;
    };
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

    // Mapa jugable (Hito 4). Se renderiza centrado en el origen del mundo;
    // tileSize=3m (escala SI realista, Hito 5 Bloque 0). Se reemplaza al
    // abrir proyectos y se resetea al mapa de prueba al cerrar.
    GridMap m_map{8u, 8u, 3.0f};

    // Modo del gizmo (Hito 13 Bloque 3-5). Cambia con W/E/R estilo Unity.
    enum class GizmoMode : u8 { Translate = 0, Rotate = 1, Scale = 2 };
    GizmoMode m_gizmoMode = GizmoMode::Translate;

    // Estado del drag del gizmo. Segun `mode`, `startValue` guarda:
    //   Translate: Transform.position inicial (vec3).
    //   Scale:     Transform.scale inicial (vec3).
    //   Rotate:    Transform.rotationEuler inicial (vec3).
    // `startParam` es el parametro/angulo inicial sobre el eje (o anillo,
    // en Rotate).
    struct GizmoDragState {
        bool active = false;
        int axis = -1;
        glm::vec3 startValue{0.0f};
        f32 startParam = 0.0f;
        // Hito 27: Field cual transform se esta editando (Position /
        // Rotation / Scale). Se setea al iniciar el drag y se usa al
        // soltar para construir el EditTransformCommand correcto.
        // 0 = Position, 1 = Rotation, 2 = Scale (matchea
        // EditTransformCommand::Field).
        u8 field = 0;
        // Entidad activa en el drag. Stack para que el undo funcione
        // aunque la seleccion cambie post-drag.
        Entity entity;
        // F2H23 polish iter 5: multi-edit del gizmo. Snapshot de las
        // entidades EXTRA del SelectionSet al iniciar el drag con sus
        // startValues del Field correspondiente. Cada frame del drag
        // aplica el mismo delta del active a todas. Vacio si solo hay
        // una entidad seleccionada (single-edit clasico).
        struct OtherStart {
            Entity entity;
            glm::vec3 startValue;
        };
        std::vector<OtherStart> otherStarts;
    };
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

    bool m_running = true;
};

} // namespace Mood
