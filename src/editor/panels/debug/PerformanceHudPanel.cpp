#include "editor/panels/debug/PerformanceHudPanel.h"

#include "core/Log.h"
#include "engine/i18n/I18n.h"  // F2H43

#include <imgui.h>

#include <filesystem>
#include <fstream>

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
    ImGui::TextUnformatted(I18n::T("editor.panel.performance.fps").c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
    ImGui::Text("%.1f", static_cast<double>(m_metrics.fps));
    ImGui::PopStyleColor();

    ImGui::Text("%s",
        I18n::T("editor.panel.performance.frame",
                static_cast<double>(m_metrics.frameMs)).c_str());

    ImGui::Separator();

    // Draw calls + tris (counter del IRenderer, solo cubre PBR + skinned +
    // shadow; skybox/particles/debug NO se cuentan — documentado en
    // IRenderer.h).
    ImGui::Text("%s",
        I18n::T("editor.panel.performance.draw_calls",
                m_metrics.drawCalls).c_str());
    if (m_metrics.triangles >= 1'000'000) {
        ImGui::Text("%s",
            I18n::T("editor.panel.performance.tris_m",
                    static_cast<double>(m_metrics.triangles) / 1'000'000.0).c_str());
    } else if (m_metrics.triangles >= 1'000) {
        ImGui::Text("%s",
            I18n::T("editor.panel.performance.tris_k",
                    static_cast<double>(m_metrics.triangles) / 1'000.0).c_str());
    } else {
        ImGui::Text("%s",
            I18n::T("editor.panel.performance.tris",
                    m_metrics.triangles).c_str());
    }

    ImGui::Separator();

    ImGui::Text("%s",
        I18n::T("editor.panel.performance.entities",
                m_metrics.entityCount).c_str());

    ImGui::Separator();

    // F2H42: toggle VSync. Con VSync OFF el FPS se desbloquea del refresco
    // del monitor, util para medir el costo CPU/GPU real (con VSync ON el
    // frame time queda capado a 16.6ms ocultando ganancias de optimizacion).
    if (ImGui::Checkbox(I18n::T("editor.panel.performance.vsync").c_str(), &m_vsyncEnabled)) {
        m_vsyncToggleRequested = true;
    }

    ImGui::Separator();

    // F2H2 Bloque G: snapshot dump del estado actual al log + CSV. El dev
    // setea el label de la escena (Empty / 10K / 100K / 500K / 1M) y
    // clickea "Snapshot". Append-only — multiples clicks generan multiples
    // filas en el CSV (util si querés re-medir despues de estabilizar).
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText(I18n::T("editor.panel.performance.label").c_str(),
                       m_snapshotLabel, sizeof(m_snapshotLabel));
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.performance.snapshot").c_str())) {
        writeSnapshot();
    }

    ImGui::End();
}

void PerformanceHudPanel::writeSnapshot() {
    // CSV en el cwd del proceso (donde se lance MoodEditor.exe). Append-only:
    // si no existe, escribimos cabecera primero.
    const auto path = std::filesystem::current_path() / "performance_baseline.csv";
    const bool needHeader = !std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) {
        Log::editor()->warn("[F2H2_SNAPSHOT] no se pudo abrir '{}'",
                              path.generic_string());
        return;
    }
    if (needHeader) {
        out << "label,fps,frame_ms,draw_calls,tris,entities\n";
    }
    out << m_snapshotLabel << ','
        << m_metrics.fps    << ','
        << m_metrics.frameMs << ','
        << m_metrics.drawCalls << ','
        << m_metrics.triangles << ','
        << m_metrics.entityCount << '\n';
    out.flush();

    // Tambien al log con formato parseable: el dev (o un script) puede
    // recuperar las filas con `Select-String "F2H2_SNAPSHOT" mood.log`.
    Log::editor()->info(
        "[F2H2_SNAPSHOT] label={} fps={:.1f} frame_ms={:.2f} "
        "draws={} tris={} entities={}",
        m_snapshotLabel, m_metrics.fps, m_metrics.frameMs,
        m_metrics.drawCalls, m_metrics.triangles, m_metrics.entityCount);
}

} // namespace Mood
