#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLMesh.h"
#include "engine/render/opengl/OpenGLRenderer.h"
#include "engine/render/opengl/OpenGLShader.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL
// (evita redefiniciones con <SDL_opengl.h> o los loaders internos).
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <stdexcept>
#include <vector>

namespace Mood {

namespace {

// Se pasa a imgui_impl_opengl3 al inicializarlo. Debe corresponder al perfil
// del contexto creado en Window (OpenGL 4.5 Core).
constexpr const char* k_GlslVersion = "#version 450 core";

} // namespace

EditorApplication::EditorApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodEngine iniciando...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodEngine Editor - v0.2.0 (Hito 2)";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    // --- GLAD: cargar punteros de OpenGL 4.5 Core ---
    // Window ya creo el contexto y lo hizo current, asi que podemos resolver
    // los punteros ahora.
    if (!gladLoaderLoadGL()) {
        throw std::runtime_error("gladLoaderLoadGL fallo: no se pudo cargar OpenGL");
    }
    Log::render()->info("OpenGL iniciado: {}",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log::render()->info("GPU: {} {}",
        reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    // --- Dear ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window->sdlHandle(), m_window->glContext())) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL fallo");
    }
    if (!ImGui_ImplOpenGL3_Init(k_GlslVersion)) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init fallo");
    }

    // --- RHI + recursos del viewport offscreen ---
    m_renderer = std::make_unique<OpenGLRenderer>();
    m_renderer->init();

    // Tamano inicial razonable: se redimensiona en el primer frame segun el
    // tamano real del panel Viewport.
    m_viewportFb = std::make_unique<OpenGLFramebuffer>(1280u, 720u);

    m_defaultShader = std::make_unique<OpenGLShader>(
        "shaders/default.vert", "shaders/default.frag");

    // Triangulo en NDC con color por vertice (rojo-verde-azul interpolados).
    const std::vector<f32> vertices = {
        //   pos (xyz)        color (rgb)
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   // abajo-izq, rojo
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   // abajo-der, verde
         0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f    // arriba,    azul
    };
    const std::vector<VertexAttribute> attrs = {
        { 0, 3 }, // aPos  -> location 0, vec3
        { 1, 3 }  // aColor -> location 1, vec3
    };
    m_triangleMesh = std::make_unique<OpenGLMesh>(vertices, attrs);

    // Conectar el framebuffer al panel Viewport para que lo muestre.
    m_ui.viewport().setFramebuffer(m_viewportFb.get());

    MOOD_LOG_INFO("Editor listo");
}

EditorApplication::~EditorApplication() {
    // Los recursos GL dependen del contexto; liberar ANTES de destruir ImGui
    // y el contexto (el destructor del Window destruye el contexto al final).
    m_triangleMesh.reset();
    m_defaultShader.reset();
    m_viewportFb.reset();
    m_renderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Liberar la tabla interna de punteros de GLAD antes de destruir el contexto.
    gladLoaderUnloadGL();

    m_window.reset();
    SDL_Quit();

    MOOD_LOG_INFO("MoodEngine cerrado limpiamente");
    Log::shutdown();
}

void EditorApplication::processEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        if (ev.type == SDL_QUIT) {
            m_running = false;
        } else if (ev.type == SDL_WINDOWEVENT &&
                   ev.window.event == SDL_WINDOWEVENT_CLOSE &&
                   ev.window.windowID == SDL_GetWindowID(m_window->sdlHandle())) {
            m_running = false;
        }
    }
}

void EditorApplication::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void EditorApplication::endFrame() {
    ImGui::Render();

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Gris oscuro de fondo del editor. ImGui dibuja encima en este mismo
    // framebuffer.
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    m_window->swapBuffers();
}

void EditorApplication::renderSceneToViewport() {
    // Sincronizar el tamano del FB con el del panel Viewport (lectura del
    // frame anterior). Ignorar si el panel todavia no se renderizo.
    const u32 desiredW = m_ui.viewport().desiredWidth();
    const u32 desiredH = m_ui.viewport().desiredHeight();
    if (desiredW > 0 && desiredH > 0) {
        m_viewportFb->resize(desiredW, desiredH);
    }

    m_viewportFb->bind();

    ClearValues clear{};
    // Azul oscuro para diferenciar del gris del editor.
    clear.color = {0.07f, 0.12f, 0.22f, 1.0f};
    m_renderer->beginFrame(clear);
    m_renderer->drawMesh(*m_triangleMesh, *m_defaultShader);
    m_renderer->endFrame();

    m_viewportFb->unbind();
}

int EditorApplication::run() {
    m_deltaTimer.reset();

    while (m_running) {
        const f64 dt = m_deltaTimer.elapsedSeconds();
        m_deltaTimer.reset();

        const f32 fps = m_fpsCounter.tick(dt);
        m_ui.setFps(fps);

        processEvents();

        bool requestQuit = false;
        beginFrame();

        // Render de la escena al framebuffer offscreen antes de que ImGui
        // muestre su textura en el panel Viewport.
        renderSceneToViewport();

        m_ui.draw(requestQuit);
        endFrame();

        if (requestQuit) {
            m_running = false;
        }
    }

    return 0;
}

} // namespace Mood
