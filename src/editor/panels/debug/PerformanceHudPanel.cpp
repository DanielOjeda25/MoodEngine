#include "editor/panels/debug/PerformanceHudPanel.h"

#include <imgui.h>

namespace Mood {

void PerformanceHudPanel::onImGuiRender() {
    if (!visible) return;

    // Window pequeno, top-right por defecto. NoResize para que tenga el
    // tamano justo del contenido. Movible y colapsable.
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin(name(), &visible, flags)) {
        ImGui::End();
        return;
    }

    // FPS / frame time. Color rojo si FPS < 30, amarillo si < 60, verde >=60.
    ImVec4 fpsColor(0.4f, 0.95f, 0.4f, 1.0f); // verde
    if (m_metrics.fps < 30.0f) {
        fpsColor = ImVec4(1.0f, 0.35f, 0.35f, 1.0f); // rojo
    } else if (m_metrics.fps < 60.0f) {
        fpsColor = ImVec4(1.0f, 0.85f, 0.25f, 1.0f); // amarillo
    }
    ImGui::TextUnformatted("FPS:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
    ImGui::Text("%.1f", static_cast<double>(m_metrics.fps));
    ImGui::PopStyleColor();

    ImGui::Text("Frame: %.2f ms",
                 static_cast<double>(m_metrics.frameMs));

    ImGui::Separator();

    // Draw calls + tris (counter del IRenderer, solo cubre PBR + skinned +
    // shadow; skybox/particles/debug NO se cuentan — documentado en
    // IRenderer.h).
    ImGui::Text("Draw calls: %u", m_metrics.drawCalls);
    if (m_metrics.triangles >= 1'000'000) {
        ImGui::Text("Tris:       %.2f M",
                     static_cast<double>(m_metrics.triangles) / 1'000'000.0);
    } else if (m_metrics.triangles >= 1'000) {
        ImGui::Text("Tris:       %.1f K",
                     static_cast<double>(m_metrics.triangles) / 1'000.0);
    } else {
        ImGui::Text("Tris:       %u", m_metrics.triangles);
    }

    ImGui::Separator();

    ImGui::Text("Entities:   %u", m_metrics.entityCount);

    ImGui::End();
}

} // namespace Mood
