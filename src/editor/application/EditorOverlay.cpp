// F2H24 Bloque C: nucleo del overlay 2D del editor.
// finalizeGizmoDrag — cierre del drag del gizmo (push del command de
//   undo agrupado MultiEdit o single).
// drawEditorOverlay — orquesta iconos + halos de seleccion + hotkeys
//   W/E/R/3/Esc/Period y delega a drawEditorOverlayGizmo (en
//   EditorOverlay_Gizmo.cpp) para los handles + drag-state.

#include "editor/application/EditorApplication.h"
#include "editor/application/EditorOverlay_Internal.h"

#include "core/Log.h"
#include "editor/commands/EditTransformCommand.h"
#include "editor/commands/MultiEditTransformCommand.h"  // F2H23 iter 5
#include "editor/selection/SelectionSet.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace Mood {

namespace {

// F2H23 iter 5: lee el campo del Transform por Field. Helper compartido
// entre el populate inicial del gizmo (capturar startValues) y el
// finalize (aplicar delta al final).
glm::vec3 readTransformField(const TransformComponent& tf,
                                EditTransformCommand::Field field) {
    switch (field) {
        case EditTransformCommand::Field::Position: return tf.position;
        case EditTransformCommand::Field::Rotation: return tf.rotationEuler;
        case EditTransformCommand::Field::Scale:    return tf.scale;
    }
    return glm::vec3(0.0f);
}

// F2H23 iter 5: aplica el field con clamp para Scale (que tiene rango
// valido 0.01-100). Otros fields aplicar directo.
void writeTransformField(TransformComponent& tf,
                          EditTransformCommand::Field field,
                          const glm::vec3& v) {
    switch (field) {
        case EditTransformCommand::Field::Position: tf.position = v; break;
        case EditTransformCommand::Field::Rotation: tf.rotationEuler = v; break;
        case EditTransformCommand::Field::Scale:
            tf.scale = glm::vec3(
                std::clamp(v.x, 0.01f, 100.0f),
                std::clamp(v.y, 0.01f, 100.0f),
                std::clamp(v.z, 0.01f, 100.0f));
            break;
    }
}

} // namespace

