// F2H24: nucleo de EditorApplication. Lifecycle del editor partido en
// archivos parciales:
//   _Init.cpp  — glDebugCallback + ctor + dtor (boot + shutdown).
//   _Run.cpp   — run() loop principal con todos los dispatchers de
//                requests del UI + click-to-select + render pipeline.
//   EditorProjectActions{,_*}.cpp — handlers Archivo/Mapa/Brush/Boolean.
//   DemoSpawners{,_*}.cpp        — handlers Ayuda > Agregar X + drops.
//   EditorOverlay.cpp            — overlay 2D + gizmo logic.
//   EditorPlayMode.cpp           — enter/exit Play Mode + HUD.
//   EditorRenderPass.cpp         — renderSceneToViewport.
//   EditorScene.cpp              — buildInitialTestMap +
//                                   rebuildSceneFromMap + helpers.
//
// Aca solo viven los helpers compartidos por todos: window title /
// dirty flag, processEvents (dispatch de SDL events), beginFrame,
// endFrame, mapWorldOrigin (geometria del mapa), viewportAspect.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/game/state/GameState.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"

#include <glad/gl.h>

#include <SDL.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <cfloat>  // F2H41 fix: FLT_MAX para neutralizar mouse pos en Play

namespace Mood {

void EditorApplication::updateWindowTitle() {
    std::string title = "MoodEngine Editor - v0.6.0-dev (Hito 6)";
    if (m_project.has_value()) {
        title = "MoodEngine Editor - " + m_project->name;
        if (m_projectDirty) title += " *";
    }
    SDL_SetWindowTitle(m_window->sdlHandle(), title.c_str());
    m_ui.setHasProject(m_project.has_value());
}

void EditorApplication::markDirty() {
    if (!m_project.has_value()) return;
    if (!m_projectDirty) {
        m_projectDirty = true;
        updateWindowTitle();
    }
}

void EditorApplication::processEvents() {
    MOOD_PROFILE_FUNCTION();
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
            // Hito 20: Esc togglea menu de pausa. La sincronizacion del
            // cursor con SDL la hace `updateCameras` detectando la
            // transicion del flag (asi tambien funciona si lo cambia un
            // script Lua via `hud.setPaused`).
            GameState::paused() = !GameState::paused();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_F1 &&
                   ev.key.repeat == 0) {
            m_debugDraw = !m_debugDraw;
            Log::editor()->info("Debug draw {}", m_debugDraw ? "activado" : "desactivado");
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_s &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Ctrl+S: atajo de Guardar. Emitimos la misma solicitud que el menu
            // para reusar el dispatcher unico.
            m_ui.requestProjectAction(ProjectAction::Save);
        } else if (ev.type == SDL_KEYDOWN &&
                   (ev.key.keysym.sym == SDLK_DELETE ||
                    ev.key.keysym.sym == SDLK_BACKSPACE) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 13/20: Delete/Backspace borra la entidad seleccionada.
            // Via evento SDL en lugar de ImGui::IsKeyPressed para evitar
            // problemas de foco entre paneles. El filtro WantTextInput
            // evita borrar la entidad mientras el usuario edita un campo.
            deleteSelectedEntity();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_z &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 27: Ctrl+Z deshace el ultimo comando del history.
            m_history.undo();
        } else if (ev.type == SDL_KEYDOWN &&
                   ((ev.key.keysym.sym == SDLK_y &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0) ||
                    (ev.key.keysym.sym == SDLK_z &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                     (ev.key.keysym.mod & KMOD_SHIFT) != 0)) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // Hito 27: Ctrl+Y o Ctrl+Shift+Z rehace.
            m_history.redo();
        } else if (ev.type == SDL_KEYDOWN &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.sym == SDLK_EQUALS ||
                    ev.key.keysym.sym == SDLK_PLUS   ||
                    ev.key.keysym.sym == SDLK_KP_PLUS ||
                    ev.key.keysym.sym == SDLK_MINUS  ||
                    ev.key.keysym.sym == SDLK_KP_MINUS) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   m_ui.workspaceManager().activeWorkspace().name
                       == "Editor de mapas" &&
                   !ImGui::GetIO().WantTextInput) {
            // F2H28 Bloque G: ciclar snap step del workspace orto.
            // Acepta:
            //   Ctrl+= (US/UK: la tecla fisica es '='; con shift produce '+')
            //   Ctrl++ via SDLK_PLUS (layout ES: tecla a la derecha de Ñ
            //                         da '+' SIN shift — F2H33 fix bug
            //                         reportado por el dev en teclado 80%
            //                         sin numerico).
            //   Ctrl+ KP_PLUS  (numerico, opcional en 80%).
            //   Ctrl+- y Ctrl+ KP_MINUS para reducir.
            // Ignora shift en la modifier mask para que Ctrl++ y Ctrl+=
            // entren por el mismo branch.
            static constexpr u32 k_steps[] = {1u, 2u, 4u, 8u, 16u,
                                                32u, 64u, 128u};
            constexpr int k_stepsCount =
                static_cast<int>(sizeof(k_steps) / sizeof(k_steps[0]));
            int idx = 4; // default 16
            for (int i = 0; i < k_stepsCount; ++i) {
                if (k_steps[i] == m_hammerSnapStep) { idx = i; break; }
            }
            const bool up = (ev.key.keysym.sym == SDLK_EQUALS ||
                              ev.key.keysym.sym == SDLK_PLUS  ||
                              ev.key.keysym.sym == SDLK_KP_PLUS);
            if (up && idx + 1 < k_stepsCount) ++idx;
            if (!up && idx - 1 >= 0) --idx;
            m_hammerSnapStep = k_steps[idx];
            Log::editor()->info("[hammer] snap step -> {}", m_hammerSnapStep);
        }
    }
}

