// F2H24 Bloque C: handles + drag-state del gizmo translate/rotate/scale
// del overlay 2D del editor. Extraido de EditorOverlay.cpp para mantener
// cada archivo bajo el cap LOC. Recibe `vp` + viewport rect + selected
// + origen proyectado ya calculados por drawEditorOverlay.

#include "editor/application/EditorApplication.h"
#include "editor/application/EditorOverlay_Internal.h"

#include "editor/commands/EditTransformCommand.h"
#include "editor/selection/SelectionSet.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>

namespace Mood {

void EditorApplication::drawEditorOverlayGizmo(ImDrawList* dl,
                                                 float x0, float y0,
                                                 float w, float h,
                                                 const glm::mat4& vp,
                                                 Entity selected,
                                                 float osx, float osy) {
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

    const glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const ImU32 axisCol[3] = {
        IM_COL32(230, 50, 50, 230),   // X rojo
        IM_COL32(80, 230, 80, 230),   // Y verde
        IM_COL32(70, 120, 255, 230),  // Z azul
    };
    const ImU32 hoverCol = IM_COL32(255, 220, 30, 255);
    const f32 k_armLen = 60.0f;
    const f32 k_ringRad = 55.0f;

    // F2H35 fix: radio del ring de Rotate CONSTANTE EN PANTALLA (igual
    // que `k_armLen = 60` de Translate/Scale arriba). Pre-F2H35 era
    // proporcional al AABB world-space (F2H30 Bloque D) — al alejar
    // la cam el ring se hacia muy chico y dificil de clickear, mientras
    // que los handles de Translate/Scale se mantenian a 60 px. El dev
    // reporto la inconsistencia: "deberia ser proporcional a la
    // distancia de la camara, porque los demas lo son". Ahora derivamos
    // `worldRadius = TARGET_PX / pixelsPerWorld` usando la distancia
    // cam->origin + FOV vertical (analogo a la proyeccion perspectiva).
    f32 gizmoRingRadius = 1.0f;
    {
        const glm::vec3 camPos = m_editorCamera.position();
        const f32 dist = glm::length(camPos - worldOrigin);
        const f32 fovYrad = m_editorCamera.fovDeg() * 3.1415926f / 180.0f;
        const f32 halfH = std::max(dist * std::tan(fovYrad * 0.5f), 0.001f);
        const f32 pxPerWorld = (h * 0.5f) / halfH;
        constexpr f32 k_targetRingPx = 70.0f;
        gizmoRingRadius = k_targetRingPx / std::max(pxPerWorld, 0.001f);
    }

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
            if (!detail::projectWorldToScreen(vp, x0, y0, w, h,
                    worldOrigin + axes[i] * 1.0f, esx, esy)) continue;
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
            // F2H30 Bloque D iter 2: removido el handle central (axis=3)
            // de uniform scale. Pedido del dev: "al gismo de escalar,
            // sacale el cuadrado central asi solo se usa la S". Para
            // uniform scale ahora se usa exclusivamente la tecla S.
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
            // F2H23 iter 5: helper interno para popular otherStarts
            // (snapshot de las entidades extra del SelectionSet) al
            // iniciar cualquier drag. Usado por los 3 modos.
            auto populateOtherStarts =
                [&](EditTransformCommand::Field f) {
                m_gizmo.otherStarts.clear();
                const SelectionSet& set = m_ui.selectionSet();
                if (set.selected.size() <= 1u) return;
                m_gizmo.otherStarts.reserve(set.selected.size() - 1);
                for (const Entity& other : set.selected) {
                    if (other.handle() == selected.handle()) continue;
                    if (!m_scene->registry().valid(other.handle())) continue;
                    if (!other.hasComponent<TransformComponent>()) continue;
                    Entity oCopy = other;
                    const auto& ot = oCopy.getComponent<TransformComponent>();
                    glm::vec3 sv;
                    switch (f) {
                        case EditTransformCommand::Field::Position: sv = ot.position; break;
                        case EditTransformCommand::Field::Rotation: sv = ot.rotationEuler; break;
                        case EditTransformCommand::Field::Scale:    sv = ot.scale; break;
                    }
                    m_gizmo.otherStarts.push_back({other, sv});
                }
            };

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
                populateOtherStarts(EditTransformCommand::Field::Scale);
                m_gizmoConsumedClick = true;
            } else {
                glm::vec3 rayOrigin, rayDir;
                if (detail::cameraRayFromScreen(vp, x0, y0, w, h, mousePos, rayOrigin, rayDir)) {
                    m_gizmo.active = true;
                    m_gizmo.axis = hoverAxis;
                    m_gizmo.startValue = (effectiveMode == GizmoMode::Translate)
                        ? tform.position : tform.scale;
                    m_gizmo.startParam = detail::closestParamLineLine(
                        worldOrigin, axes[hoverAxis], rayOrigin, rayDir);
                    const auto f = (effectiveMode == GizmoMode::Translate)
                        ? EditTransformCommand::Field::Position
                        : EditTransformCommand::Field::Scale;
                    m_gizmo.field = static_cast<u8>(f);
                    m_gizmo.entity = selected;
                    populateOtherStarts(f);
                    m_gizmoConsumedClick = true;
                }
            }
        }

        if (m_gizmo.active && mouseDown && m_gizmo.axis >= 0) {
            // F2H23 iter 5: helper para aplicar el mismo delta a las
            // otras entidades del SelectionSet en VIVO (cada frame del
            // drag). Sin esto el dev veia que solo el active se movia
            // y las demas saltaban al final.
            auto applyDeltaLive = [&](const glm::vec3& delta) {
                if (m_gizmo.otherStarts.empty()) return;
                const auto field = static_cast<EditTransformCommand::Field>(
                    m_gizmo.field);
                for (const auto& os : m_gizmo.otherStarts) {
                    if (!m_scene->registry().valid(os.entity.handle())) continue;
                    if (!os.entity.hasComponent<TransformComponent>()) continue;
                    Entity oCopy = os.entity;
                    auto& ot = oCopy.getComponent<TransformComponent>();
                    switch (field) {
                        case EditTransformCommand::Field::Position:
                            ot.position = os.startValue + delta;
                            break;
                        case EditTransformCommand::Field::Rotation:
                            ot.rotationEuler = os.startValue + delta;
                            break;
                        case EditTransformCommand::Field::Scale: {
                            glm::vec3 ns = os.startValue + delta;
                            ot.scale = glm::vec3(
                                std::clamp(ns.x, 0.01f, 100.0f),
                                std::clamp(ns.y, 0.01f, 100.0f),
                                std::clamp(ns.z, 0.01f, 100.0f));
                            break;
                        }
                    }
                }
            };

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
                // Multi-edit: para uniform scale el "delta" no es additive
                // sino multiplicative — applyDelta(scale_delta) no es lo
                // mismo. Hacemos el factor por entidad: each.scale =
                // each.startValue * factor.
                for (const auto& os : m_gizmo.otherStarts) {
                    if (!m_scene->registry().valid(os.entity.handle())) continue;
                    if (!os.entity.hasComponent<TransformComponent>()) continue;
                    Entity oCopy = os.entity;
                    auto& ot = oCopy.getComponent<TransformComponent>();
                    glm::vec3 ns = os.startValue * factor;
                    ot.scale = glm::vec3(
                        std::clamp(ns.x, 0.01f, 100.0f),
                        std::clamp(ns.y, 0.01f, 100.0f),
                        std::clamp(ns.z, 0.01f, 100.0f));
                }
                if (m_project) markDirty();
            } else {
                glm::vec3 rayOrigin, rayDir;
                if (detail::cameraRayFromScreen(vp, x0, y0, w, h, mousePos, rayOrigin, rayDir)) {
                    const glm::vec3 axisPoint = (effectiveMode == GizmoMode::Translate)
                        ? m_gizmo.startValue : worldOrigin;
                    const f32 nowParam = detail::closestParamLineLine(
                        axisPoint, axes[m_gizmo.axis], rayOrigin, rayDir);
                    const f32 delta = nowParam - m_gizmo.startParam;
                    if (effectiveMode == GizmoMode::Translate) {
                        const glm::vec3 deltaVec = axes[m_gizmo.axis] * delta;
                        tform.position = m_gizmo.startValue + deltaVec;
                        applyDeltaLive(deltaVec);
                    } else {
                        glm::vec3 ns = m_gizmo.startValue;
                        ns[m_gizmo.axis] = std::max(0.01f, ns[m_gizmo.axis] + delta);
                        tform.scale = ns;
                        // Per-axis scale multi-edit: aplicar el mismo
                        // delta_axis a las demas (additive en el axis,
                        // preserva los otros 2 ejes de cada entidad).
                        for (const auto& os : m_gizmo.otherStarts) {
                            if (!m_scene->registry().valid(os.entity.handle())) continue;
                            if (!os.entity.hasComponent<TransformComponent>()) continue;
                            Entity oCopy = os.entity;
                            auto& ot = oCopy.getComponent<TransformComponent>();
                            glm::vec3 sv = os.startValue;
                            sv[m_gizmo.axis] = std::clamp(
                                sv[m_gizmo.axis] + delta, 0.01f, 100.0f);
                            ot.scale = sv;
                        }
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
        // F2H30 Bloque D iter 2: el handle central de uniform scale fue
        // removido (pedido del dev). Para escalar uniforme se usa la
        // tecla S del modal Blender; los 3 arrows per-axis siguen para
        // scale por eje individual.
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
                // F2H30 Bloque D: radio = `gizmoRingRadius` (proporcional
                // al AABB del entity, computado arriba). Pre-F2H30 era
                // fijo 1m — visible/clickeable solo para entidades del
                // tamano del cubo unitario.
                const glm::vec3 p = worldOrigin + (u * std::cos(a) + v * std::sin(a))
                                     * gizmoRingRadius;
                f32 sx, sy;
                if (!detail::projectWorldToScreen(vp, x0, y0, w, h, p, sx, sy)) {
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
            // F2H23 iter 5: snapshot rotacion inicial de las demas
            // entidades del SelectionSet para multi-edit en vivo.
            m_gizmo.otherStarts.clear();
            const SelectionSet& set = m_ui.selectionSet();
            if (set.selected.size() > 1u) {
                m_gizmo.otherStarts.reserve(set.selected.size() - 1);
                for (const Entity& other : set.selected) {
                    if (other.handle() == selected.handle()) continue;
                    if (!m_scene->registry().valid(other.handle())) continue;
                    if (!other.hasComponent<TransformComponent>()) continue;
                    Entity oCopy = other;
                    const auto& ot = oCopy.getComponent<TransformComponent>();
                    m_gizmo.otherStarts.push_back({other, ot.rotationEuler});
                }
            }
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
            const f32 axisDelta = sgn * deltaDeg;
            nr[m_gizmo.axis] = m_gizmo.startValue[m_gizmo.axis] + axisDelta;
            tform.rotationEuler = nr;
            // F2H23 iter 5: aplicar el mismo delta del eje a las
            // entidades extra del SelectionSet (multi-edit en vivo).
            for (const auto& os : m_gizmo.otherStarts) {
                if (!m_scene->registry().valid(os.entity.handle())) continue;
                if (!os.entity.hasComponent<TransformComponent>()) continue;
                Entity oCopy = os.entity;
                auto& ot = oCopy.getComponent<TransformComponent>();
                glm::vec3 osr = os.startValue;
                osr[m_gizmo.axis] = os.startValue[m_gizmo.axis] + axisDelta;
                ot.rotationEuler = osr;
            }
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
