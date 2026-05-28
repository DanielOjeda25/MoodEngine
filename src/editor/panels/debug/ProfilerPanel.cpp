#include "editor/panels/debug/ProfilerPanel.h"

#include "core/UserSettings.h"
#include "core/i18n/I18n.h"
#include "engine/profile/ProfilerBuffer.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace Mood {

void ProfilerPanel::onImGuiRender() {
    if (!visible) return;

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin(name(), &visible, flags)) {
        ImGui::End();
        return;
    }

    const auto& buf = profilerBuffer();
    const u32 cap = buf.frameCapacity();
    const u32 recorded = buf.framesRecorded();

    // Header: capacity + frames con data + último frame total ms.
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.profiler.header",
                static_cast<int>(recorded),
                static_cast<int>(cap),
                static_cast<double>(buf.lastFrameTotalMs())).c_str());

    // Ventana de agregación. 0 = todos los frames con data.
    ImGui::PushItemWidth(140.0f);
    int window = m_aggregateWindow;
    if (ImGui::SliderInt(
            I18n::T("editor.panel.profiler.window").c_str(),
            &window, 1, static_cast<int>(cap))) {
        m_aggregateWindow = window;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Checkbox(
        I18n::T("editor.panel.profiler.show_histogram").c_str(),
        &m_showHistogram);

    ImGui::Separator();

    if (recorded == 0) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.profiler.empty").c_str());
        ImGui::End();
        return;
    }

    // Tabla de scopes ordenada por avg desc.
    const u32 windowFrames = (m_aggregateWindow > 0)
        ? std::min(static_cast<u32>(m_aggregateWindow), recorded)
        : recorded;
    const auto rows = buf.aggregate(windowFrames);
    const f32 lastTotalMs = buf.lastFrameTotalMs();

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    const ImVec2 tableSize(0.0f, m_showHistogram ? 220.0f : 0.0f);
    if (ImGui::BeginTable("##profiler_table", 7, kTableFlags, tableSize)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.scope").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.avg").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.min").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.max").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.last").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.hits").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(I18n::T("editor.panel.profiler.col.percent").c_str(),
                                 ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        for (const auto& row : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name ? row.name : "?");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f ms", static_cast<double>(row.avgMs));
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f ms", static_cast<double>(row.minMs));
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f ms", static_cast<double>(row.maxMs));
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f ms", static_cast<double>(row.lastMs));
            ImGui::TableSetColumnIndex(5); ImGui::Text("%u", row.hitsPerFrame);
            ImGui::TableSetColumnIndex(6);
            const f32 pct = (lastTotalMs > 0.0f)
                ? (row.lastMs / lastTotalMs) * 100.0f : 0.0f;
            ImGui::Text("%.1f %%", static_cast<double>(pct));
        }
        ImGui::EndTable();
    }

    if (m_showHistogram) {
        // Histograma del total ms del último frame del ring — vista
        // global del "frame time instrumentado". ImGui::PlotHistogram
        // necesita un array contiguo de f32 — armamos sobre la marcha
        // recorriendo lastFrame() de los últimos N samples (no del ring
        // completo, sería caro). Substitute realista: agrupar el
        // último frame por scope name y graficar.
        ImGui::Spacing();
        ImGui::SeparatorText(
            I18n::T("editor.panel.profiler.last_frame").c_str());
        const auto& last = buf.lastFrame();
        if (last.empty()) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.profiler.last_frame_empty").c_str());
        } else {
            std::vector<f32> data;
            std::vector<const char*> labels;
            data.reserve(last.size());
            labels.reserve(last.size());
            for (const auto& s : last) {
                data.push_back(s.milliseconds);
                labels.push_back(s.name);
            }
            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "total %.3f ms",
                          static_cast<double>(lastTotalMs));
            ImGui::PlotHistogram("##profiler_last_frame_hist",
                                  data.data(), static_cast<int>(data.size()),
                                  0, overlay, 0.0f, FLT_MAX,
                                  ImVec2(0.0f, 120.0f));
            // Mostrar los top 5 nombres para que el usuario sepa qué barra
            // es cada cosa (PlotHistogram no acepta labels).
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.profiler.legend_hint").c_str());
            const usize topN = std::min<usize>(5, labels.size());
            for (usize i = 0; i < topN; ++i) {
                ImGui::BulletText("%s: %.3f ms",
                                    labels[i],
                                    static_cast<double>(data[i]));
            }
        }
    }

    ImGui::End();
}

} // namespace Mood
