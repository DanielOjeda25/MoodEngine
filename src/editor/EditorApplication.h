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
#include "platform/Window.h"

#include <memory>

namespace Mood {

class IRenderer;
class IFramebuffer;
class IShader;
class IMesh;
class ITexture;

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

    std::unique_ptr<Window> m_window;

    // RHI y recursos graficos. Se destruyen en orden inverso antes del
    // contexto GL (ver destructor).
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<IFramebuffer> m_viewportFb;
    std::unique_ptr<IShader> m_defaultShader;
    std::unique_ptr<IMesh> m_cubeMesh;
    std::unique_ptr<ITexture> m_gridTexture;

    EditorCamera m_editorCamera;
    FpsCamera m_playCamera;
    EditorMode m_mode = EditorMode::Editor;
    f32 m_modelRotationRadians = 0.0f;

    EditorUI m_ui;

    Timer m_deltaTimer;
    FpsCounter m_fpsCounter;

    bool m_running = true;
};

} // namespace Mood
