#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/PrimitiveMeshes.h"
#include "engine/render/ITexture.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLMesh.h"
#include "engine/render/opengl/OpenGLRenderer.h"
#include "engine/render/opengl/OpenGLShader.h"
#include "engine/render/opengl/OpenGLTexture.h"

// glad/gl.h debe ir antes que cualquier otro header que pueda incluir GL
// (evita redefiniciones con <SDL_opengl.h> o los loaders internos).
#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <stdexcept>

namespace Mood {

namespace {

constexpr const char* k_GlslVersion = "#version 450 core";

} // namespace

EditorApplication::EditorApplication() {
    Log::init();
    MOOD_LOG_INFO("MoodEngine iniciando...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init fallo: ") + SDL_GetError());
    }

    WindowSpec spec{};
    spec.title = "MoodEngine Editor - v0.3.0 (Hito 3)";
    spec.width = 1280;
    spec.height = 720;
    m_window = std::make_unique<Window>(spec);

    // --- GLAD ---
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

    m_viewportFb = std::make_unique<OpenGLFramebuffer>(1280u, 720u);

    m_defaultShader = std::make_unique<OpenGLShader>(
        "shaders/default.vert", "shaders/default.frag");

    MeshData cube = createCubeMesh();
    m_cubeMesh = std::make_unique<OpenGLMesh>(cube.vertices, cube.attributes);

    m_gridTexture = std::make_unique<OpenGLTexture>("assets/textures/grid.png");

    m_ui.viewport().setFramebuffer(m_viewportFb.get());

    MOOD_LOG_INFO("Editor listo");
}

EditorApplication::~EditorApplication() {
    // Los recursos GL dependen del contexto; liberar ANTES de destruir ImGui
    // y el contexto (el destructor del Window destruye el contexto al final).
    m_gridTexture.reset();
    m_cubeMesh.reset();
    m_defaultShader.reset();
    m_viewportFb.reset();
    m_renderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

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
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_ESCAPE &&
                   m_mode == EditorMode::Play) {
            exitPlayMode();
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

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    m_window->swapBuffers();
}

void EditorApplication::enterPlayMode() {
    m_mode = EditorMode::Play;
    m_ui.setMode(EditorMode::Play);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    // Descartar cualquier delta acumulado para evitar un salto inicial de la vista.
    SDL_GetRelativeMouseState(nullptr, nullptr);
    Log::editor()->info("Play Mode activo (WASD + mouse. Esc para salir)");
}

void EditorApplication::exitPlayMode() {
    m_mode = EditorMode::Editor;
    m_ui.setMode(EditorMode::Editor);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    Log::editor()->info("Editor Mode activo");
}

void EditorApplication::updateCameras(f32 dt) {
    if (m_mode == EditorMode::Editor) {
        const float dx = m_ui.viewport().cameraRotateDx();
        const float dy = m_ui.viewport().cameraRotateDy();
        const float wheel = m_ui.viewport().cameraWheel();
        if (dx != 0.0f || dy != 0.0f) m_editorCamera.applyMouseDrag(dx, dy);
        if (wheel != 0.0f) m_editorCamera.applyWheel(wheel);
    } else {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 dir(0.0f);
        if (keys[SDL_SCANCODE_W]) dir.z += 1.0f;
        if (keys[SDL_SCANCODE_S]) dir.z -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dir.x += 1.0f;
        if (keys[SDL_SCANCODE_A]) dir.x -= 1.0f;
        if (keys[SDL_SCANCODE_SPACE]) dir.y += 1.0f;
        if (keys[SDL_SCANCODE_LSHIFT]) dir.y -= 1.0f;
        m_playCamera.move(dir, dt);

        int mx = 0;
        int my = 0;
        SDL_GetRelativeMouseState(&mx, &my);
        if (mx != 0 || my != 0) {
            m_playCamera.applyMouseMove(static_cast<float>(mx), static_cast<float>(my));
        }
    }
}

void EditorApplication::renderSceneToViewport(f32 dt) {
    const u32 desiredW = m_ui.viewport().desiredWidth();
    const u32 desiredH = m_ui.viewport().desiredHeight();
    if (desiredW > 0 && desiredH > 0) {
        m_viewportFb->resize(desiredW, desiredH);
    }

    m_viewportFb->bind();

    ClearValues clear{};
    clear.color = {0.07f, 0.12f, 0.22f, 1.0f};
    m_renderer->beginFrame(clear);

    m_modelRotationRadians += dt * 0.6f;
    const glm::mat4 model = glm::rotate(
        glm::mat4(1.0f), m_modelRotationRadians, glm::vec3(0.3f, 1.0f, 0.2f));

    const float aspect = (m_viewportFb->height() > 0)
        ? static_cast<float>(m_viewportFb->width()) / static_cast<float>(m_viewportFb->height())
        : 1.0f;

    glm::mat4 view;
    glm::mat4 projection;
    if (m_mode == EditorMode::Play) {
        view = m_playCamera.viewMatrix();
        projection = m_playCamera.projectionMatrix(aspect);
    } else {
        view = m_editorCamera.viewMatrix();
        projection = m_editorCamera.projectionMatrix(aspect);
    }

    m_defaultShader->bind();
    m_defaultShader->setMat4("uModel", model);
    m_defaultShader->setMat4("uView", view);
    m_defaultShader->setMat4("uProjection", projection);
    m_defaultShader->setInt("uTexture", 0);

    m_gridTexture->bind(0);

    m_renderer->drawMesh(*m_cubeMesh, *m_defaultShader);
    m_renderer->endFrame();

    m_viewportFb->unbind();
}

int EditorApplication::run() {
    m_deltaTimer.reset();

    while (m_running) {
        const f64 dtD = m_deltaTimer.elapsedSeconds();
        const f32 dt = static_cast<f32>(dtD);
        m_deltaTimer.reset();

        const f32 fps = m_fpsCounter.tick(dtD);
        m_ui.setFps(fps);

        processEvents();
        beginFrame();

        // 1) Construir el widget tree de ImGui. ViewportPanel captura input
        //    de camara aqui (solo efectivo en Editor Mode).
        bool requestQuit = false;
        m_ui.draw(requestQuit);

        // 2) Atender toggles de modo solicitados desde la UI (boton Play/Stop).
        if (m_ui.consumeTogglePlayRequest()) {
            if (m_mode == EditorMode::Editor) {
                enterPlayMode();
            } else {
                exitPlayMode();
            }
        }

        // 3) Input -> camaras.
        updateCameras(dt);

        // 4) Render 3D al framebuffer offscreen (el panel mostrara la textura).
        renderSceneToViewport(dt);

        // 5) Draw final de ImGui.
        endFrame();

        if (requestQuit) {
            m_running = false;
        }
    }

    return 0;
}

} // namespace Mood
