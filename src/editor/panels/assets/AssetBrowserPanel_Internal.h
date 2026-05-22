#pragma once

// F2H81 (auditoría): helpers visuales compartidos por los `render*Tab()` de
// AssetBrowserPanel, que viven repartidos entre AssetBrowserPanel.cpp (shell +
// scan) y AssetBrowserPanel_Tabs.cpp (cuerpo de cada tab). Header privado del
// panel — no incluir desde otro módulo.

#include <imgui.h>

#include <algorithm>
#include <string>

namespace Mood::assetbrowser_detail {

// Botón con ícono grande centrado (escala de fuente 2.6x) para los tabs sin
// preview 3D (Scripts / Prefabs / Audio). El caller envuelve en
// PushID/BeginGroup y agrega el drag-source justo después del botón.
inline bool bigIconButton(const char* icon, float size) {
    ImGui::SetWindowFontScale(2.6f);
    const bool clicked = ImGui::Button(icon, ImVec2(size, size));
    ImGui::SetWindowFontScale(1.0f);
    return clicked;
}

// Label de card truncado con ".." si excede `size` (ancho de la card). Unifica
// la lógica de truncado que se repetía inline en cada tab.
inline void cardLabel(const std::string& label, float size) {
    if (ImGui::CalcTextSize(label.c_str()).x <= size) {
        ImGui::TextUnformatted(label.c_str());
        return;
    }
    std::string truncated = label;
    while (!truncated.empty() &&
           ImGui::CalcTextSize((truncated + "..").c_str()).x > size) {
        truncated.pop_back();
    }
    ImGui::Text("%s..", truncated.c_str());
}

// Columnas que entran en el ancho disponible para una grilla de cards de lado
// `cardSize` (con 12px de gutter). Mínimo 1.
inline int cardGridCols(float cardSize) {
    const float availW = ImGui::GetContentRegionAvail().x;
    return std::max(1, static_cast<int>(availW / (cardSize + 12.0f)));
}

} // namespace Mood::assetbrowser_detail
