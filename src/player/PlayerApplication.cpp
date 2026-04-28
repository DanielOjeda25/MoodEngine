#include "player/PlayerApplication.h"

#include "core/Log.h"
#include "platform/Window.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL.
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <stdexcept>

namespace Mood {

namespace {
constexpr const char* k_GlslVersion = "#version 450 core";
} // namespace

PlayerApplication::PlayerApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodPlayer iniciando...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodPlayer";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    if (!gladLoaderLoadGL()) {
        throw std::runtime_error("gladLoaderLoadGL fallo: no se pudo cargar OpenGL");
    }
    Log::render()->info("OpenGL iniciado: {}",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    // ImGui inicializado igual que en el editor — el HUD del juego usa
    // ImDrawList del Hito 20. La diferencia es que el player nunca crea
    // paneles (no hay ImGui::Begin con paneles del editor).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // sin layout persistente: no hay panels
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    MOOD_LOG_INFO("MoodPlayer listo");
}

PlayerApplication::~PlayerApplication() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    gladLoaderUnloadGL();

    m_window.reset();
    SDL_Quit();

    MOOD_LOG_INFO("MoodPlayer cerrado limpiamente");
    Log::shutdown();
}

void PlayerApplication::processEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) {
            m_running = false;
        } else if (ev.type == SDL_WINDOWEVENT &&
                   ev.window.event == SDL_WINDOWEVENT_CLOSE &&
                   ev.window.windowID == SDL_GetWindowID(m_window->sdlHandle())) {
            m_running = false;
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_ESCAPE) {
            // Hito 21 Bloque 1: Esc cierra el juego (sin menu de pausa
            // todavia). Cuando el Bloque 3 conecte la escena + GameState,
            // Esc togglea pausa como en el editor; el menu lo dibuja
            // drawGameOverlay reusado del Hito 20.
            m_running = false;
        }
    }
}

void PlayerApplication::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void PlayerApplication::endFrame() {
    ImGui::Render();

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Clear color violeta tenue para distinguir visualmente del editor
    // mientras MoodPlayer no tiene escena cargada (Bloque 1 stub).
    glClearColor(0.15f, 0.10f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    m_window->swapBuffers();
}

int PlayerApplication::run() {
    while (m_running) {
        processEvents();
        beginFrame();
        endFrame();
    }
    return 0;
}

} // namespace Mood
