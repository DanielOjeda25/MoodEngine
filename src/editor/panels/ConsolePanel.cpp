#include "editor/panels/ConsolePanel.h"

#include "core/Log.h"
#include "core/LogRingSink.h"

#include <imgui.h>

#include <cstring>

namespace Mood {

namespace {

ImVec4 colorForLevel(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::trace:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
        case spdlog::level::debug:    return ImVec4(0.65f, 0.80f, 1.00f, 1.0f);
        case spdlog::level::info:     return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        case spdlog::level::warn:     return ImVec4(1.00f, 0.85f, 0.30f, 1.0f);
        case spdlog::level::err:      return ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
        case spdlog::level::critical: return ImVec4(1.00f, 0.15f, 0.60f, 1.0f);
        default:                      return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    }
}

const char* levelTag(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::trace:    return "TRC";
        case spdlog::level::debug:    return "DBG";
        case spdlog::level::info:     return "INF";
        case spdlog::level::warn:     return "WRN";
        case spdlog::level::err:      return "ERR";
        case spdlog::level::critical: return "CRT";
        default:                      return "???";
    }
}

} // namespace

void ConsolePanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    LogRingSink* sink = Log::ringSink();
    if (sink == nullptr) {
        ImGui::TextDisabled("Log::init() no se llamo aun.");
        ImGui::End();
        return;
    }

    // Toolbar: limpiar, auto-scroll, filtro por canal.
    if (ImGui::Button("Limpiar")) sink->clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##channel", "filtro canal", m_channelFilter.data(),
                             m_channelFilter.size());
    ImGui::Separator();

    const auto entries = sink->snapshot();
    const char* filter = m_channelFilter.data();
    const bool hasFilter = filter[0] != '\0';

    if (ImGui::BeginChild("##log", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& e : entries) {
            if (hasFilter && e.channel.find(filter) == std::string::npos) continue;
            ImGui::PushStyleColor(ImGuiCol_Text, colorForLevel(e.level));
            ImGui::Text("[%s] [%s] %s", levelTag(e.level), e.channel.c_str(),
                        e.text.c_str());
            ImGui::PopStyleColor();
        }
        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace Mood
