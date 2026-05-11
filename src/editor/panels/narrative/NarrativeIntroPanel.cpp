#include "editor/panels/narrative/NarrativeIntroPanel.h"

#include "engine/i18n/I18n.h"

#include <imgui.h>

namespace Mood {

void NarrativeIntroPanel::onImGuiRender() {
    if (!visible) return;
    // F2H46: window ID estable = name() (stable English, matchea con
    // DockBuilderDockWindow). El contenido SI es i18n.
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    // Titulo grande.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.13f, 1.0f));  // amarillo
    ImGui::TextUnformatted(I18n::T("editor.panel.narrative_intro.heading").c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // Explicacion principal.
    ImGui::TextWrapped("%s", I18n::T("editor.panel.narrative_intro.body").c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Roadmap.
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.85f, 1.0f, 1.0f));  // cyan
    ImGui::TextUnformatted(I18n::T("editor.panel.narrative_intro.roadmap_header").c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::Bullet();
    ImGui::TextWrapped("%s", I18n::T("editor.panel.narrative_intro.item_dialog").c_str());
    ImGui::Spacing();
    ImGui::Bullet();
    ImGui::TextWrapped("%s", I18n::T("editor.panel.narrative_intro.item_quest").c_str());
    ImGui::Spacing();
    ImGui::Bullet();
    ImGui::TextWrapped("%s", I18n::T("editor.panel.narrative_intro.item_inventory").c_str());
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("%s", I18n::T("editor.panel.narrative_intro.footer").c_str());

    ImGui::End();
}

} // namespace Mood
