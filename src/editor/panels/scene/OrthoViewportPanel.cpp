#include "editor/panels/scene/OrthoViewportPanel.h"

#include "engine/render/rhi/IFramebuffer.h"
#include "engine/scene/components/BrushComponent.h"   // F2H35 Bloque E: skip
#include "engine/scene/components/Components.h"        // F2H35 Bloque E
#include "engine/scene/core/Entity.h"                  // F2H35 Bloque E
#include "engine/scene/core/Scene.h"                   // F2H35 Bloque E

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace Mood {

void OrthoViewportPanel::onImGuiRender() {
    if (!visible) return;

    // Naranja Valve para el label y crosshair (paleta F2H28 opcion A).
    constexpr ImU32 k_orangeValve = IM_COL32(245, 130, 32, 255);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin(name(), &visible)) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        m_desiredWidth = static_cast<u32>(avail.x);
        m_desiredHeight = static_cast<u32>(avail.y);

        const bool hasFb = m_framebuffer != nullptr
                        && m_framebuffer->width() > 0
                        && m_framebuffer->height() > 0;

        ImVec2 imageMin{0.0f, 0.0f};
        ImVec2 imageSize{0.0f, 0.0f};

        if (hasFb) {
            // OpenGL origen abajo-izq, ImGui arriba-izq -> invertir V.
            const ImVec2 uv0(0.0f, 1.0f);
            const ImVec2 uv1(1.0f, 0.0f);
            ImGui::Image(m_framebuffer->colorAttachmentHandle(),
                         avail, uv0, uv1);
            imageMin = ImGui::GetItemRectMin();
            imageSize = ImGui::GetItemRectSize();
        } else {
            // Placeholder: rect negro (mismo color del fondo orto del
            // Bloque D para que el dev intuya como va a quedar). Texto
            // gris claro arriba para que se lea sobre el negro.
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 panelPos = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(panelPos,
                              ImVec2(panelPos.x + avail.x,
                                     panelPos.y + avail.y),
                              IM_COL32(0, 0, 0, 255));
            imageMin = panelPos;
            imageSize = avail;
            // Reservar espacio para que la ventana no colapse.
            ImGui::Dummy(avail);

            const char* placeholder = "(sin framebuffer)";
            const ImVec2 textSize = ImGui::CalcTextSize(placeholder);
            dl->AddText(ImVec2(panelPos.x + (avail.x - textSize.x) * 0.5f,
                               panelPos.y + (avail.y - textSize.y) * 0.5f),
                        IM_COL32(180, 180, 180, 255), placeholder);
        }

        // Label arriba-izq estilo Hammer ("top (xz)" en Hammer original
        // pero en castellano y con eje en mayusculas).
        ImDrawList* dlText = ImGui::GetWindowDrawList();
        dlText->AddText(ImVec2(imageMin.x + 6.0f, imageMin.y + 4.0f),
                         k_orangeValve, name());

        // F2H28 Bloque G: label "Grid: Nu" arriba-derecha mostrando el
        // snap step actual. El state vive en EditorApplication; aca solo
        // lo formateamos. Mismo color naranja que el label de la vista
        // para consistencia visual.
        char gridLabel[32];
        std::snprintf(gridLabel, sizeof(gridLabel),
                       "Grid: %uu", m_snapStep);
        const ImVec2 gridLabelSize = ImGui::CalcTextSize(gridLabel);
        dlText->AddText(ImVec2(imageMin.x + imageSize.x - gridLabelSize.x - 6.0f,
                                imageMin.y + 4.0f),
                         k_orangeValve, gridLabel);

        // F2H35 Bloque E: labels de point entities (Light/Audio/
        // Trigger/Camera/Particle sin mesh) flotando arriba del icono
        // del Bloque D en cada orto. Proyecta pos world -> ndc del orto
        // -> screen del panel. Mismo color/outline que el label del
        // perspective (gris claro + outline negro). Solo si toggle ON
        // (m_showEntityLabels, sincronizado desde EditorUI cada frame).
        if (m_scene != nullptr && m_showEntityLabels &&
            hasFb && imageSize.x > 0 && imageSize.y > 0) {
            const f32 oAspect = imageSize.x / imageSize.y;
            const glm::mat4 oView = m_camera.viewMatrix();
            const glm::mat4 oProj = m_camera.projMatrix(oAspect);
            const glm::mat4 vp = oProj * oView;
            const ImU32 colTextBg = IM_COL32(0, 0, 0, 220);
            const ImU32 colText   = IM_COL32(220, 220, 220, 235);
            m_scene->forEach<TransformComponent, TagComponent>(
                [&](Entity e, TransformComponent& t, TagComponent& tag) {
                    if (e.hasComponent<MeshRendererComponent>()) return;
                    if (e.hasComponent<BrushComponent>()) return;
                    const bool isPointEntity =
                        e.hasComponent<LightComponent>() ||
                        e.hasComponent<AudioSourceComponent>() ||
                        e.hasComponent<TriggerComponent>() ||
                        e.hasComponent<CameraComponent>() ||
                        e.hasComponent<ParticleEmitterComponent>();
                    if (!isPointEntity || tag.name.empty()) return;
                    const glm::vec4 clip = vp * glm::vec4(t.position, 1.0f);
                    if (clip.w <= 0.0f) return;
                    const float ndcX = clip.x / clip.w;
                    const float ndcY = clip.y / clip.w;
                    if (ndcX < -1.1f || ndcX > 1.1f ||
                        ndcY < -1.1f || ndcY > 1.1f) return;
                    const float sx = imageMin.x +
                        (ndcX * 0.5f + 0.5f) * imageSize.x;
                    const float sy = imageMin.y +
                        (1.0f - (ndcY * 0.5f + 0.5f)) * imageSize.y;
                    const ImVec2 textSize =
                        ImGui::CalcTextSize(tag.name.c_str());
                    // Offset arriba del icono (cubo de r=0.4 en world,
                    // ~10 px en pantalla a zoom default — aprox 14 px de
                    // offset garantiza que no tape el cubo).
                    const ImVec2 textPos(sx - textSize.x * 0.5f, sy - 18.0f);
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dy = -1; dy <= 1; ++dy)
                            if (dx != 0 || dy != 0)
                                dlText->AddText(
                                    ImVec2(textPos.x + (float)dx,
                                           textPos.y + (float)dy),
                                    colTextBg, tag.name.c_str());
                    dlText->AddText(textPos, colText, tag.name.c_str());
                });
        }

        // F2H31 Bloque D: coords del cursor en world space (debajo del
        // label de la vista). Solo se muestra si el cursor esta dentro
        // del panel. Usa la matriz de la camara orto + aspect actual
        // para la conversion ndc -> world.
        if (m_liveCursor.hovered && imageSize.y > 0) {
            const f32 oAspect = imageSize.x / imageSize.y;
            const glm::vec3 wp = m_camera.worldFromNdc(
                m_liveCursor.ndcX, m_liveCursor.ndcY, oAspect);
            char coordsLabel[64];
            std::snprintf(coordsLabel, sizeof(coordsLabel),
                           "(%.1f, %.1f, %.1f)", wp.x, wp.y, wp.z);
            dlText->AddText(
                ImVec2(imageMin.x + 6.0f, imageMin.y + 22.0f),
                IM_COL32(200, 200, 200, 230), coordsLabel);
        }

        if (!hasFb) {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        const bool hovered = ImGui::IsItemHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGuiIO& io = ImGui::GetIO();

        auto mousePosToNdc = [&](ImVec2 mp, float& ndcX, float& ndcY) {
            if (imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
                ndcX = 0.0f; ndcY = 0.0f; return;
            }
            const float lx = mp.x - imageMin.x;
            const float ly = mp.y - imageMin.y;
            ndcX = (lx / imageSize.x) * 2.0f - 1.0f;
            ndcY = 1.0f - (ly / imageSize.y) * 2.0f;
        };

        // F2H30 Bloque C: live cursor en NDC mientras el panel este
        // hovered. Lo lee el pincel poligonal para la rubber-band.
        m_liveCursor.hovered = hovered;
        if (hovered) {
            const ImVec2 p = ImGui::GetMousePos();
            mousePosToNdc(p, m_liveCursor.ndcX, m_liveCursor.ndcY);
        }

        if (hovered) {
            // Zoom con scroll wheel.
            if (io.MouseWheel != 0.0f) {
                m_camera.zoom(io.MouseWheel);
            }

            // Pan con MMB drag.
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                m_panDragging = true;
            }

            // Click-select con LMB (registramos down + comparamos delta al up).
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_leftMouseDown = true;
                const ImVec2 p = ImGui::GetMousePos();
                m_leftDownX = p.x;
                m_leftDownY = p.y;
                // F2H29 Bloque B: capturar NDC inicial para potencial drag.
                // Si el LMB se mantiene + se mueve > 4 px, activamos drag.
                mousePosToNdc(p, m_dragState.ndcStartX, m_dragState.ndcStartY);
                m_dragState.ndcCurX = m_dragState.ndcStartX;
                m_dragState.ndcCurY = m_dragState.ndcStartY;
                m_dragState.active = false;
                m_dragState.justEnded = false;
            }
        }

        // F2H29 Bloque B: detectar transicion a drag activo + actualizar
        // ndcCur cada frame que el LMB siga down. Se chequea siempre
        // (no solo hovered) por si el cursor sale del panel mid-drag —
        // la consigna del drag sigue valida hasta el LMB up.
        if (m_leftMouseDown && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 p = ImGui::GetMousePos();
            const float dx = p.x - m_leftDownX;
            const float dy = p.y - m_leftDownY;
            if (!m_dragState.active && (dx*dx + dy*dy) >= 16.0f) {
                m_dragState.active = true;
            }
            if (m_dragState.active) {
                mousePosToNdc(p, m_dragState.ndcCurX, m_dragState.ndcCurY);
            }
        }

        // Aplicar pan delta cada frame que el MMB siga down.
        if (m_panDragging && imageSize.x > 0.0f && imageSize.y > 0.0f) {
            const float aspect = imageSize.x / imageSize.y;
            // Drag a derecha -> el mundo se mueve a izquierda en
            // pantalla, equivale a panOffset.x bajando. Igual logica
            // para Y (drag arriba = panOffset.y subiendo en world).
            const float worldDx = -io.MouseDelta.x / imageSize.x
                                   * m_camera.worldHeight * aspect;
            const float worldDy =  io.MouseDelta.y / imageSize.y
                                   * m_camera.worldHeight;
            m_camera.panOffset.x += worldDx;
            m_camera.panOffset.y += worldDy;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            m_panDragging = false;
        }

        // Resolver click-select / drag-end al soltar el LMB. Chequear
        // siempre (no solo hovered) por si el dev mueve el mouse fuera
        // antes de soltar — sin drag real, sigue contando como click;
        // con drag real, sigue contando como drag-end.
        if (m_leftMouseDown && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (m_dragState.active) {
                // F2H29 Bloque B: termino el drag. Pulso para que el
                // caller pushee el command. `active=false` para que el
                // siguiente frame deje de mover en vivo.
                m_dragState.active = false;
                m_dragState.justEnded = true;
            } else {
                // No hubo drag — caer al click-select del Bloque F.
                const ImVec2 p = ImGui::GetMousePos();
                if (hovered) {
                    float ndcX = 0.0f;
                    float ndcY = 0.0f;
                    mousePosToNdc(p, ndcX, ndcY);
                    m_pendingClick = ClickSelect{true, ndcX, ndcY};
                }
            }
            m_leftMouseDown = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Mood
