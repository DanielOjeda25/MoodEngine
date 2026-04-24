#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "editor/EditorMode.h"
#include "editor/EditorUI.h"
#include "engine/assets/AssetManager.h"
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
class IShader;
class IMesh;
class OpenGLDebugRenderer;
class GridRenderer;
class ScriptSystem;

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

    /// @brief Reconstruye `m_scene` desde cero a partir del estado actual de
    ///        `m_map`: una entidad por tile solido con Tag + Transform +
    ///        MeshRenderer. Se llama en cada modificacion del mapa (drop,
    ///        load, rebuild) para mantener ambos lados sincronizados.
    ///        Estrategia brute-force O(W*H) — suficiente para los tamanos
    ///        actuales (mapa 8x8 = ~30 entidades max). Optimizable cuando
    ///        cambie el modelo primario a Scene-driven.
    void rebuildSceneFromMap();

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
    std::unique_ptr<IFramebuffer> m_viewportFb;
    std::unique_ptr<IShader> m_defaultShader;
    std::unique_ptr<IMesh> m_cubeMesh;
    std::unique_ptr<OpenGLDebugRenderer> m_debugRenderer;
    std::unique_ptr<GridRenderer> m_gridRenderer;
    std::unique_ptr<ScriptSystem> m_scriptSystem;

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

    // Mapa jugable (Hito 4). Se renderiza centrado en el origen del mundo;
    // tileSize=3m (escala SI realista, Hito 5 Bloque 0). Se reemplaza al
    // abrir proyectos y se resetea al mapa de prueba al cerrar.
    GridMap m_map{8u, 8u, 3.0f};

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