void EditorApplication::finalizeGizmoDrag() {
    if (!m_gizmo.entity) return;
    if (!m_scene || !m_scene->registry().valid(m_gizmo.entity.handle())) return;
    if (!m_gizmo.entity.hasComponent<TransformComponent>()) return;

    const auto field = static_cast<EditTransformCommand::Field>(m_gizmo.field);
    const auto& tfActive = m_gizmo.entity.getComponent<TransformComponent>();
    const glm::vec3 finalValueActive = readTransformField(tfActive, field);

    // F2H23 iter 5: si hay otherStarts (multi-edit), construimos un
    // MultiEditTransformCommand que opera sobre todas las entidades
    // (active + N otros). Ctrl+Z revierte el conjunto entero.
    // Si no hay otherStarts (single-edit), seguimos con el
    // EditTransformCommand simple.
    if (!m_gizmo.otherStarts.empty()) {
        std::vector<MultiEditTransformCommand::Entry> entries;
        entries.reserve(m_gizmo.otherStarts.size() + 1);

        // Active primero.
        entries.push_back({m_gizmo.entity, m_gizmo.startValue, finalValueActive});

        // Los demas: el delta es finalValueActive - startValueActive.
        // Apply ese delta a su propio startValue para obtener el "after".
        const glm::vec3 delta = finalValueActive - m_gizmo.startValue;
        for (const auto& os : m_gizmo.otherStarts) {
            if (!m_scene->registry().valid(os.entity.handle())) continue;
            if (!os.entity.hasComponent<TransformComponent>()) continue;
            glm::vec3 after = os.startValue + delta;
            if (field == EditTransformCommand::Field::Scale) {
                after = glm::vec3(
                    std::clamp(after.x, 0.01f, 100.0f),
                    std::clamp(after.y, 0.01f, 100.0f),
                    std::clamp(after.z, 0.01f, 100.0f));
            }
            entries.push_back({os.entity, os.startValue, after});
        }

        auto cmd = std::make_unique<MultiEditTransformCommand>(
            field, std::move(entries));
        if (cmd->isNoOp()) {
            // Nada movio realmente — limpiar state y salir.
            m_gizmo.entity = Entity{};
            m_gizmo.otherStarts.clear();
            return;
        }

        // Revertir TODOS los transforms al startValue antes del push:
        // execute() los va a re-aplicar. Sin esto el push hace doble-
        // aplicacion (mismo valor pero conceptualmente raro).
        Entity activeCopy = m_gizmo.entity;
        writeTransformField(activeCopy.getComponent<TransformComponent>(),
                              field, m_gizmo.startValue);
        for (const auto& os : m_gizmo.otherStarts) {
            if (!m_scene->registry().valid(os.entity.handle())) continue;
            if (!os.entity.hasComponent<TransformComponent>()) continue;
            Entity oCopy = os.entity;
            writeTransformField(oCopy.getComponent<TransformComponent>(),
                                  field, os.startValue);
        }

        Log::editor()->info(
            "[gizmo multi-edit] push MultiEditTransformCommand: {} entidades, "
            "field={}",
            m_gizmo.otherStarts.size() + 1u, static_cast<int>(field));

        m_history.push(std::move(cmd));
        m_gizmo.entity = Entity{};
        m_gizmo.otherStarts.clear();
        return;
    }

    // Single-edit (selectionSet.selected.size() == 1 al iniciar el drag).
    auto cmd = std::make_unique<EditTransformCommand>(
        m_gizmo.entity, field, m_gizmo.startValue, finalValueActive);
    if (cmd->isNoOp()) {
        m_gizmo.entity = Entity{};
        return;
    }

    // Revert al before antes del push (mismo razonamiento que el
    // MultiEdit path).
    Entity activeCopy = m_gizmo.entity;
    writeTransformField(activeCopy.getComponent<TransformComponent>(),
                          field, m_gizmo.startValue);
    m_history.push(std::move(cmd));
    m_gizmo.entity = Entity{};
}

