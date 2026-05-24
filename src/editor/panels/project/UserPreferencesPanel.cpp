#include "editor/panels/project/UserPreferencesPanel.h"

#include "core/UserSettings.h"
#include "core/i18n/I18n.h"
#include "editor/ui/EditorThemes.h"

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

// Mismo layout estilo Unity que ProjectSettingsPanel — label izquierda con
// ancho fijo, control derecha.
constexpr float kLabelColumnWidth = 160.0f;
constexpr float kControlWidth     = 200.0f;

} // namespace

void UserPreferencesPanel::onImGuiRender() {
    if (!visible) return;

    // Ventana flotante centrada + tamano fijo, no dockeable, sin resize/
    // collapse — espejo de ProjectSettingsPanel (consistencia UX).
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540.0f, 360.0f), ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking;

    if (!ImGui::Begin(name(), &visible, kFlags)) {
        ImGui::End();
        return;
    }

    ImGui::Spacing();
    drawGeneralSection();

    ImGui::End();
}

void UserPreferencesPanel::drawGeneralSection() {
    ImGui::SeparatorText(I18n::T("editor.user_preferences.section.general").c_str());
    ImGui::Spacing();
    ImGui::Indent();

    // === Tema ===
    const auto& themes = EditorThemes::available();
    const std::string& curThemeId = UserSettings::theme();
    int curThemeIdx = 0;
    for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
        if (themes[i].id == curThemeId) { curThemeIdx = i; break; }
    }
    const std::string curThemeLabel = I18n::T(themes[curThemeIdx].i18nKey);

    ImGui::TextUnformatted(I18n::T("editor.user_preferences.theme").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::BeginCombo("##user_pref_theme", curThemeLabel.c_str())) {
        for (int i = 0; i < static_cast<int>(themes.size()); ++i) {
            const bool sel = (i == curThemeIdx);
            if (ImGui::Selectable(I18n::T(themes[i].i18nKey).c_str(), sel)) {
                UserSettings::setTheme(themes[i].id);
                EditorThemes::apply(themes[i].id);  // preview live
                UserSettings::save();
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // === Idioma ===
    const auto curLang = I18n::currentLanguage();
    const std::string curLangLabel = I18n::T(
        curLang == I18n::Language::English
            ? "editor.user_preferences.language.english"
            : "editor.user_preferences.language.spanish");

    ImGui::TextUnformatted(I18n::T("editor.user_preferences.language").c_str());
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::SetNextItemWidth(kControlWidth);
    if (ImGui::BeginCombo("##user_pref_lang", curLangLabel.c_str())) {
        const bool isEs = (curLang == I18n::Language::Spanish);
        if (ImGui::Selectable(
                I18n::T("editor.user_preferences.language.spanish").c_str(), isEs)) {
            if (I18n::setLanguage(I18n::Language::Spanish)) {
                UserSettings::setLanguage(I18n::Language::Spanish);
                UserSettings::save();
            }
        }
        if (isEs) ImGui::SetItemDefaultFocus();
        const bool isEn = (curLang == I18n::Language::English);
        if (ImGui::Selectable(
                I18n::T("editor.user_preferences.language.english").c_str(), isEn)) {
            if (I18n::setLanguage(I18n::Language::English)) {
                UserSettings::setLanguage(I18n::Language::English);
                UserSettings::save();
            }
        }
        if (isEn) ImGui::SetItemDefaultFocus();
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s",
        I18n::T("editor.user_preferences.live_apply_hint").c_str());

    ImGui::Unindent();
}

} // namespace Mood
