#include "editor/panels/debug/ConsolePanel.h"

#include "core/Log.h"
#include "core/LogRingSink.h"
#include "core/Types.h"  // F2H23: usize
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons por nivel
#include "engine/i18n/I18n.h"  // F2H43

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

// F2H37: icono FA por nivel. Pre-F2H37 los logs solo tenian color +
// tag de 3 chars; con icon el tipo se reconoce de un vistazo aun
// si la columna del log es chica.
const char* iconForLevel(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::trace:    return ICON_FA_BUG;
        case spdlog::level::debug:    return ICON_FA_BUG_SLASH;
        case spdlog::level::info:     return ICON_FA_CIRCLE_INFO;
        case spdlog::level::warn:     return ICON_FA_TRIANGLE_EXCLAMATION;
        case spdlog::level::err:      return ICON_FA_CIRCLE_XMARK;
        case spdlog::level::critical: return ICON_FA_SKULL;
        default:                      return ICON_FA_CIRCLE_INFO;
    }
}

// F2H37: indice 0..5 a level_enum (para iterar los 6 toggles del UI).
spdlog::level::level_enum levelFromIndex(int i) {
    switch (i) {
        case 0: return spdlog::level::trace;
        case 1: return spdlog::level::debug;
        case 2: return spdlog::level::info;
        case 3: return spdlog::level::warn;
        case 4: return spdlog::level::err;
        case 5: return spdlog::level::critical;
        default: return spdlog::level::info;
    }
}

int indexFromLevel(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::trace:    return 0;
        case spdlog::level::debug:    return 1;
        case spdlog::level::info:     return 2;
        case spdlog::level::warn:     return 3;
        case spdlog::level::err:      return 4;
        case spdlog::level::critical: return 5;
        default: return 2;
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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.console.no_log_init").c_str());
        ImGui::End();
        return;
    }

    // F2H23: toolbar reorganizada.
    // - Boton "Limpiar" abre popup de confirmacion (accion destructiva
    //   irreversible — sin undo).
    // - Auto-scroll checkbox sin cambios.
    // - Input de filtro ancho dinamico (40% del panel).
    // - Hint "(?)" con leyenda de los tags TRC/DBG/INF/WRN/ERR/CRT.
    const std::string clearBtnLabel = std::string(ICON_FA_CIRCLE_XMARK " ") +
        I18n::T("editor.panel.console.clear");
    if (ImGui::Button(clearBtnLabel.c_str())) {
        ImGui::OpenPopup("##confirm_clear");
    }
    if (ImGui::BeginPopup("##confirm_clear")) {
        ImGui::TextUnformatted(I18n::T("editor.panel.console.clear_confirm").c_str());
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.console.clear_warn").c_str());
        ImGui::Separator();
        if (ImGui::Button(I18n::T("editor.panel.console.clear").c_str())) {
            sink->clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::T("editor.panel.console.cancel").c_str())) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Checkbox(I18n::T("editor.panel.console.auto_scroll").c_str(), &m_autoScroll);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // F2H37: 6 toggles compactos para filtrar por nivel. Cada uno es un
    // boton con icon (sin label) que cambia color cuando esta activo
    // vs inactivo. Click toggle el bool del array m_levelEnabled.
    {
        constexpr const char* kLevelLabels[6] = {
            "Trace", "Debug", "Info", "Warning", "Error", "Critical"
        };
        for (int i = 0; i < 6; ++i) {
            const auto lvl = levelFromIndex(i);
            const char* icon = iconForLevel(lvl);
            const bool on = m_levelEnabled[i];
            ImVec4 col = colorForLevel(lvl);
            if (!on) col.w = 0.30f; // tenue cuando filtrado
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::PushStyleColor(ImGuiCol_Button,
                on ? ImVec4(0.20f, 0.20f, 0.25f, 1.0f)
                   : ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
            char id[16];
            std::snprintf(id, sizeof(id), "%s##lv%d", icon, i);
            if (ImGui::SmallButton(id)) {
                m_levelEnabled[i] = !on;
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s %s",
                    I18n::T(on ? "editor.panel.console.hide"
                                : "editor.panel.console.show").c_str(),
                    kLevelLabels[i]);
            }
            if (i < 5) ImGui::SameLine();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Input de filtro: 40% del ancho disponible, minimo 140px.
    const float filterWidth =
        std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.40f);
    ImGui::SetNextItemWidth(filterWidth);
    ImGui::InputTextWithHint("##channel",
                                I18n::T("editor.panel.console.filter_hint").c_str(),
                                m_channelFilter.data(),
                                m_channelFilter.size());

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Niveles de log (icon + tag + color):\n"
            "  " ICON_FA_BUG                    " TRC = Trace     (gris)\n"
            "  " ICON_FA_BUG_SLASH              " DBG = Debug     (celeste)\n"
            "  " ICON_FA_CIRCLE_INFO            " INF = Info      (blanco)\n"
            "  " ICON_FA_TRIANGLE_EXCLAMATION   " WRN = Warning   (amarillo)\n"
            "  " ICON_FA_CIRCLE_XMARK           " ERR = Error     (rojo)\n"
            "  " ICON_FA_SKULL                  " CRT = Critical  (magenta)\n"
            "\n"
            "Toggles a la izquierda: click para mostrar/ocultar cada nivel.\n"
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
            // F2H37: filtro por nivel (toggles arriba). Si el nivel del
            // entry esta off, skipear sin contar como visible.
            const int idx = indexFromLevel(e.level);
            if (!m_levelEnabled[idx]) continue;
            if (hasFilter && e.channel.find(filter) == std::string::npos) continue;
            ++visibleCount;
            ImGui::PushStyleColor(ImGuiCol_Text, colorForLevel(e.level));
            // F2H37: prefijo icon FA antes del tag de 3 chars. Tag se
            // mantiene para que copy-paste de la console preserve el
            // texto buscable (los icons no se ven en pegado).
            ImGui::Text("%s [%s] [%s] %s", iconForLevel(e.level),
                        levelTag(e.level), e.channel.c_str(),
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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.console.lines_filtered",
                    visibleCount, totalCount).c_str());
    } else {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.console.lines",
                    totalCount).c_str());
    }

    ImGui::End();
}

} // namespace Mood
