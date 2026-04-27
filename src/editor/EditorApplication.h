#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "editor/EditorMode.h"
#include "editor/EditorUI.h"
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

class IRenderer;
class IFramebuffer;
class OpenGLFramebuffer;
class IShader;
class IMesh;
class ITexture;
class OpenGLCubemapTexture;
class OpenGLDebugRenderer;
class PostProcessPass;
enum class TonemapMode : i32;
class ScriptSystem;
class AudioDevice;
class AudioSystem;
class LightSystem;
class PhysicsWorld;
class SkyboxRenderer;
class ShadowPass;

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

    /// @brief Hito 15: resetea m_fog/m_exposure/m_tonemap a defaults y aplica
    ///        el primer EnvironmentComponent encontrado en `m_scene`. Si no
    ///        hay ninguno, quedan los defaults. Llamado por `renderSceneToViewport`
    ///        cada frame y por `tryOpenProjectPath` justo despues de cargar
    ///        las entidades — asi el primer render del proyecto nuevo ya
    ///        muestra los valores guardados sin un frame intermedio default.
    void applyEnvironmentFromScene();

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
    void processSpawnAudioSourceRequest();
    void processSpawnPointLightRequest();
    void processSpawnPhysicsBoxRequest();
    void processSpawnEnvironmentRequest();
    void processSpawnShadowDemoRequest();
    void processSpawnPbrSpheresRequest();
    void processSavePrefabRequest();
    void processViewportTextureDrop();
    void processViewportMeshDrop();
    void processViewportPrefabDrop();
    void processViewportMaterialDrop();

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

    // RHI y recursos graficos. Se destruyen en orden inverso antes del
    // contexto GL (ver destructor).
    std::unique_ptr<IRenderer> m_renderer;
    // Hito 15 Bloque 3: pipeline HDR. Sky + escena + lit + fog escriben a
    // `m_sceneFb` (RGBA16F). El post-process pass lo procesa con
    // exposicion + tonemap + gamma y deja el resultado en `m_viewportFb`
    // (RGBA8) que es lo que muestra el panel Viewport.
    std::unique_ptr<OpenGLFramebuffer> m_sceneFb;
    std::unique_ptr<OpenGLFramebuffer> m_viewportFb;
    std::unique_ptr<PostProcessPass> m_postProcess;
    f32 m_exposure = 0.0f;                      // EVs, [-5..+5] tipico
    TonemapMode m_tonemap = TonemapMode{2};     // ACES por default
    std::unique_ptr<IShader> m_defaultShader;
    std::unique_ptr<IShader> m_pbrShader; // Hito 17: reemplaza al lit Phong
    std::unique_ptr<OpenGLDebugRenderer> m_debugRenderer;
    std::unique_ptr<ScriptSystem> m_scriptSystem;
    std::unique_ptr<AudioDevice> m_audioDevice;
    std::unique_ptr<AudioSystem> m_audioSystem;
    std::unique_ptr<LightSystem> m_lightSystem;
    std::unique_ptr<PhysicsWorld> m_physicsWorld;
    std::unique_ptr<SkyboxRenderer> m_skyboxRenderer;
    std::unique_ptr<ShadowPass> m_shadowPass; // Hito 16

    // IBL (Hito 17 Bloque 3). Tres recursos:
    //   - irradiance cubemap (32x32): integral cosine-weighted del sky para
    //     el termino diffuse de la indirecta.
    //   - prefilter cubemap con mip chain (128 base, 5 niveles): aproxima
    //     el termino especular pre-integrado por roughness.
    //   - BRDF LUT 2D (256x256, RG channels): split-sum de Epic.
    // Tolera fallo de carga: el shader cae a `uAmbient` escalar si los
    // tres no estan disponibles (mismo comportamiento que pre-Hito 17).
    std::unique_ptr<OpenGLCubemapTexture> m_iblIrradiance;
    std::unique_ptr<OpenGLCubemapTexture> m_iblPrefilter;
    std::unique_ptr<ITexture>             m_iblBrdfLut;
    // Para loguear la primera vez que el shadow pass se activa / desactiva
    // (cambio de estado del directional light con castShadows). Se actualiza
    // dentro de `renderSceneToViewport`.
    bool m_shadowEnabledLastFrame = false;

    // Estado de render del frame actual. Lo regenera
    // `applyEnvironmentFromScene` cada frame (reset a defaults + override
    // con el primer EnvironmentComponent encontrado). Sin Environment los
    // valores quedan en sus defaults (fog Off, sin exposure, tonemap ACES).
    FogParams m_fog{};

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

    // Mapa jugable (Hito 4). Se renderiza centrado en el origen del mundo;
    // tileSize=3m (escala SI realista, Hito 5 Bloque 0). Se reemplaza al
    // abrir proyectos y se resetea al mapa de prueba al cerrar.
    GridMap m_map{8u, 8u, 3.0f};

    // Matrices activas del ultimo frame de render (Hito 13). Las guarda
    // `renderSceneToViewport`; las lee el overlay del ViewportPanel (iconos,
    // gizmos) con un frame de lag, imperceptible a 60fps.
    glm::mat4 m_lastView{1.0f};
    glm::mat4 m_lastProjection{1.0f};
    f32 m_lastAspect = 1.0f;

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
    };
    GizmoDragState m_gizmo;
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
