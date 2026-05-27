#pragma once

// F3H17: helpers de feedback visual de drag & drop. Cuando el dev
// arrastra un asset desde el Asset Browser (o una entity desde el
// Hierarchy), los drop targets validos del editor (viewports +
// Inspector slots compatibles) reciben un halo / borde brillante. La
// idea estilo Substance/Unreal: el dev sabe inmediatamente "puedo
// soltar acá" sin tener que adivinar.

#include <imgui.h>
#include <imgui_internal.h>  // F3H17: acceso a GImGui->DragDropActive para cancel con Esc

#include <cstring>

namespace Mood::DragDropFeedback {

/// @brief Tipos de payload que el viewport (perspectiva + ortos) acepta
///        para spawn de entidad o asignacion contextual. Mantener en
///        sync con `ViewportPanel.cpp::onImGuiRender` AcceptDragDropPayload
///        calls.
constexpr const char* kViewportSupportedTypes[] = {
    "MOOD_TEXTURE_ASSET",
    "MOOD_MESH_ASSET",
    "MOOD_PREFAB_ASSET",
    "MOOD_MATERIAL_ASSET",
    "MOOD_SCRIPT_ASSET",
    "MOOD_ITEM_ASSET",
    "MOOD_VEHICLE_ASSET",
};
constexpr int kViewportSupportedCount =
    sizeof(kViewportSupportedTypes) / sizeof(kViewportSupportedTypes[0]);

/// @brief True si hay un drag activo este frame de cualquier tipo
///        listado en `kViewportSupportedTypes`. Util para el halo del
///        viewport (que acepta TODOS esos tipos — un solo helper).
inline bool isViewportDragActive() {
    const ImGuiPayload* p = ImGui::GetDragDropPayload();
    if (p == nullptr) return false;
    for (int i = 0; i < kViewportSupportedCount; ++i) {
        if (p->IsDataType(kViewportSupportedTypes[i])) return true;
    }
    return false;
}

/// @brief Pinta un halo rectangular sobre el area `[min, max]` del
///        draw list dado. Color cyan-brillante si el cursor NO esta
///        sobre el target; verde si SI esta sobre el target (drop
///        confirmado al soltar).
///
///        Llamar SOLO cuando hay drag activo (chequea isViewportDragActive
///        o equivalente antes). El thickness es proporcional al spacing
///        del frame para que se vea consistente entre DPIs.
inline void drawDropHalo(ImDrawList* drawList,
                          const ImVec2& min, const ImVec2& max,
                          bool cursorOver, float thickness = 3.0f) {
    if (drawList == nullptr) return;
    const ImU32 colorCyan  = IM_COL32(80, 180, 255, 200);  // drop possible
    const ImU32 colorGreen = IM_COL32(80, 230, 130, 230);  // drop now
    const ImU32 color = cursorOver ? colorGreen : colorCyan;
    drawList->AddRect(min, max, color, 0.0f, 0, thickness);
}

/// @brief True si hay drag activo de tipo `type` (single check, mismo
///        helper que existia en InspectorPanel_Internal.h pero re-exportado
///        aca para que los Inspector slot panels NO tengan que incluir
///        el Internal del Inspector). Usado por los slots de
///        Animation / MeshRenderer / Vehicle / Joint / Inventory.
inline bool isDragActiveOfType(const char* type) {
    const ImGuiPayload* p = ImGui::GetDragDropPayload();
    return p != nullptr && p->IsDataType(type);
}

/// @brief F3H17: cancela el drag activo (si hay) cuando el dev pulsa
///        Esc. Llamar UNA vez por frame, despues de `ImGui::NewFrame()`
///        y antes de los `BeginDragDropSource/Target` (sino los handlers
///        del frame actual ya ejecutaron). Accede al context interno de
///        ImGui — la API publica no expone cancel.
inline void cancelDragOnEscape() {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr) return;
    if (!ctx->DragDropActive) return;
    if (!ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) return;
    // Cancel: limpiamos los flags del DragDrop. El payload queda en
    // memoria pero ningun target lo aceptara (los handlers se chequean
    // contra DragDropActive). Al soltar el mouse, ImGui hara cleanup
    // residual normal.
    ctx->DragDropActive            = false;
    ctx->DragDropPayload.Clear();
    ctx->DragDropSourceFlags       = 0;
    ctx->DragDropAcceptIdCurr      = 0;
    ctx->DragDropAcceptIdPrev      = 0;
}

/// @brief Pinta un halo sutil alrededor del ultimo item dibujado.
///        Para Inspector slots — el rect del item ya esta calculado
///        por ImGui en `GetItemRectMin/Max`. Llamar inmediatamente
///        despues del widget (Button/Image) que es drop target.
inline void drawItemDropHalo(bool cursorOver) {
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    // Thickness 2 px para slots chicos del Inspector (vs 3 px del
    // viewport, donde hay mas area que llenar).
    drawDropHalo(ImGui::GetWindowDrawList(), min, max, cursorOver, 2.0f);
}

} // namespace Mood::DragDropFeedback
