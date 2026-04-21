#include "platform/Window.h"

#include "core/Log.h"

#include <SDL.h>

#include <stdexcept>

namespace Mood {

Window::Window(const WindowSpec& spec) : m_spec(spec) {
    // Contexto OpenGL 4.5 Core Profile (ver docs/DECISIONS.md).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL;
    if (m_spec.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (m_spec.highDpi)   flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    m_window = SDL_CreateWindow(
        m_spec.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(m_spec.width), static_cast<int>(m_spec.height),
        flags);

    if (!m_window) {
        throw std::runtime_error(std::string("SDL_CreateWindow fallo: ") + SDL_GetError());
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        const std::string err = SDL_GetError();
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        throw std::runtime_error("SDL_GL_CreateContext fallo: " + err);
    }

    // VSync activado por defecto; si falla no es fatal.
    if (SDL_GL_SetSwapInterval(1) != 0) {
        MOOD_LOG_WARN("VSync no disponible: {}", SDL_GetError());
    }

    MOOD_LOG_INFO("Ventana creada ({}x{})", m_spec.width, m_spec.height);
}

Window::~Window() {
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void Window::swapBuffers() {
    SDL_GL_SwapWindow(m_window);
}

} // namespace Mood
