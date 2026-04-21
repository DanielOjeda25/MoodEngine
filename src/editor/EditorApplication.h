#pragma once

// Shell principal del editor. Gestiona el ciclo de vida: inicializa SDL,
// crea la ventana + contexto OpenGL, inicializa Dear ImGui con sus backends
// SDL2 y OpenGL3, corre el loop principal y cierra todo en orden inverso.

#include "core/Time.h"
#include "editor/EditorUI.h"
#include "platform/Window.h"

#include <memory>

namespace Mood {

class IRenderer;
class IFramebuffer;
class IShader;
class IMesh;

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
    ///        panel Viewport. Se llama por frame antes de que la UI dibuje.
    void renderSceneToViewport();

    std::unique_ptr<Window> m_window;

    // RHI y recursos graficos. Se destruyen en orden inverso antes del
    // contexto GL (ver destructor).
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<IFramebuffer> m_viewportFb;
    std::unique_ptr<IShader> m_defaultShader;
    std::unique_ptr<IMesh> m_triangleMesh;

    EditorUI m_ui;

    Timer m_deltaTimer;
    FpsCounter m_fpsCounter;

    bool m_running = true;
};

} // namespace Mood
