#include "editor/panels/ViewportPanel.h"

#include "engine/render/IFramebuffer.h"

#include <imgui.h>

#include <cstring>

namespace Mood {

void ViewportPanel::onImGuiRender() {
    // Reset de input capturado: se actualiza dentro del panel si aplica.
    m_cameraRotateDx = 0.0f;
    m_cameraRotateDy = 0.0f;
    m_cameraPanDx = 0.0f;
    m_cameraPanDy = 0.0f;
    m_cameraWheel = 0.0f;
    m_imageHovered = false;
    m_mouseNdcX = 0.0f;
    m_mouseNdcY = 0.0f;

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

            // Capturar el rect de la imagen AHORA: dentro de
            // BeginDragDropTarget el "ultimo item" cambia a un widget
            // interno de ImGui y GetItemRectMin() daria coords erroneas.
            const ImVec2 imageMin = ImGui::GetItemRectMin();
            const ImVec2 imageSize = ImGui::GetItemRectSize();

            // Overlay callback (Hito 13): iconos + gizmos se dibujan sobre
            // la imagen con ImGui drawlist, asi quedan por encima del render
            // 3D sin pasar por GL. Clip rect al area de la imagen.
            if (m_overlayDraw) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->PushClipRect(imageMin,
                                 ImVec2(imageMin.x + imageSize.x,
                                        imageMin.y + imageSize.y),
                                 true);
                m_overlayDraw(dl, imageMin.x, imageMin.y,
                               imageSize.x, imageSize.y);
                dl->PopClipRect();
            }

            // Helper local: pos del cursor -> NDC dentro de la imagen.
            auto mousePosToNdc = [&imageMin, &imageSize](ImVec2 mp, float& ndcX, float& ndcY) {
                if (imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
                    ndcX = 0.0f; ndcY = 0.0f; return;
                }
                const float lx = mp.x - imageMin.x;
                const float ly = mp.y - imageMin.y;
                ndcX = (lx / imageSize.x) * 2.0f - 1.0f;
                ndcY = 1.0f - (ly / imageSize.y) * 2.0f;
            };

            // Drop target: el AssetBrowser emite payloads
            // "MOOD_TEXTURE_ASSET" (id de textura a aplicar a un tile) o
            // "MOOD_MESH_ASSET" (id de mesh a spawnear como nueva entidad).
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_TEXTURE_ASSET")) {
                    u32 id = 0;
                    if (payload->DataSize == sizeof(id)) {
                        std::memcpy(&id, payload->Data, sizeof(id));
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingDrop = TextureDrop{true, ndcX, ndcY, id};
                    }
                }
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_MESH_ASSET")) {
                    u32 id = 0;
                    if (payload->DataSize == sizeof(id)) {
                        std::memcpy(&id, payload->Data, sizeof(id));
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingMeshDrop = MeshDrop{true, ndcX, ndcY, id};
                    }
                }
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_PREFAB_ASSET")) {
                    u32 id = 0;
                    if (payload->DataSize == sizeof(id)) {
                        std::memcpy(&id, payload->Data, sizeof(id));
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingPrefabDrop = PrefabDrop{true, ndcX, ndcY, id};
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Captura de input para la camara + hover del viewport. Importante:
            // AllowWhenBlockedByActiveItem mantiene el hover activo durante
            // drags (sin eso, al arrastrar una textura se pierde el cyan).
            const bool hovered = ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            ImGuiIO& io = ImGui::GetIO();

            if (hovered) {
                m_imageHovered = true;
                mousePosToNdc(ImGui::GetMousePos(), m_mouseNdcX, m_mouseNdcY);

                m_cameraWheel = io.MouseWheel;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    m_rightDragging = true;
                }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                    m_middleDragging = true;
                }
                // Click-to-select (Hito 13): registrar mouse down y probar
                // al soltar si hubo drag. Umbral 4px distingue click puro.
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_leftMouseDown = true;
                    const ImVec2 p = ImGui::GetMousePos();
                    m_leftDownX = p.x;
                    m_leftDownY = p.y;
                }
            }
            // Al soltar el left button (haya sido donde haya sido) — si
            // el down fue sobre el viewport y el desplazamiento es chico,
            // disparar click-select. Chequear siempre (no solo si hovered)
            // para soportar que el usuario mueva el mouse hacia afuera
            // antes de soltar; sin drag real, cuenta como click.
            if (m_leftMouseDown && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const ImVec2 p = ImGui::GetMousePos();
                const float dx = p.x - m_leftDownX;
                const float dy = p.y - m_leftDownY;
                const bool wasClick = (dx*dx + dy*dy) < 16.0f; // 4px
                if (wasClick && m_imageHovered) {
                    float ndcX = 0.0f;
                    float ndcY = 0.0f;
                    mousePosToNdc(p, ndcX, ndcY);
                    m_pendingClick = ClickSelect{true, ndcX, ndcY};
                }
                m_leftMouseDown = false;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                m_rightDragging = false;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                m_middleDragging = false;
            }
            if (m_rightDragging) {
                m_cameraRotateDx = io.MouseDelta.x;
                m_cameraRotateDy = io.MouseDelta.y;
            }
            if (m_middleDragging) {
                m_cameraPanDx = io.MouseDelta.x;
                m_cameraPanDy = io.MouseDelta.y;
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