void EditorApplication::drawEditorOverlay(ImDrawList* dl,
                                          float x0, float y0,
                                          float w, float h) {
    // Hito 13 Bloque 2.5 + 3: overlay 2D sobre el viewport en Editor Mode.
    // - Iconos para entidades sin mesh visible (Light/Audio).
    // - Halo de seleccion.
    // - Gizmo translate/rotate/scale + hotkeys W/E/R/Period.
    // - Delete/Backspace borra la entidad seleccionada (excepto tiles).
    const glm::mat4 vp = m_sceneRenderer->lastProjection() * m_sceneRenderer->lastView();

    const ImU32 colLight = IM_COL32(255, 225, 100, 220);
    const ImU32 colAudio = IM_COL32(100, 220, 230, 220);
    const ImU32 colBorder = IM_COL32(20, 20, 20, 255);
    const ImU32 colSel = IM_COL32(30, 180, 255, 255);

    Entity selected = m_ui.selectedEntity();
    const auto selectedHandle = selected ? selected.handle() : entt::entity{entt::null};

    // --- 1) Iconos + halo por entidad ---
    m_scene->forEach<TransformComponent>(
        [&](Entity e, TransformComponent& t) {
            const bool hasMesh = e.hasComponent<MeshRendererComponent>();
            const bool hasLight = e.hasComponent<LightComponent>();
            const bool hasAudio = e.hasComponent<AudioSourceComponent>();
            const bool needsIcon = (hasLight || hasAudio) && !hasMesh;
            const bool isSelected = (selectedHandle != entt::entity{entt::null})
                                     && (e.handle() == selectedHandle);
            if (!needsIcon && !isSelected) return;

            float sx, sy;
            if (!detail::projectWorldToScreen(vp, x0, y0, w, h, t.position, sx, sy)) return;

            if (needsIcon) {
                const ImVec2 c(sx, sy);
                if (hasLight) {
                    const auto& lc = e.getComponent<LightComponent>();
                    // Estilo inspirado en Blender: el icono distingue
                    // a simple vista entre Point (anillo con puntos)
                    // y Sun (centro con rayos). Los outlines negros
                    // mantienen legibilidad contra cielos claros.
                    constexpr f32 kTwoPi = 6.28318530718f;
                    if (lc.type == LightComponent::Type::Point) {
                        const f32 rOuter = 11.0f;
                        const f32 rDots  = 7.0f;
                        const f32 rCore  = 2.5f;
                        dl->AddCircle(c, rOuter, colLight, 24, 2.0f);
                        dl->AddCircle(c, rOuter, colBorder, 24, 0.8f);
                        for (int i = 0; i < 8; ++i) {
                            const f32 a = (f32)i * kTwoPi / 8.0f;
                            const ImVec2 p(c.x + std::cos(a) * rDots,
                                           c.y + std::sin(a) * rDots);
                            dl->AddCircleFilled(p, 1.6f, colLight, 8);
                        }
                        dl->AddCircleFilled(c, rCore, colLight, 12);
                    } else { // Directional / Sun
                        const f32 rCore     = 3.5f;
                        const f32 rayInner  = 6.5f;
                        const f32 rayOuter  = 11.0f;
                        dl->AddCircleFilled(c, rCore, colLight, 16);
                        dl->AddCircle(c, rCore, colBorder, 16, 0.8f);
                        for (int i = 0; i < 8; ++i) {
                            const f32 a = (f32)i * kTwoPi / 8.0f
                                        + kTwoPi / 16.0f; // offset 22.5
                            const ImVec2 p1(c.x + std::cos(a) * rayInner,
                                            c.y + std::sin(a) * rayInner);
                            const ImVec2 p2(c.x + std::cos(a) * rayOuter,
                                            c.y + std::sin(a) * rayOuter);
                            dl->AddLine(p1, p2, colBorder, 3.0f);
                            dl->AddLine(p1, p2, colLight,  1.8f);
                        }
                        // Linea de direccion: corta proyeccion del
                        // vector lc.direction desde la posicion de
                        // la entidad. Indica visualmente hacia donde
                        // apunta el sol sin necesidad de gizmo.
                        glm::vec3 dir = lc.direction;
                        if (glm::length(dir) > 1e-4f) {
                            dir = glm::normalize(dir);
                            const glm::vec3 endW = t.position + dir * 2.0f;
                            f32 ex, ey;
                            if (detail::projectWorldToScreen(vp, x0, y0, w, h, endW, ex, ey)) {
                                dl->AddLine(c, ImVec2(ex, ey), colBorder, 3.0f);
                                dl->AddLine(c, ImVec2(ex, ey), colLight,  1.5f);
                            }
                        }
                    }
                } else { // Audio
                    const f32 r = 10.0f;
                    dl->AddCircleFilled(c, r, colAudio, 24);
                    dl->AddCircle(c, r, colBorder, 24, 1.5f);
                }
            }

            if (isSelected) {
                const float r = needsIcon ? 14.0f : 12.0f;
                dl->AddCircle(ImVec2(sx, sy), r, colSel, 24, 2.0f);
            }
        });

    // --- 2) Hotkeys del gizmo + sub-mode + frame ---
    // Hotkeys W/E/R estilo Unity + `.` estilo Blender ("frame
    // selected"). Ignorar si ImGui esta capturando texto (p.ej. un
    // InputText del Inspector esta focuseado).
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_gizmoMode = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_gizmoMode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_gizmoMode = GizmoMode::Scale;
        // F2H17 + F2H30: teclas 1/2/3 toggle sub-modo Vertex/Edge/Face.
        // Esc vuelve a Object. ImGuiKey_N es la fila superior;
        // ImGuiKey_KeypadN es numpad. Aceptamos ambas para no fallar
        // segun el layout.
        auto toggleSubMode = [&](EditorSubMode target) {
            m_subMode = (m_subMode == target)
                ? EditorSubMode::Object
                : target;
            if (m_subMode == EditorSubMode::Object) {
                m_ui.selectionSet().activeFaceIndex = -1;
            }
            const char* label = "Object";
            switch (m_subMode) {
                case EditorSubMode::Vertex: label = "Vertex"; break;
                case EditorSubMode::Edge:   label = "Edge";   break;
                case EditorSubMode::Face:   label = "Face";   break;
                case EditorSubMode::Object: label = "Object"; break;
            }
            Log::editor()->info("Sub-mode: {}", label);
        };
        // F2H30: tecla 1 -> Vertex Mode.
        const bool key1 = ImGui::IsKeyPressed(ImGuiKey_1, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_Keypad1, false);
        if (key1) toggleSubMode(EditorSubMode::Vertex);
        // F2H30: tecla 2 -> Edge Mode.
        const bool key2 = ImGui::IsKeyPressed(ImGuiKey_2, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_Keypad2, false);
        if (key2) toggleSubMode(EditorSubMode::Edge);
        // F2H17: tecla 3 -> Face Mode.
        const bool key3 = ImGui::IsKeyPressed(ImGuiKey_3, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_Keypad3, false);
        if (key3) toggleSubMode(EditorSubMode::Face);
        // F2H30 Bloque C: tecla B toggle pincel poligonal. Solo
        // activo en workspace "Editor de mapas" (atajo seria
        // confuso en otros).
        if (ImGui::IsKeyPressed(ImGuiKey_B, false) &&
            m_ui.workspaceManager().activeWorkspace().name == "Editor de mapas") {
            togglePolygonDrawMode();
        }
        // F2H30 Bloque C: Enter cierra el polígono activo.
        if ((ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
             ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) &&
            m_polyDraw.active) {
            closePolygonDraw();
        }
        // Esc: si hay polígono en progreso, cancelarlo PRIMERO. Si no,
        // volver a Object mode.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (m_polyDraw.active) {
                cancelPolygonDraw();
            } else if (m_subMode != EditorSubMode::Object) {
                m_subMode = EditorSubMode::Object;
                m_ui.selectionSet().activeFaceIndex = -1;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Period, false) &&
            selected && selected.hasComponent<TransformComponent>()) {
            const auto& tf = selected.getComponent<TransformComponent>();
            // Bounding sphere approx: para entidades con mesh,
            // usar la diagonal del scale como radio (cubrir cubos
            // y meshes razonables). Para Light/Audio sin mesh,
            // un radio chico para que el zoom sea cercano sin
            // pegarse al icono.
            const f32 r = selected.hasComponent<MeshRendererComponent>()
                ? glm::length(tf.scale) * 0.6f
                : 1.0f;
            m_editorCamera.focusOn(tf.position, std::max(r, 0.3f));
        }
    }
    // La tecla Delete/Backspace se procesa via evento SDL en
    // `processEvents` (mas robusto que ImGui::IsKeyPressed porque
    // no depende del foco de teclado del panel actual).

    if (!selected || !selected.hasComponent<TransformComponent>()) return;

    auto& tform = selected.getComponent<TransformComponent>();
    const glm::vec3 worldOrigin = tform.position;
    f32 osx, osy;
    if (!detail::projectWorldToScreen(vp, x0, y0, w, h, worldOrigin, osx, osy)) return;

    // --- 3) Delegar al gizmo (en EditorOverlay_Gizmo.cpp) ---
    drawEditorOverlayGizmo(dl, x0, y0, w, h, vp, selected, osx, osy);
}

} // namespace Mood
