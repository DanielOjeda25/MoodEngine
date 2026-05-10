#pragma once

// Wrapper RAII sobre SDL_Window + contexto OpenGL. Abstrae la creacion y
// destruccion de la ventana y su contexto grafico. El resto del motor habla
// con esta clase, no con SDL directamente.

#include "core/Types.h"

#include <string>

struct SDL_Window;
using SDL_GLContext = void*;

namespace Mood {

struct WindowSpec {
    std::string title = "MoodEngine";
    u32 width = 1280;
    u32 height = 720;
    bool resizable = true;
    bool highDpi = true;
    /// @brief F2H35: si true, la ventana arranca maximizada (cubre toda
    ///        el area util del escritorio). Default false para preservar
    ///        comportamiento del Player. El Editor lo activa para que el
    ///        dev no tenga que maximizar manualmente cada arranque.
    bool maximized = false;
};

class Window {
public:
    /// @brief Crea la ventana SDL y el contexto OpenGL 4.5 Core asociado.
    ///        Lanza std::runtime_error si algo falla.
    explicit Window(const WindowSpec& spec);

    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /// @brief Intercambia los buffers de la ventana (presenta el frame).
    void swapBuffers();

    /// @brief F2H42: cambia el swap interval. true = VSync ON (60fps cap),
    ///        false = VSync OFF (uncapped). Si el driver no soporta el
    ///        modo pedido, se loggea warn y `vsyncEnabled()` queda como
    ///        estaba.
    void setVSync(bool enabled);

    /// @brief F2H42: estado actual de VSync (segun ultimo `setVSync` o
    ///        valor inicial del ctor).
    bool vsyncEnabled() const { return m_vsyncEnabled; }

    SDL_Window* sdlHandle() const { return m_window; }
    SDL_GLContext glContext() const { return m_glContext; }

    u32 width() const { return m_spec.width; }
    u32 height() const { return m_spec.height; }

private:
    WindowSpec m_spec{};
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_vsyncEnabled = true;  // F2H42: ON por defecto en el ctor
};

} // namespace Mood