void EditorApplication::beginFrame() {
    MOOD_PROFILE_FUNCTION();
    // F2H7: aplicar pending workspace switch ANTES de NewFrame.
    // LoadIniSettingsFromMemory no debe llamarse dentro de un frame
    // ImGui activo (segun la doc de ImGui).
    m_ui.applyPendingWorkspaceSwitch();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // F2H41 fix lateral: en Play Mode el cursor SDL esta capturado
    // (relative mouse mode) — el FpsCamera lee deltas via
    // SDL_GetRelativeMouseState directo. Pero ImGui sigue viendo
    // la pos OS del cursor (warpeada al centro de la ventana al
    // entrar a relative mode), lo que hace que los panels que
    // queden bajo del cursor virtual muestren entries como
    // hovered/selected (Hierarchy, Inspector, AssetBrowser, etc.).
    // El dev rotaba la cam y "veia" entries seleccionarse en la
    // escena.
    //
    // Fix: forzar MousePos off-screen mientras Play Mode activo
    // y NO paused. Cuando paused, el cursor se libera (Esc dispara
    // SDL_SetRelativeMouseMode(SDL_FALSE)) y queremos hover/click
    // normales para los botones del pause menu.
    if (m_mode == EditorMode::Play && !GameState::paused()) {
        ImGuiIO& io = ImGui::GetIO();
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    }
}

void EditorApplication::endFrame() {
    MOOD_PROFILE_FUNCTION();
    {
        MOOD_PROFILE_SCOPE("ImGui::Render");
        ImGui::Render();
    }

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GL_GetDrawableSize(m_window->sdlHandle(), &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        MOOD_PROFILE_SCOPE("ImGui_ImplOpenGL3_RenderDrawData");
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    {
        MOOD_PROFILE_SCOPE("swapBuffers");
        m_window->swapBuffers();
    }
}

glm::vec3 EditorApplication::mapWorldOrigin() const {
    return glm::vec3(
        -0.5f * static_cast<f32>(m_map.width())  * m_map.tileSize(),
        0.0f,
        -0.5f * static_cast<f32>(m_map.height()) * m_map.tileSize()
    );
}

f32 EditorApplication::viewportAspect() const {
    const auto& fb = m_sceneRenderer->viewportFb();
    return (fb.height() > 0)
        ? static_cast<f32>(fb.width()) / static_cast<f32>(fb.height())
        : 1.0f;
}

} // namespace Mood
