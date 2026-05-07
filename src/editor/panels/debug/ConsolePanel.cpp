#include "editor/panels/debug/ConsolePanel.h"

#include "core/Log.h"
#include "core/LogRingSink.h"
#include "core/Types.h"  // F2H23: usize

#include <imgui.h>

#include <algorithm>     // F2H23: std::max
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

    // F2H23: toolbar reorganizada.
    // - Boton "Limpiar" abre popup de confirmacion (accion destructiva
    //   irreversible — sin undo).
    // - Auto-scroll checkbox sin cambios.
    // - Input de filtro ancho dinamico (40% del panel).
    // - Hint "(?)" con leyenda de los tags TRC/DBG/INF/WRN/ERR/CRT.
    if (ImGui::Button("Limpiar")) {
        ImGui::OpenPopup("##confirm_clear");
    }
    if (ImGui::BeginPopup("##confirm_clear")) {
        ImGui::TextUnformatted("Limpiar todos los logs?");
        ImGui::TextDisabled("Esta accion no se puede deshacer.");
        ImGui::Separator();
        if (ImGui::Button("Limpiar")) {
            sink->clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Input de filtro: 40% del ancho disponible, minimo 140px.
    const float filterWidth =
        std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.40f);
    ImGui::SetNextItemWidth(filterWidth);
    ImGui::InputTextWithHint("##channel", "filtro canal (substring)",
                                m_channelFilter.data(),
                                m_channelFilter.size());

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Niveles de log (color + tag):\n"
            "  TRC = Trace     (gris)\n"
            "  DBG = Debug     (celeste)\n"
            "  INF = Info      (blanco)\n"
            "  WRN = Warning   (amarillo)\n"
            "  ERR = Error     (rojo)\n"
            "  CRT = Critical  (magenta)\n"
            "\n"
            "Filtro canal: substring match contra el nombre del canal\n"
            "(engine / render / assets / editor / world / etc).");
    }
    ImGui::Separator();

    const auto entries = sink->snapshot();
    const char* filter = m_channelFilter.data();
    const bool hasFilter = filter[0] != '\0';
    const auto totalCount = entries.size();
    usize visibleCount = 0;

    if (ImGui::BeginChild("##log", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& e : entries) {
            if (hasFilter && e.channel.find(filter) == std::string::npos) continue;
            ++visibleCount;
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

    // F2H23: counter en el footer del panel — N visibles / M totales.
    if (hasFilter) {
        ImGui::TextDisabled("%zu de %zu lineas (filtro activo)",
                              visibleCount, totalCount);
    } else {
        ImGui::TextDisabled("%zu lineas", totalCount);
    }

    ImGui::End();
}

} // namespace Mood
