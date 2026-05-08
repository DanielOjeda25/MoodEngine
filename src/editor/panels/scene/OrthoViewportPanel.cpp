#include "editor/panels/scene/OrthoViewportPanel.h"

#include "engine/render/rhi/IFramebuffer.h"

#include <imgui.h>

#include <algorithm>

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
            // Placeholder: rect gris claro (mismo color del fondo orto
            // del Bloque D para que el dev intuya como va a quedar).
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 panelPos = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(panelPos,
                              ImVec2(panelPos.x + avail.x,
                                     panelPos.y + avail.y),
                              IM_COL32(200, 200, 200, 255));
            imageMin = panelPos;
            imageSize = avail;
            // Reservar espacio para que la ventana no colapse.
            ImGui::Dummy(avail);

            const char* placeholder = "(sin framebuffer)";
            const ImVec2 textSize = ImGui::CalcTextSize(placeholder);
            dl->AddText(ImVec2(panelPos.x + (avail.x - textSize.x) * 0.5f,
                               panelPos.y + (avail.y - textSize.y) * 0.5f),
                        IM_COL32(80, 80, 80, 255), placeholder);
        }

        // Label arriba-izq estilo Hammer ("top (xz)" en Hammer original
        // pero en castellano y con eje en mayusculas).
        ImDrawList* dlText = ImGui::GetWindowDrawList();
        dlText->AddText(ImVec2(imageMin.x + 6.0f, imageMin.y + 4.0f),
                         k_orangeValve, name());

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

        // Resolver click-select al soltar el LMB. Chequear siempre
        // (no solo hovered) por si el dev mueve el mouse fuera antes
        // de soltar — sin drag real, sigue contando como click.
        if (m_leftMouseDown && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 p = ImGui::GetMousePos();
            const float dx = p.x - m_leftDownX;
            const float dy = p.y - m_leftDownY;
            const bool wasClick = (dx*dx + dy*dy) < 16.0f; // 4 px
            if (wasClick && hovered) {
                float ndcX = 0.0f;
                float ndcY = 0.0f;
                mousePosToNdc(p, ndcX, ndcY);
                m_pendingClick = ClickSelect{true, ndcX, ndcY};
            }
            m_leftMouseDown = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Mood
