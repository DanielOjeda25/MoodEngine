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
#include "editor/panels/IPanel.h"  // F2H78: consumesSaveShortcut() en Ctrl+S contextual
#include "editor/panels/scene/OrthoViewportPanel.h"  // F2H44: Shift+wheel snap step
#include "editor/ui/DragDropFeedback.h"  // F3H17: cancelDragOnEscape
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
    std::string title = "MoodEngine Editor";
    if (m_project.has_value()) {
        title = "MoodEngine Editor - " + m_project->name;
        if (m_projectDirty) title += " *";
    }
    SDL_SetWindowTitle(m_window->sdlHandle(), title.c_str());
    m_ui.setHasProject(m_project.has_value());
    // F2H77: surfacear el "sin guardar" en la status bar (el " *" del titulo es
    // facil de no ver). Este es el unico punto de sync — updateWindowTitle() ya
    // se llama en cada transicion de m_projectDirty (markDirty / save / new /
    // open / close), asi que no hace falta polling por frame.
    m_ui.setProjectDirty(m_project.has_value() && m_projectDirty);
    // F3H1: mismo punto de sync sirve para el puntero al proyecto que
    // consumen panels editables (ProjectSettingsPanel). El address de un
    // `std::optional<T>::value()` es estable mientras el optional no
    // cambie de estado vacio↔no-vacio (lo cual SI ocurre aca; por eso el
    // re-sync en cada transicion).
    m_ui.setCurrentProject(m_project.has_value() ? &*m_project : nullptr);
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
                   ev.key.keysym.sym == SDLK_f &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Play &&
                   !ImGui::GetIO().WantTextInput) {
            // F2H67 polish: mount/dismount toggle. Latch consumido por
            // updateCameras. Edge detection via repeat==0 — mas robusto
            // que el polling con prev-frame anterior.
            m_fEventPressed = true;
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_s &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Ctrl+S contextual (F2H78): si un editor guardable (script /
            // shader / item / quest) tiene foco, el se guarda solo en su
            // render — NO disparamos ademas el guardado de proyecto. Si no
            // hay ninguno enfocado, Ctrl+S guarda proyecto+mapa como siempre.
            bool panelHandlesSave = false;
            for (const IPanel* p : m_ui.panels()) {
                if (p->visible && p->consumesSaveShortcut()) {
                    panelHandlesSave = true;
                    break;
                }
            }
            if (!panelHandlesSave) {
                m_ui.requestProjectAction(ProjectAction::Save);
            }
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_s &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) != 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // F2H85: Ctrl+Shift+S contextual ("Guardar como"). Gemelo del
            // Ctrl+S de F2H78: si un editor guardable con foco lo consume,
            // se guarda en una ruta nueva (pfd::save_file con su extension
            // nativa) en su propio render. Si nadie lo consume, fallback al
            // "Guardar proyecto como" tradicional.
            bool panelHandlesSaveAs = false;
            for (const IPanel* p : m_ui.panels()) {
                if (p->visible && p->consumesSaveAsShortcut()) {
                    panelHandlesSaveAs = true;
                    break;
                }
            }
            if (!panelHandlesSaveAs) {
                m_ui.requestProjectAction(ProjectAction::SaveAs);
            }
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_d &&
                   (ev.key.keysym.mod & KMOD_SHIFT) != 0 &&
                   (ev.key.keysym.mod & KMOD_CTRL) == 0 &&
                   (ev.key.keysym.mod & KMOD_ALT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // F2H85: Shift+D Blender-style duplicate de la(s) entidad(es)
            // del SelectionSet. Gate por: solo Editor Mode (no Play),
            // tecla sin Ctrl/Alt (Ctrl+D = undo/redo en otros editores,
            // Alt+D viewports), sin foco en input de texto (no robar la
            // 'D' a un campo). Convencion Blender — la copia aparece con
            // offset +0.5 m en X.
            duplicateSelectedEntities();
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
                   ev.key.keysym.sym == SDLK_g &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) != 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // F3H27: Shift+Ctrl+G des-agrupa selección. Convencion Blender.
            ungroupSelectedEntities();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_g &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor &&
                   !ImGui::GetIO().WantTextInput) {
            // F3H27: Ctrl+G agrupa selección bajo un Empty padre nuevo.
            // Convencion Blender (G de "group"). Sin selección o con 1
            // entity → no-op (logueado).
            groupSelectedEntities();
        } else if (ev.type == SDL_KEYDOWN &&
                   ev.key.keysym.sym == SDLK_z &&
                   (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                   (ev.key.keysym.mod & KMOD_SHIFT) == 0 &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Hito 27: Ctrl+Z deshace el ultimo comando del history.
            // F4H2 Bloque B fix iter2: gate completamente eliminado.
            // La gate previa (`!WantTextInput` y luego `!IsAnyItemActive`)
            // bloqueaba undo cuando un DragFloat del Inspector retenía
            // focus por un frame post-Enter. ImGui maneja su propio
            // Ctrl+Z LOCAL en text inputs (deshacer typing dentro del
            // campo) — no le afecta el handler global del editor.
            // Diagnóstico: log explícito de pre/post size para que el
            // dev vea si el HistoryStack está vacío vs si el undo no
            // tiene efecto visible.
            Log::editor()->info(
                "[ctrl+z] undo solicitado — historia: {} comandos antes (canUndo={})",
                m_history.undoCount(), m_history.canUndo());
            m_history.undo();
        } else if (ev.type == SDL_KEYDOWN &&
                   ((ev.key.keysym.sym == SDLK_y &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0) ||
                    (ev.key.keysym.sym == SDLK_z &&
                     (ev.key.keysym.mod & KMOD_CTRL) != 0 &&
                     (ev.key.keysym.mod & KMOD_SHIFT) != 0)) &&
                   ev.key.repeat == 0 &&
                   m_mode == EditorMode::Editor) {
            // Hito 27: Ctrl+Y o Ctrl+Shift+Z rehace.
            // F4H2 Bloque B fix iter2: gate eliminado (misma razón que undo).
            Log::editor()->info(
                "[ctrl+y/shift+z] redo solicitado — historia: {} comandos",
                m_history.redoCount());
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
                   !ImGui::GetIO().WantTextInput) {
            // Ciclar snap step. Dos contextos:
            //   - workspace "map_editor" => orto step (m_hammerSnapStep,
            //     int [1..128], F2H28).
            //   - cualquier otro workspace => grid step perspectivo
            //     (snapGridStep, f32 [0.125..4], F3H20).
            // Acepta Ctrl+= / Ctrl++ / Ctrl+ KP_PLUS / Ctrl+- / Ctrl+ KP_MINUS.
            // Ignora shift en la modifier mask para que Ctrl++ y Ctrl+=
            // entren por el mismo branch.
            const bool up = (ev.key.keysym.sym == SDLK_EQUALS ||
                              ev.key.keysym.sym == SDLK_PLUS  ||
                              ev.key.keysym.sym == SDLK_KP_PLUS);
            const bool inMapEditor = m_ui.workspaceManager().activeWorkspace().name
                                     == "map_editor";
            if (inMapEditor) {
                // F2H28 Bloque G + F3H6: array de steps leido live de
                // settings.snap.stepsAvailable (fallback a defaults sin proyecto).
                const SnapSettings k_snapCfg = m_project
                    ? m_project->settings.snap : SnapSettings{};
                const auto& k_steps = k_snapCfg.stepsAvailable;
                const int k_stepsCount = static_cast<int>(k_steps.size());
                int idx = k_snapCfg.defaultStepIndex;
                for (int i = 0; i < k_stepsCount; ++i) {
                    if (static_cast<u32>(k_steps[i]) == m_hammerSnapStep) { idx = i; break; }
                }
                if (up && idx + 1 < k_stepsCount) ++idx;
                if (!up && idx - 1 >= 0) --idx;
                if (k_stepsCount > 0) m_hammerSnapStep = static_cast<u32>(k_steps[idx]);
                Log::editor()->info("[hammer] snap step -> {}", m_hammerSnapStep);
            } else if (m_project) {
                // F3H20: cycle del grid step perspectivo. Lista fija
                // hardcoded (sub-meter granular). Sin proyecto -> no-op
                // (no hay donde persistir).
                constexpr f32 k_gridSteps[] = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
                constexpr int k_gridStepsCount = sizeof(k_gridSteps) / sizeof(f32);
                f32 cur = m_project->settings.snap.snapGridStep;
                int idx = 2;  // 0.5 default
                for (int i = 0; i < k_gridStepsCount; ++i) {
                    if (std::abs(k_gridSteps[i] - cur) < 1e-4f) { idx = i; break; }
                }
                if (up && idx + 1 < k_gridStepsCount) ++idx;
                if (!up && idx - 1 >= 0) --idx;
                m_project->settings.snap.snapGridStep = k_gridSteps[idx];
                markDirty();
                Log::editor()->info("[grid] snap step -> {:.3f}",
                                      m_project->settings.snap.snapGridStep);
            }
        }
        // F2H44 + F3H6 polish: Shift+ScrollWheel sobre cualquiera de los 3
        // ortho viewports cicla el snap step (atajo paralelo a Ctrl+= /
        // Ctrl+-, mas natural cuando ya tenes el mouse encima del viewport).
        // Solo en Editor Mode + workspace map_editor + cursor hovered en
        // alguno de los ortos. Mismo set de pasos.
        //
        // F3H6 polish: cambiamos Ctrl+Wheel -> Shift+Wheel porque los
        // trackpads (Windows Precision / Synaptics / Elan) inyectan KMOD_CTRL
        // automaticamente para soportar pinch-zoom en browsers. Con Ctrl
        // el atajo era inutilizable en notebook (cualquier wheel ciclaba el
        // grid). Shift no es simulado por ningun gesto de trackpad estandar.
        else if (ev.type == SDL_MOUSEWHEEL &&
                  ev.wheel.y != 0 &&
                  (SDL_GetModState() & KMOD_SHIFT) != 0 &&
                  m_mode == EditorMode::Editor &&
                  m_ui.workspaceManager().activeWorkspace().name == "map_editor" &&
                  (m_ui.orthoTop().liveCursor().hovered ||
                   m_ui.orthoFront().liveCursor().hovered ||
                   m_ui.orthoSide().liveCursor().hovered)) {
            // F3H6: mismo array que el branch de teclado — leido del proyecto.
            const SnapSettings k_snapCfg = m_project
                ? m_project->settings.snap : SnapSettings{};
            const auto& k_wheelSteps = k_snapCfg.stepsAvailable;
            const int k_wheelStepsCount = static_cast<int>(k_wheelSteps.size());
            int idx = k_snapCfg.defaultStepIndex;
            for (int i = 0; i < k_wheelStepsCount; ++i) {
                if (static_cast<u32>(k_wheelSteps[i]) == m_hammerSnapStep) { idx = i; break; }
            }
            const bool up = ev.wheel.y > 0;
            if (up && idx + 1 < k_wheelStepsCount) ++idx;
            if (!up && idx - 1 >= 0) --idx;
            if (k_wheelStepsCount > 0) m_hammerSnapStep = static_cast<u32>(k_wheelSteps[idx]);
            Log::editor()->info("[hammer] snap step -> {} (Shift+wheel)",
                                  m_hammerSnapStep);
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

    // F3H17: Esc cancela el drag&drop activo (si hay). Hook al inicio
    // del frame, antes de que cualquier BeginDragDropSource/Target del
    // widget tree dispare handlers.
    DragDropFeedback::cancelDragOnEscape();

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
    // y el input este capturado (no paused y no inventory abierto).
    // Cuando hay overlay UI activo (pause menu o inventory_panel),
    // se libera el cursor (SetRelativeMouseMode(FALSE)) y queremos
    // hover/click normales para los botones.
    //
    // F2H52 M-fix: el chequeo originalmente era `!paused()`; eso
    // rompia el inventory_panel porque lo forzaba off-screen aunque
    // el cursor estuviese liberado para hover items. Cambio a
    // `!isInputBlocked()` que incluye ambos casos (pause + inventory).
    if (m_mode == EditorMode::Play && !GameState::isInputBlocked()) {
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
