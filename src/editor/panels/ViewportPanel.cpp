#include "editor/panels/ViewportPanel.h"

#include "engine/render/IFramebuffer.h"

#include <imgui.h>

namespace Mood {

void ViewportPanel::onImGuiRender() {
    // Reset de input capturado: se actualiza dentro del panel si aplica.
    m_cameraRotateDx = 0.0f;
    m_cameraRotateDy = 0.0f;
    m_cameraWheel = 0.0f;

    if (!visible) return;

    // Quitar padding para que la imagen ocupe todo el panel.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin(name(), &visible)) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        m_desiredWidth = static_cast<u32>(avail.x);
        m_desiredHeight = static_cast<u32>(avail.y);

        if (m_framebuffer != nullptr && m_framebuffer->width() > 0 && m_framebuffer->height() > 0) {
            // OpenGL tiene origen abajo-izquierda; ImGui, arriba-izquierda.
            // Se invierte la coordenada V para que la imagen no salga volteada.
            const ImVec2 uv0(0.0f, 1.0f);
            const ImVec2 uv1(1.0f, 0.0f);
            ImGui::Image(m_framebuffer->colorAttachmentHandle(), avail, uv0, uv1);

            // Captura de input para la camara del editor mientras el cursor
            // esta sobre la imagen del viewport.
            const bool hovered = ImGui::IsItemHovered();
            ImGuiIO& io = ImGui::GetIO();

            if (hovered) {
                m_cameraWheel = io.MouseWheel;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    m_rightDragging = true;
                }
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                m_rightDragging = false;
            }
            if (m_rightDragging) {
                m_cameraRotateDx = io.MouseDelta.x;
                m_cameraRotateDy = io.MouseDelta.y;
            }
        } else {
            const char* placeholder = "Viewport (sin framebuffer)";
            const ImVec2 textSize = ImGui::CalcTextSize(placeholder);
            ImGui::SetCursorPos(ImVec2(
                (avail.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
                (avail.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()));
            ImGui::TextUnformatted(placeholder);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Mood
