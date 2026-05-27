#include "editor/panels/scene/ViewportPanel.h"

#include "editor/application/EditorMode.h"   // F2H59: EditorSubMode para Cara toggle.
#include "editor/ui/EditorUI.h"               // F2H59: overlay tool buttons.
#include "editor/ui/IconsFontAwesome6.h"     // F2H59: icons del overlay.
#include "core/UserSettings.h"               // F3H7: click-vs-drag threshold
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/rhi/IFramebuffer.h"
#include "engine/scene/serialization/ProjectSerializer.h"  // F3H20: snap toggles

#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

namespace Mood {

namespace {

// F2H59: overlay flotante de herramientas estilo Blender. Sub-window
// posicionada al top-left de la imagen del viewport, sin titulo / sin
// docking / sin resize, con background semi-transparente. La posicion
// es sticky al viewport (no es movible) para que el dev siempre la
// encuentre en el mismo lugar al cambiar de layout.
void drawViewportToolsOverlay(EditorUI* ui, ImVec2 imageMin) {
    if (ui == nullptr) return;

    constexpr float kOverlayOffset = 8.0f;
    ImGui::SetNextWindowPos(ImVec2(imageMin.x + kOverlayOffset,
                                     imageMin.y + kOverlayOffset),
                              ImGuiCond_Always);
    // F2H59: alpha 0 -> background totalmente transparente, solo se ven
    // los botones (estilo Blender). Pedido del dev: "le podemos dejar el
    // fondo transparente para que solo sean botones flotantes?".
    ImGui::SetNextWindowBgAlpha(0.0f);

    // F2H59 fix: WindowBorderSize y PopupBorderSize a 0 para que no
    // queden lineas finas del frame de la sub-window con bg transparente.
    // Pedido del dev: "se sigue viendo unas lineas finas".
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground;  // F2H59: redundante con alpha=0 pero explicito.

    if (!ImGui::Begin("##viewport_tools_overlay", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    constexpr ImVec2 kBtnSize{36.0f, 36.0f};
    auto iconBtn = [&kBtnSize](const char* icon, const char* tooltipKey,
                                  bool active = false) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                    ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const bool clicked = ImGui::Button(icon, kBtnSize);
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(tooltipKey).c_str());
        }
        return clicked;
    };

    if (iconBtn(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
                 "editor.panel.toolbar.move_tooltip")) {
        ui->requestGizmoMode(0);
    }
    if (iconBtn(ICON_FA_ROTATE,
                 "editor.panel.toolbar.rotate_tooltip")) {
        ui->requestGizmoMode(1);
    }
    if (iconBtn(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER,
                 "editor.panel.toolbar.scale_tooltip")) {
        ui->requestGizmoMode(2);
    }

    ImGui::Separator();

    // F2H59: el modo Face se identifica con la letra "F" (mas claro que
    // el icono cuadrado generico). Pedido del dev. Homogeneizar el
    // sistema de iconos del editor queda como follow-up.
    const bool faceActive = (ui->subMode() == EditorSubMode::Face);
    if (iconBtn("F",
                 "editor.panel.toolbar.face_tooltip",
                 faceActive)) {
        ui->requestToggleFaceMode();
    }

