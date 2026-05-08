#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/EditTransformCommand.h"
#include "editor/selection/SelectionSet.h"  // F2H23: multi-edit del gizmo
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/components/BrushComponent.h"  // F2H14: rotate/scale gizmo
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <algorithm>  // F2H23: std::clamp para multi-edit scale
#include <cmath>
#include <memory>

namespace Mood {

void EditorApplication::finalizeGizmoDrag() {
    if (!m_gizmo.entity) return;
    if (!m_scene || !m_scene->registry().valid(m_gizmo.entity.handle())) return;
    if (!m_gizmo.entity.hasComponent<TransformComponent>()) return;

    const auto& tf = m_gizmo.entity.getComponent<TransformComponent>();
    const auto field = static_cast<EditTransformCommand::Field>(m_gizmo.field);
    glm::vec3 finalValue;
    switch (field) {
        case EditTransformCommand::Field::Position: finalValue = tf.position; break;
        case EditTransformCommand::Field::Rotation: finalValue = tf.rotationEuler; break;
        case EditTransformCommand::Field::Scale:    finalValue = tf.scale; break;
    }

    // Push solo si hubo movimiento real. Click sin drag (param tocado pero
    // sin desplazar el cursor) caeria como no-op.
    auto cmd = std::make_unique<EditTransformCommand>(
        m_gizmo.entity, field, m_gizmo.startValue, finalValue);
    if (cmd->isNoOp()) return;

    // F2H23: multi-edit del gizmo. Si hay >1 entidad seleccionada, el
    // mismo delta se aplica a las demas (estilo Maya / Blender). MVP:
    // las "demas" NO entran al HistoryStack — solo el active. Ctrl+Z
    // solo revierte la activa; las demas quedan con el delta aplicado.
    // CompoundCommand para undo agrupado = deuda en PENDIENTES.md.
    const glm::vec3 delta = finalValue - m_gizmo.startValue;
    const SelectionSet& set = m_ui.selectionSet();
    if (set.selected.size() > 1u) {
        u32 affected = 0;
        for (const Entity& other : set.selected) {
            if (other.handle() == m_gizmo.entity.handle()) continue;
            Entity oCopy = other;
            if (!m_scene->registry().valid(oCopy.handle())) continue;
            if (!oCopy.hasComponent<TransformComponent>()) continue;
            auto& ot = oCopy.getComponent<TransformComponent>();
            switch (field) {
                case EditTransformCommand::Field::Position:
                    ot.position += delta; break;
                case EditTransformCommand::Field::Rotation:
                    ot.rotationEuler += delta; break;
                case EditTransformCommand::Field::Scale: {
                    // Scale: clamp para no caer fuera del rango valido
                    // (mismos limites que el DragFloat3 del Inspector).
                    glm::vec3 v = ot.scale + delta;
                    ot.scale = glm::vec3(
                        std::clamp(v.x, 0.01f, 100.0f),
                        std::clamp(v.y, 0.01f, 100.0f),
                        std::clamp(v.z, 0.01f, 100.0f));
                    break;
                }
            }
            ++affected;
        }
        if (affected > 0u) {
            const char* fieldName = "?";
            switch (field) {
                case EditTransformCommand::Field::Position: fieldName = "position"; break;
                case EditTransformCommand::Field::Rotation: fieldName = "rotation"; break;
                case EditTransformCommand::Field::Scale:    fieldName = "scale";    break;
            }
            Log::editor()->info(
                "[gizmo multi-edit] {} delta=({:.3f},{:.3f},{:.3f}) "
                "aplicado a {} entidad(es) extra (active via gizmo, sin undo "
                "individual de las demas)",
                fieldName, delta.x, delta.y, delta.z, affected);
        }
    }

    // Importante: el HistoryStack::push llama a execute() — pero el valor
    // ya esta aplicado en el Transform por el drag. Eso causaria una
    // doble-aplicacion (re-asigna el mismo finalValue, no-op visualmente
    // pero conceptualmente raro). Para evitarlo: revertimos al before
    // antes de pushear, asi push() ejecuta y deja el estado correcto.
    switch (field) {
        case EditTransformCommand::Field::Position:
            m_gizmo.entity.getComponent<TransformComponent>().position = m_gizmo.startValue; break;
        case EditTransformCommand::Field::Rotation:
            m_gizmo.entity.getComponent<TransformComponent>().rotationEuler = m_gizmo.startValue; break;
        case EditTransformCommand::Field::Scale:
            m_gizmo.entity.getComponent<TransformComponent>().scale = m_gizmo.startValue; break;
    }
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

    // Helper: worldPos -> pixel-space.
    auto project = [&](const glm::vec3& p, float& sx, float& sy) -> bool {
        const glm::vec4 clip = vp * glm::vec4(p, 1.0f);
        if (clip.w <= 1e-4f) return false;
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        const float ndcZ = clip.z / clip.w;
        if (ndcZ < -1.0f || ndcZ > 1.0f) return false;
        sx = x0 + (ndcX * 0.5f + 0.5f) * w;
        sy = y0 + (1.0f - (ndcY * 0.5f + 0.5f)) * h;
        return true;
    };
    // Helper: mouse screen -> camera ray (origen + direccion).
    auto cameraRay = [&](ImVec2 mp, glm::vec3& rorigin, glm::vec3& rdir) -> bool {
        const float ndcX = ((mp.x - x0) / w) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((mp.y - y0) / h) * 2.0f;
        const glm::mat4 invVP = glm::inverse(vp);
        const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        const glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY, +1.0f, 1.0f);
        if (nearH.w == 0.0f || farH.w == 0.0f) return false;
        rorigin = glm::vec3(nearH) / nearH.w;
        const glm::vec3 farW = glm::vec3(farH) / farH.w;
        const glm::vec3 d = farW - rorigin;
        const float len = glm::length(d);
        if (len < 1e-6f) return false;
        rdir = d / len;
        return true;
    };
    // Parametro del punto de lineA (pA + dA*t) mas cercano al rayo
    // lineB (pB + dB*t). dA y dB deben ser unitarios.
    auto closestParam = [](const glm::vec3& pA, const glm::vec3& dA,
                            const glm::vec3& pB, const glm::vec3& dB) -> f32 {
        const glm::vec3 n = pA - pB;
        const f32 b  = glm::dot(dA, dB);
        const f32 d1 = glm::dot(dA, n);
        const f32 d2 = glm::dot(dB, n);
        const f32 denom = 1.0f - b * b;
        if (std::abs(denom) < 1e-5f) return 0.0f; // paralelas
        return (b * d2 - d1) / denom;
    };

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
            if (!project(t.position, sx, sy)) return;

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
                            if (project(endW, ex, ey)) {
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

    // --- 2) Gizmo (translate / rotate / scale) para la seleccion ---

    // Hotkeys W/E/R estilo Unity + `.` estilo Blender ("frame
    // selected"). Ignorar si ImGui esta capturando texto (p.ej. un
    // InputText del Inspector esta focuseado).
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_gizmoMode = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) m_gizmoMode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_gizmoMode = GizmoMode::Scale;
        // F2H17: tecla 3 toggle Face Mode. Esc vuelve a Object.
        // ImGuiKey_3 es la fila superior; ImGuiKey_Keypad3 es numpad.
        // Aceptamos ambas para no fallar segun el layout.
        const bool key3 = ImGui::IsKeyPressed(ImGuiKey_3, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_Keypad3, false);
        if (key3) {
            m_subMode = (m_subMode == EditorSubMode::Face)
                ? EditorSubMode::Object
                : EditorSubMode::Face;
            if (m_subMode == EditorSubMode::Object) {
                m_ui.selectionSet().activeFaceIndex = -1;
            }
            Log::editor()->info("Sub-mode: {}",
                m_subMode == EditorSubMode::Face ? "Face" : "Object");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
            m_subMode != EditorSubMode::Object) {
            m_subMode = EditorSubMode::Object;
            m_ui.selectionSet().activeFaceIndex = -1;
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
    // F2H14: tanto MeshRenderer como BrushComponent tienen geometria
    // visible que se beneficia de rotate/scale. Light/Audio puros
    // (sin mesh ni brush) caen a Translate.
    const bool hasGeometry =
        selected.hasComponent<MeshRendererComponent>() ||
        selected.hasComponent<BrushComponent>();

    GizmoMode effectiveMode = m_gizmoMode;
    if (!hasGeometry && effectiveMode != GizmoMode::Translate) {
        effectiveMode = GizmoMode::Translate;
    }

    const glm::vec3 worldOrigin = tform.position;
    f32 osx, osy;
    if (!project(worldOrigin, osx, osy)) return;

    const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const ImU32 axisCol[3] = {
        IM_COL32(230, 50, 50, 230),   // X rojo
        IM_COL32(80, 230, 80, 230),   // Y verde
        IM_COL32(70, 120, 255, 230),  // Z azul
    };
    const ImU32 hoverCol = IM_COL32(255, 220, 30, 255);
    const f32 k_armLen = 60.0f;
    const f32 k_ringRad = 55.0f;

    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool mouseDown    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool insideViewport = mousePos.x >= x0 && mousePos.x <= x0 + w
                             && mousePos.y >= y0 && mousePos.y <= y0 + h;

    // ====== MODO TRANSLATE / SCALE (ambos usan handles por eje) ======
    if (effectiveMode == GizmoMode::Translate ||
        effectiveMode == GizmoMode::Scale) {

        f32 screenDX[3] = {0, 0, 0};
        f32 screenDY[3] = {0, 0, 0};
        f32 endSX[3] = {osx, osx, osx};
        f32 endSY[3] = {osy, osy, osy};
        bool axisVisible[3] = {false, false, false};
        for (int i = 0; i < 3; ++i) {
            f32 esx, esy;
            if (!project(worldOrigin + axes[i] * 1.0f, esx, esy)) continue;
            const f32 dx = esx - osx;
            const f32 dy = esy - osy;
            const f32 lenPx = std::sqrt(dx * dx + dy * dy);
            if (lenPx < 1e-3f) continue;
            screenDX[i] = dx / lenPx;
            screenDY[i] = dy / lenPx;
            endSX[i] = osx + screenDX[i] * k_armLen;
            endSY[i] = osy + screenDY[i] * k_armLen;
            axisVisible[i] = true;
        }

        int hoverAxis = -1;
        if (!m_gizmo.active) {
            // En modo Scale, el handle central (axis=3) escala
            // uniformemente. Hit-test primero (gana al per-axis si
            // el mouse esta encima del centro).
            if (effectiveMode == GizmoMode::Scale) {
                const f32 dx = mousePos.x - osx;
                const f32 dy = mousePos.y - osy;
                if (std::sqrt(dx * dx + dy * dy) < 8.0f) {
                    hoverAxis = 3;
                }
            }
            if (hoverAxis < 0) {
                f32 bestDist = 7.0f;
                for (int i = 0; i < 3; ++i) {
                    if (!axisVisible[i]) continue;
                    const f32 dx = endSX[i] - osx;
                    const f32 dy = endSY[i] - osy;
                    const f32 len2 = dx * dx + dy * dy;
                    if (len2 < 1.0f) continue;
                    const f32 px = mousePos.x - osx;
                    const f32 py = mousePos.y - osy;
                    const f32 t = std::max(0.0f, std::min(1.0f, (px * dx + py * dy) / len2));
                    const f32 qx = osx + t * dx - mousePos.x;
                    const f32 qy = osy + t * dy - mousePos.y;
                    const f32 d = std::sqrt(qx * qx + qy * qy);
                    if (d < bestDist) { bestDist = d; hoverAxis = i; }
                }
            }
        }

        if (!m_gizmo.active && mouseClicked && hoverAxis >= 0 && insideViewport) {
            if (hoverAxis == 3) {
                // Uniform scale: param = distancia 2D del mouse al centro.
                m_gizmo.active = true;
                m_gizmo.axis = 3;
                m_gizmo.startValue = tform.scale;
                const f32 dx = mousePos.x - osx;
                const f32 dy = mousePos.y - osy;
                m_gizmo.startParam = std::sqrt(dx * dx + dy * dy);
                m_gizmo.field = static_cast<u8>(EditTransformCommand::Field::Scale);
                m_gizmo.entity = selected;
                m_gizmoConsumedClick = true;
            } else {
                glm::vec3 rayOrigin, rayDir;
                if (cameraRay(mousePos, rayOrigin, rayDir)) {
                    m_gizmo.active = true;
                    m_gizmo.axis = hoverAxis;
                    m_gizmo.startValue = (effectiveMode == GizmoMode::Translate)
                        ? tform.position : tform.scale;
                    m_gizmo.startParam = closestParam(worldOrigin, axes[hoverAxis],
                                                       rayOrigin, rayDir);
                    m_gizmo.field = static_cast<u8>(
                        (effectiveMode == GizmoMode::Translate)
                            ? EditTransformCommand::Field::Position
                            : EditTransformCommand::Field::Scale);
                    m_gizmo.entity = selected;
                    m_gizmoConsumedClick = true;
                }
            }
        }

        if (m_gizmo.active && mouseDown && m_gizmo.axis >= 0) {
            if (m_gizmo.axis == 3) {
                // Uniform scale: factor = 1 + (delta_pixels / k_armLen).
                // Mover el cursor k_armLen px afuera del centro duplica
                // la escala; mover hacia el centro la divide.
                const f32 dx = mousePos.x - osx;
                const f32 dy = mousePos.y - osy;
                const f32 nowParam = std::sqrt(dx * dx + dy * dy);
                const f32 deltaPx = nowParam - m_gizmo.startParam;
                const f32 factor = std::max(0.01f, 1.0f + deltaPx / k_armLen);
                tform.scale = m_gizmo.startValue * factor;
                if (m_project) markDirty();
            } else {
                glm::vec3 rayOrigin, rayDir;
                if (cameraRay(mousePos, rayOrigin, rayDir)) {
                    const glm::vec3 axisPoint = (effectiveMode == GizmoMode::Translate)
                        ? m_gizmo.startValue : worldOrigin;
                    const f32 nowParam = closestParam(
                        axisPoint, axes[m_gizmo.axis], rayOrigin, rayDir);
                    const f32 delta = nowParam - m_gizmo.startParam;
                    if (effectiveMode == GizmoMode::Translate) {
                        tform.position = m_gizmo.startValue + axes[m_gizmo.axis] * delta;
                    } else {
                        glm::vec3 ns = m_gizmo.startValue;
                        ns[m_gizmo.axis] = std::max(0.01f, ns[m_gizmo.axis] + delta);
                        tform.scale = ns;
                    }
                    if (m_project) markDirty();
                }
            }
        }

        if (m_gizmo.active && !mouseDown) {
            // Hito 27: empuja EditTransformCommand antes de resetear, asi
            // Ctrl+Z deshace el drag completo (no cada frame del drag).
            finalizeGizmoDrag();
            m_gizmo.active = false;
        }

        // Draw.
        // Handle central de uniform scale (cuadrado en el origen).
        if (effectiveMode == GizmoMode::Scale) {
            const bool isActive = m_gizmo.active && m_gizmo.axis == 3;
            const bool isHover  = !m_gizmo.active && hoverAxis == 3;
            const ImU32 ccol = (isActive || isHover) ? hoverCol
                               : IM_COL32(220, 220, 220, 230);
            const f32 cs = 5.0f;
            dl->AddRectFilled(ImVec2(osx - cs, osy - cs),
                               ImVec2(osx + cs, osy + cs), ccol);
            dl->AddRect(ImVec2(osx - cs, osy - cs),
                         ImVec2(osx + cs, osy + cs),
                         IM_COL32(20, 20, 20, 255));
        }
        for (int i = 0; i < 3; ++i) {
            if (!axisVisible[i]) continue;
            ImU32 col = axisCol[i];
            const bool isActive = m_gizmo.active && m_gizmo.axis == i;
            const bool isHover  = !m_gizmo.active && hoverAxis == i;
            if (isActive || isHover) col = hoverCol;
            dl->AddLine(ImVec2(osx, osy), ImVec2(endSX[i], endSY[i]), col, 2.5f);

            const f32 perpX = -screenDY[i];
            const f32 perpY =  screenDX[i];
            if (effectiveMode == GizmoMode::Translate) {
                // Flecha triangular al final.
                const f32 ah = 10.0f;
                const f32 aw = 5.0f;
                const f32 bx = endSX[i] - screenDX[i] * ah;
                const f32 by = endSY[i] - screenDY[i] * ah;
                dl->AddTriangleFilled(
                    ImVec2(endSX[i], endSY[i]),
                    ImVec2(bx + perpX * aw, by + perpY * aw),
                    ImVec2(bx - perpX * aw, by - perpY * aw),
                    col);
            } else {
                // Scale: cuadrado en el extremo.
                const f32 s = 5.0f;
                dl->AddQuadFilled(
                    ImVec2(endSX[i] + perpX * s,   endSY[i] + perpY * s),
                    ImVec2(endSX[i] + screenDX[i] * s*2 + perpX * s,
                           endSY[i] + screenDY[i] * s*2 + perpY * s),
                    ImVec2(endSX[i] + screenDX[i] * s*2 - perpX * s,
                           endSY[i] + screenDY[i] * s*2 - perpY * s),
                    ImVec2(endSX[i] - perpX * s,   endSY[i] - perpY * s),
                    col);
            }
        }
    }
    // ====== MODO ROTATE (3 anillos eje-alineados) ======
    else {
        // Samplear cada anillo como N puntos del circulo en el plano
        // perpendicular al eje. Proyectar cada punto a screen-space.
        // Hit-test: distancia minima 2D entre mouse y cualquier sample.
        constexpr int k_ringSamples = 48;
        f32 ringSX[3][k_ringSamples];
        f32 ringSY[3][k_ringSamples];
        bool ringVisible[3] = {true, true, true};

        for (int i = 0; i < 3; ++i) {
            // Base vectors en el plano perpendicular al eje i.
            glm::vec3 u, v;
            if (i == 0)      { u = {0, 1, 0}; v = {0, 0, 1}; }
            else if (i == 1) { u = {1, 0, 0}; v = {0, 0, 1}; }
            else             { u = {1, 0, 0}; v = {0, 1, 0}; }

            bool anyHit = false;
            for (int s = 0; s < k_ringSamples; ++s) {
                const f32 a = 2.0f * 3.1415926f * static_cast<f32>(s)
                               / static_cast<f32>(k_ringSamples);
                // Radio 1m world-space. La conversion a screen via
                // project() da un radio variable segun zoom, buscado
                // (Blender hace lo mismo: scale con el zoom).
                const glm::vec3 p = worldOrigin + (u * std::cos(a) + v * std::sin(a));
                f32 sx, sy;
                if (!project(p, sx, sy)) {
                    ringSX[i][s] = osx; ringSY[i][s] = osy; continue;
                }
                ringSX[i][s] = sx; ringSY[i][s] = sy;
                anyHit = true;
            }
            ringVisible[i] = anyHit;
        }

        int hoverAxis = -1;
        if (!m_gizmo.active) {
            f32 bestDist = 8.0f;
            for (int i = 0; i < 3; ++i) {
                if (!ringVisible[i]) continue;
                for (int s = 0; s < k_ringSamples; ++s) {
                    const f32 dx = ringSX[i][s] - mousePos.x;
                    const f32 dy = ringSY[i][s] - mousePos.y;
                    const f32 d = std::sqrt(dx * dx + dy * dy);
                    if (d < bestDist) { bestDist = d; hoverAxis = i; }
                }
            }
        }

        if (!m_gizmo.active && mouseClicked && hoverAxis >= 0 && insideViewport) {
            const f32 startAng = std::atan2(mousePos.y - osy, mousePos.x - osx);
            m_gizmo.active = true;
            m_gizmo.axis = hoverAxis;
            m_gizmo.startValue = tform.rotationEuler;
            m_gizmo.startParam = startAng;
            m_gizmo.field = static_cast<u8>(EditTransformCommand::Field::Rotation);
            m_gizmo.entity = selected;
            m_gizmoConsumedClick = true;
        }

        if (m_gizmo.active && mouseDown && m_gizmo.axis >= 0) {
            const f32 nowAng = std::atan2(mousePos.y - osy, mousePos.x - osx);
            f32 deltaRad = nowAng - m_gizmo.startParam;
            // Unwrap: mantener la acumulacion suave al cruzar el pi / -pi.
            while (deltaRad >  3.1415926f) deltaRad -= 2.0f * 3.1415926f;
            while (deltaRad < -3.1415926f) deltaRad += 2.0f * 3.1415926f;
            const f32 deltaDeg = deltaRad * (180.0f / 3.1415926f);
            glm::vec3 nr = m_gizmo.startValue;
            // Signo: si el eje apunta "hacia la camara", la rotacion
            // en sentido horario del mouse equivale a sentido antihor
            // alrededor del eje (regla de la mano derecha vista desde +axis).
            // Aprox: invertir si axis . camera_forward > 0 — el eje
            // apunta hacia donde mira la camara.
            const glm::vec3 camPos = glm::vec3(glm::inverse(m_sceneRenderer->lastView())[3]);
            const glm::vec3 camToOrigin = worldOrigin - camPos;
            const f32 sgn = (glm::dot(axes[m_gizmo.axis], camToOrigin) > 0.0f) ? 1.0f : -1.0f;
            nr[m_gizmo.axis] = m_gizmo.startValue[m_gizmo.axis] + sgn * deltaDeg;
            tform.rotationEuler = nr;
            if (m_project) markDirty();
        }

        if (m_gizmo.active && !mouseDown) {
            // Hito 27: empuja EditTransformCommand antes de resetear, asi
            // Ctrl+Z deshace el drag completo (no cada frame del drag).
            finalizeGizmoDrag();
            m_gizmo.active = false;
        }

        // Draw rings as polylines.
        for (int i = 0; i < 3; ++i) {
            if (!ringVisible[i]) continue;
            ImU32 col = axisCol[i];
            const bool isActive = m_gizmo.active && m_gizmo.axis == i;
            const bool isHover  = !m_gizmo.active && hoverAxis == i;
            if (isActive || isHover) col = hoverCol;
            for (int s = 0; s < k_ringSamples; ++s) {
                const int n = (s + 1) % k_ringSamples;
                dl->AddLine(ImVec2(ringSX[i][s], ringSY[i][s]),
                             ImVec2(ringSX[i][n], ringSY[i][n]),
                             col, 2.0f);
            }
        }
        (void)k_ringRad; // silenciar warning si no se usa
    }
}

} // namespace Mood
