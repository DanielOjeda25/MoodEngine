#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "editor/EditorMode.h"
#include "editor/EditorUI.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/Fog.h"
#include "engine/scene/EditorCamera.h"
#include "engine/scene/FpsCamera.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ViewportPick.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/world/GridMap.h"
#include "platform/Window.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace Mood {

class IFramebuffer;
class IMesh;
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

    /// @brief Hito 28: empaqueta una creacion (spawn / drop) como
    ///        `CreateEntityCommand` y la empuja al `m_history`. El
    ///        callsite ya creo las entidades; este helper captura el
    ///        snapshot para que Ctrl+Z las destruya. `markDirty()` se
    ///        invoca aca tambien para no duplicarlo en cada path.
    void pushCreatedEntities(std::vector<Entity> created, std::string label);
    void processSavePrefabRequest();
    void processViewportTextureDrop();
    void processViewportMeshDrop();
    void processViewportPrefabDrop();
    void processViewportMaterialDrop();
    void processViewportScriptDrop(); // Hito 22 Bloque 2

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

    std::unique_ptr<ScriptSystem> m_scriptSystem;
    std::unique_ptr<AudioDevice> m_audioDevice;
    std::unique_ptr<AudioSystem> m_audioSystem;
    std::unique_ptr<AnimationSystem> m_animationSystem; // Hito 19
    std::unique_ptr<NavSystem>       m_navSystem;       // Hito 23
    std::unique_ptr<ParticleSystem>  m_particleSystem;  // Hito 29

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
    EditorCamera m_editorCamera{45.0f, 30.0f, 30.0f};
    FpsCamera m_playCamera{glm::vec3(-4.5f, 1.6f, 7.5f), -90.0f, 0.0f};
    EditorMode m_mode = EditorMode::Editor;

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