    // ---- F3H20: snap toggles del gizmo perspectivo (Grid / Angle).
    //   G (grid)  -> translate (W) cuantiza delta a multiplos del step.
    //   A (angle) -> rotate (R) cuantiza grados a multiplos del increment.
    // No son mutuamente exclusivos (afectan modos distintos). Vertex snap
    // de orthos (workspace "Editor de mapas") vive en su propia UI; aca
    // solo lo del perspectivo. Sin proyecto no hay donde persistir => sin
    // botones. El step actual se ve en el status bar arriba del viewport
    // (`drawViewportSnapStatusBar`).
    Project* project = ui->currentProject();
    if (project != nullptr) {
        ImGui::Separator();
        auto& snap = project->settings.snap;
        auto snapToggle = [&](const char* shortLabel, const char* tooltipKey,
                                bool& flag) {
            if (iconBtn(shortLabel, tooltipKey, flag)) {
                flag = !flag;
                ui->setProjectDirty(true);
            }
        };
        snapToggle("G",
                    "editor.panel.toolbar.snap_grid_tooltip",
                    snap.snapGridEnabled);
        snapToggle("A",
                    "editor.panel.toolbar.snap_angle_tooltip",
                    snap.snapAngleEnabled);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);  // WindowBorderSize + WindowPadding push del prologo.
}

// F3H20 iter 6: status bar de snap arriba del viewport, estilo Blender.
// Muestra chips para cada snap mode activo con su step/valor actual:
//   "Grid 0.5"   (cuando snapGridEnabled)
//   "Angle 15°"  (cuando snapAngleEnabled)
// Click sobre cada chip cicla su step (forward; backward via Ctrl+- en
// el global handler para grid). Cuando no hay snap activo, el status bar
// no se renderiza — sin ruido visual cuando no hace falta.
void drawViewportSnapStatusBar(EditorUI* ui, ImVec2 imageMin,
                                ImVec2 imageSize) {
    if (ui == nullptr) return;
    Project* project = ui->currentProject();
    if (project == nullptr) return;
    auto& snap = project->settings.snap;
    if (!snap.snapGridEnabled && !snap.snapAngleEnabled) {
        return;
    }

    constexpr float kTopOffset = 8.0f;
    // Centro horizontal del viewport. AutoResize hace que el ancho de la
    // ventana se ajuste al contenido; pivot 0.5 centra respecto al X dado.
    ImGui::SetNextWindowPos(
        ImVec2(imageMin.x + imageSize.x * 0.5f, imageMin.y + kTopOffset),
        ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    if (!ImGui::Begin("##viewport_snap_status", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // Chip helper: SmallButton con label "Mode val". Click invoca onCycle.
    auto chip = [&](const char* label, std::function<void()> onCycle,
                    const char* tooltipKey) {
        if (ImGui::SmallButton(label)) onCycle();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", I18n::T(tooltipKey).c_str());
        }
    };

    // Helper para ciclar un step en una lista circular.
    auto cycleStep = [&](f32& current, const f32* steps, int count) {
        int idx = 0;
        for (int i = 0; i < count; ++i) {
            if (std::abs(steps[i] - current) < 1e-4f) { idx = i; break; }
        }
        idx = (idx + 1) % count;
        current = steps[idx];
        ui->setProjectDirty(true);
    };

    bool first = true;
    if (snap.snapGridEnabled) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Grid %.3g", snap.snapGridStep);
        constexpr f32 k_gridSteps[] = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
        chip(buf, [&]() {
            cycleStep(snap.snapGridStep, k_gridSteps,
                       sizeof(k_gridSteps) / sizeof(f32));
        }, "editor.panel.toolbar.snap_grid_step_tooltip");
        first = false;
    }
    if (snap.snapAngleEnabled) {
        if (!first) { ImGui::SameLine(); }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Angle %.4g\xc2\xb0", snap.snapAngleDegrees);
        constexpr f32 k_angleSteps[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f};
        chip(buf, [&]() {
            cycleStep(snap.snapAngleDegrees, k_angleSteps,
                       sizeof(k_angleSteps) / sizeof(f32));
        }, "editor.panel.toolbar.snap_angle_step_tooltip");
        first = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace

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

    // Detectar si hay un drag-drop activo de un asset compatible. ImGui
    // expone el payload "actual" (no consumido) con `GetDragDropPayload()`;
    // usamos su tipo para decidir si mostrar el highlight cyan del tile bajo
    // el cursor en este frame. Sin drag activo el highlight no aparece, asi
    // que mover la camara no muestra ningun cubo flotante.
    m_assetDragKind = AssetDragKind::None;
    if (const ImGuiPayload* p = ImGui::GetDragDropPayload(); p != nullptr) {
        if      (p->IsDataType("MOOD_TEXTURE_ASSET"))  m_assetDragKind = AssetDragKind::Texture;
        else if (p->IsDataType("MOOD_MESH_ASSET"))     m_assetDragKind = AssetDragKind::Mesh;
        else if (p->IsDataType("MOOD_PREFAB_ASSET"))   m_assetDragKind = AssetDragKind::Prefab;
        else if (p->IsDataType("MOOD_MATERIAL_ASSET")) m_assetDragKind = AssetDragKind::Material;
        else if (p->IsDataType("MOOD_SCRIPT_ASSET"))   m_assetDragKind = AssetDragKind::Script;
        else if (p->IsDataType("MOOD_ITEM_ASSET"))     m_assetDragKind = AssetDragKind::Item;
        else if (p->IsDataType("MOOD_VEHICLE_ASSET"))  m_assetDragKind = AssetDragKind::Vehicle;
    }

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

            // F2H59: overlay flotante de herramientas estilo Blender. Se
            // pinta como sub-window posicionada al top-left de la imagen
            // del viewport. Reemplaza al panel Toolbar separado pre-F2H59.
            // El overlay aparece DESPUES del drawlist callback para que
            // los iconos no queden ocultos por gizmos / outlines.
            drawViewportToolsOverlay(m_editorUi, imageMin);
            // F3H20 iter 6: status bar arriba del viewport mostrando los
            // steps de cada snap activo (Grid/Angle/Scale). Estilo Blender.
            drawViewportSnapStatusBar(m_editorUi, imageMin, imageSize);

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
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_MATERIAL_ASSET")) {
                    u32 id = 0;
                    if (payload->DataSize == sizeof(id)) {
                        std::memcpy(&id, payload->Data, sizeof(id));
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingMaterialDrop =
                            MaterialDrop{true, ndcX, ndcY, id};
                    }
                }
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_SCRIPT_ASSET")) {
                    // Payload es un buffer fijo (256 bytes) con el path
                    // logico null-terminated. AssetBrowserPanel lo arma asi.
                    if (payload->DataSize > 0) {
                        const char* str = static_cast<const char*>(payload->Data);
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingScriptDrop = ScriptDrop{
                            true, ndcX, ndcY, std::string{str}};
                    }
                }
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_ITEM_ASSET")) {
                    // F2H52 Bloque D: payload es el path logico
                    // null-terminated emitido por ItemBrowserPanel
                    // (relativo a assets/, ej. "items/iron_sword.mooditem").
                    if (payload->DataSize > 0) {
                        const char* str = static_cast<const char*>(payload->Data);
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingItemDrop = ItemDrop{
                            true, ndcX, ndcY, std::string{str}};
                    }
                }
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("MOOD_VEHICLE_ASSET")) {
                    // F2H70.3 Bloque F: payload es el path logico del
                    // .moodvehicle (buffer fijo 256 bytes null-terminated,
                    // mismo formato que MOOD_SCRIPT_ASSET).
                    if (payload->DataSize > 0) {
                        const char* str = static_cast<const char*>(payload->Data);
                        float ndcX = 0.0f;
                        float ndcY = 0.0f;
                        mousePosToNdc(ImGui::GetMousePos(), ndcX, ndcY);
                        m_pendingVehicleDrop = VehicleDrop{
                            true, ndcX, ndcY, std::string{str}};
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Captura de input para la camara + hover del viewport. Importante:
            // AllowWhenBlockedByActiveItem mantiene el hover activo durante
            // drags (sin eso, al arrastrar una textura se pierde el cyan).
            const bool hovered = ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

            // F3H17: el feedback visual de drag dentro del viewport se hace
            // sobre el TARGET especifico (cubo cyan en tile / outline en
            // entity bajo cursor), no en el borde del panel. La logica vive
            // en `EditorRenderPass_Overlay.cpp` (overlay 3D del editor) que
            // ya tiene acceso a Scene + cameras + `pickEntity`.
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
                // F3H7: umbral leido live de UserSettings. Comparacion
                // al cuadrado para evitar sqrt.
                const float thresh = static_cast<float>(
                    UserSettings::editor().clickDragThresholdPx);
                const bool wasClick = (dx*dx + dy*dy) < thresh*thresh;
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
            const std::string placeholder = I18n::T("editor.panel.viewport.no_framebuffer");
            const ImVec2 textSize = ImGui::CalcTextSize(placeholder.c_str());
            ImGui::SetCursorPos(ImVec2(
                (avail.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
                (avail.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()));
            ImGui::TextUnformatted(placeholder.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Mood
