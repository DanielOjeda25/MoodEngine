#include "editor/panels/debug/ConsolePanel.h"

#include "core/Log.h"
#include "core/LogRingSink.h"
#include "core/Toasts.h"  // F3H24: toast tras "Copy as bug report"
#include "core/Types.h"  // F2H23: usize
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons por nivel
#include "core/i18n/I18n.h"  // F2H43

#include <imgui.h>

#include <algorithm>     // F2H23: std::max
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shellapi.h>  // F3H24: ShellExecuteA para abrir .lua:N en editor externo
#endif

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

    // F3H24: input de filtro por MENSAJE (case-insensitive). Reemplaza
    // el viejo filtro por channel — el dev encuentra entries por
    // keyword del bug ("vehicle", "shader", "missing texture") sin
    // saber en qué channel del logger se emitio.
    const float filterWidth =
        std::max(140.0f, ImGui::GetContentRegionAvail().x * 0.32f);
    ImGui::SetNextItemWidth(filterWidth);
    ImGui::InputTextWithHint("##msg_filter",
                                I18n::T("editor.panel.console.search_hint").c_str(),
                                m_messageFilter.data(),
                                m_messageFilter.size());

    // F3H24: boton "Copy as bug report" — formatea los entries filtrados
    // (mismas reglas de visibilidad que el render) y los copia al
    // clipboard del SO. Util para pegar en issues / Discord / etc.
    ImGui::SameLine();
    const std::string copyBtnLabel = std::string(ICON_FA_PASTE " ") +
        I18n::T("editor.panel.console.copy_bug_report");
    const bool copyClicked = ImGui::Button(copyBtnLabel.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.console.copy_bug_report.tooltip").c_str());
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        // F2H45 Bloque C: tooltip multilinea via i18n. Los icons FA viven
        // en macros (`#define ICON_FA_X "..."`) y se mantienen en codigo
        // — el JSON no puede cargarlos sin perder el byte sequence UTF-8.
        // Cada linea de nivel se compone aqui: dos espacios + icon +
        // espacio + I18n::T(level_label).
        const std::string body =
            I18n::T("editor.panel.console.help.header") + "\n"
            "  " ICON_FA_BUG                  " " + I18n::T("editor.panel.console.help.level.trace")    + "\n"
            "  " ICON_FA_BUG_SLASH            " " + I18n::T("editor.panel.console.help.level.debug")    + "\n"
            "  " ICON_FA_CIRCLE_INFO          " " + I18n::T("editor.panel.console.help.level.info")     + "\n"
            "  " ICON_FA_TRIANGLE_EXCLAMATION " " + I18n::T("editor.panel.console.help.level.warn")     + "\n"
            "  " ICON_FA_CIRCLE_XMARK         " " + I18n::T("editor.panel.console.help.level.err")      + "\n"
            "  " ICON_FA_SKULL                " " + I18n::T("editor.panel.console.help.level.critical") + "\n"
            "\n" + I18n::T("editor.panel.console.help.footer");
        ImGui::SetTooltip("%s", body.c_str());
    }
    ImGui::Separator();

    const auto entries = sink->snapshot();
    const char* filter = m_messageFilter.data();
    const bool hasFilter = filter[0] != '\0';
    const auto totalCount = entries.size();
    usize visibleCount = 0;

    // F3H24: case-insensitive substring match. Lower-case del filter una
    // vez fuera del loop (el haystack se cae per-entry).
    std::string filterLower;
    if (hasFilter) {
        filterLower = std::string(filter);
        for (auto& c : filterLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    auto matchFilter = [&](const std::string& text, const std::string& channel) {
        if (!hasFilter) return true;
        std::string hay;
        hay.reserve(text.size() + channel.size() + 1);
        hay = text;
        hay.push_back(' ');
        hay.append(channel);
        for (auto& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return hay.find(filterLower) != std::string::npos;
    };

    // F3H24: detector simple de path `.lua:N` (o `.lua` sin línea) para
    // el botón "ir" — encuentra el inicio del path desde la primera
    // posición del .lua hacia atrás hasta un whitespace / inicio. No es
    // un regex completo; basta para los mensajes típicos del Lua VM.
    auto findLuaPath = [](const std::string& text) -> std::string {
        const auto pos = text.find(".lua");
        if (pos == std::string::npos) return {};
        // Walk hacia atrás hasta espacio / `(` / `[` / `'` / `"` / inicio.
        usize start = pos;
        while (start > 0) {
            const char c = text[start - 1];
            if (c == ' ' || c == '\t' || c == '(' || c == '[' ||
                c == '\'' || c == '"' || c == '<') break;
            --start;
        }
        // Walk hacia adelante hasta `:N` / whitespace / cerrar paréntesis.
        usize end = pos + 4;  // después de ".lua"
        if (end < text.size() && text[end] == ':') {
            ++end;
            while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
        }
        return text.substr(start, end - start);
    };

    // F3H24: handler Copy as bug report. Recolecta los entries que se
    // VAN a renderizar (mismas reglas que el render) y los copia al
    // clipboard formateados con tag de nivel + channel + text.
    if (copyClicked) {
        std::ostringstream oss;
        oss << "MoodEngine — Console export (" << totalCount << " total entries)\n";
        oss << "---\n";
        usize copied = 0;
        for (const auto& e : entries) {
            const int idx = indexFromLevel(e.level);
            if (!m_levelEnabled[idx]) continue;
            if (!matchFilter(e.text, e.channel)) continue;
            oss << "[" << levelTag(e.level) << "] [" << e.channel << "] "
                << e.text << "\n";
            ++copied;
        }
        ImGui::SetClipboardText(oss.str().c_str());
        Toasts::pushSuccess(
            I18n::T("editor.toast.console_copied", static_cast<int>(copied)));
    }

    if (ImGui::BeginChild("##log", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& e : entries) {
            // F2H37: filtro por nivel (toggles arriba). Si el nivel del
            // entry esta off, skipear sin contar como visible.
            const int idx = indexFromLevel(e.level);
            if (!m_levelEnabled[idx]) continue;
            // F3H24: filtro por mensaje (case-insensitive).
            if (!matchFilter(e.text, e.channel)) continue;
            ++visibleCount;
            ImGui::PushStyleColor(ImGuiCol_Text, colorForLevel(e.level));
            // F2H37: prefijo icon FA antes del tag de 3 chars. Tag se
            // mantiene para que copy-paste de la console preserve el
            // texto buscable (los icons no se ven en pegado).
            ImGui::Text("%s [%s] [%s] %s", iconForLevel(e.level),
                        levelTag(e.level), e.channel.c_str(),
                        e.text.c_str());
            ImGui::PopStyleColor();

            // F3H24: si el entry menciona un `.lua` path, agregar boton
            // chico "[ir]" al lado para abrirlo con el editor default
            // del SO (ShellExecute "open"). Out-of-scope: paths que no
            // sean `.lua` (.material/.lua-mod/etc) — hito propio si
            // emerge demanda.
#if defined(_WIN32)
            const std::string luaPath = findLuaPath(e.text);
            if (!luaPath.empty()) {
                ImGui::SameLine();
                const std::string btnId = std::string(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE)
                                            + "##go_" + std::to_string(visibleCount);
                if (ImGui::SmallButton(btnId.c_str())) {
                    // Separar path real de `:N` (ShellExecute no acepta `:N`).
                    std::string fileOnly = luaPath;
                    const auto colon = fileOnly.find(".lua:");
                    if (colon != std::string::npos) {
                        fileOnly = fileOnly.substr(0, colon + 4);  // incluye ".lua"
                    }
                    const HINSTANCE r = ShellExecuteA(
                        nullptr, "open", fileOnly.c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
                    if (reinterpret_cast<INT_PTR>(r) <= 32) {
                        Log::editor()->warn(
                            "[console] no se pudo abrir '{}' (ShellExecute code {})",
                            fileOnly,
                            static_cast<int>(reinterpret_cast<INT_PTR>(r)));
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\n%s",
                        I18n::T("editor.panel.console.open_lua").c_str(),
                        luaPath.c_str());
                }
            }
#endif
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
