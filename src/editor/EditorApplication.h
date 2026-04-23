#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "core/Types.h"
#include "editor/EditorMode.h"
#include "editor/EditorUI.h"
#include "engine/scene/EditorCamera.h"
#include "engine/scene/FpsCamera.h"
#include "engine/world/GridMap.h"
#include "platform/Window.h"

#include <memory>

namespace Mood {

class IRenderer;
class IFramebuffer;
class IShader;
class IMesh;
class ITexture;
class OpenGLDebugRenderer;

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

    std::unique_ptr<Window> m_window;

    // RHI y recursos graficos. Se destruyen en orden inverso antes del
    // contexto GL (ver destructor).
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<IFramebuffer> m_viewportFb;
    std::unique_ptr<IShader> m_defaultShader;
    std::unique_ptr<IMesh> m_cubeMesh;
    std::unique_ptr<ITexture> m_gridTexture;
    std::unique_ptr<OpenGLDebugRenderer> m_debugRenderer;

    // Escala SI: 1 unidad = 1 metro (ver DECISIONS.md). Mapa 8x8 con
    // tileSize=3 ocupa 24x24 m; diagonal ~34 m. Cam orbital con radius=30
    // deja el mapa entero en cuadro. Cam FPS en tile interior (2,6):
    // world = origen_mapa + ((2+0.5)*3, 1.6, (6+0.5)*3) = (-4.5, 1.6, 7.5).
    EditorCamera m_editorCamera{45.0f, 30.0f, 30.0f};
    FpsCamera m_playCamera{glm::vec3(-4.5f, 1.6f, 7.5f), -90.0f, 0.0f};
    EditorMode m_mode = EditorMode::Editor;

    // Mapa jugable (Hito 4). Se renderiza centrado en el origen del mundo;
    // tileSize=3m (escala SI realista, Hito 5 Bloque 0).
    GridMap m_map{8u, 8u, 3.0f};

    // Toggle del debug draw de AABBs (tile colliders + AABB del jugador en
    // Play Mode). Controlado con F1. Default off para no ensuciar la escena.
    bool m_debugDraw = false;

    EditorUI m_ui;

    Timer m_deltaTimer;
    FpsCounter m_fpsCounter;

    bool m_running = true;
};

} // namespace Mood
