#include "editor/panels/narrative/DialogNodeInspectorPanel.h"

#include "editor/panels/narrative/DialogEditorPanel.h"
#include "editor/ui/EditorUI.h"
#include "engine/dialog/DialogAsset.h"
#include "core/i18n/I18n.h"

#include <imgui.h>

#include <cstring>

namespace Mood {

using namespace Mood::NodeGraph;

namespace {

// Helper: wrapper de ImGui::InputText sobre std::string.
bool inputTextString(const char* label, std::string& s, ImGuiInputTextFlags flags = 0) {
    char buf[512];
    std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(label, buf, sizeof(buf), flags)) {
        s = buf;
        return true;
    }
    return false;
}

// Wrapper para textarea multiline.
bool inputTextMultiline(const char* label, std::string& s, ImVec2 size) {
    char buf[2048];
    std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputTextMultiline(label, buf, sizeof(buf), size)) {
        s = buf;
        return true;
    }
    return false;
}

} // namespace

void DialogNodeInspectorPanel::onImGuiRender() {
    if (!visible) return;
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (!m_ui) {
        ImGui::TextDisabled("EditorUI no inyectado");
        ImGui::End();
        return;
    }
    DialogEditorPanel& editor = m_ui->dialogEditor();
    Dialog::Asset* asset = editor.asset();
    if (!asset) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.dialog_inspector.no_asset").c_str());
        ImGui::End();
        return;
    }
    const NodeId sel = editor.selectedNode();
    if (sel == k_invalidNodeId) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.dialog_inspector.no_selection").c_str());
        ImGui::End();
        return;
    }
    const Node* n = asset->graph().findNode(sel);
    if (!n) {
        ImGui::TextDisabled("Nodo inexistente: %u", sel);
        ImGui::End();
        return;
    }
    if (n->typeTag != Dialog::k_typeDialogLine) {
        ImGui::TextDisabled("Tipo de nodo no editable: %s", n->typeTag.c_str());
        ImGui::End();
        return;
    }

    Dialog::Line line = asset->parseLine(sel);
    bool changed = false;

    ImGui::Text("Node ID: %u", sel);
    if (asset->metadata().start_node_id == sel) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.13f, 1.0f), "%s",
                            I18n::T("editor.panel.dialog_inspector.is_start").c_str());
    }
    ImGui::Separator();

    // ----- Text (key vs literal toggle) -----
    ImGui::TextUnformatted(I18n::T("editor.panel.dialog_inspector.text_label").c_str());
    bool useKey = !line.text_key.empty();
    if (ImGui::Checkbox(I18n::T("editor.panel.dialog_inspector.use_i18n_key").c_str(),
                          &useKey)) {
        // Toggle: si pasa a key, copio el literal a key + limpio literal.
        // Si pasa a literal, limpio key.
        if (useKey) {
            line.text_key     = line.text_literal;  // promote literal a "key candidate"
            line.text_literal = "";
        } else {
            line.text_literal = line.text_key;
            line.text_key     = "";
        }
        changed = true;
    }
    if (useKey) {
        if (inputTextString("##text_key", line.text_key)) changed = true;
    } else {
        if (inputTextMultiline("##text_literal", line.text_literal,
                                  ImVec2(-1.0f, 60.0f))) changed = true;
    }

    ImGui::Spacing();

    // ----- Portrait / audio / animation -----
    if (inputTextString(I18n::T("editor.panel.dialog_inspector.portrait").c_str(),
                          line.portrait)) changed = true;
    if (inputTextString(I18n::T("editor.panel.dialog_inspector.audio").c_str(),
                          line.audio)) changed = true;
    if (inputTextString(I18n::T("editor.panel.dialog_inspector.animation").c_str(),
                          line.animation)) changed = true;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted(
        I18n::T("editor.panel.dialog_inspector.choices_header").c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("+##add_choice")) {
        line.choices.emplace_back();
        changed = true;
    }

    // ----- Choices array -----
    int toRemove = -1;
    for (size_t i = 0; i < line.choices.size(); ++i) {
        Dialog::Choice& c = line.choices[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::CollapsingHeader(
                (I18n::T("editor.panel.dialog_inspector.choice_label") +
                 " " + std::to_string(i)).c_str(),
                ImGuiTreeNodeFlags_DefaultOpen)) {
            // Toggle literal vs key (igual que el text).
            bool useKey2 = !c.label_key.empty();
            if (ImGui::Checkbox(I18n::T("editor.panel.dialog_inspector.use_i18n_key").c_str(),
                                  &useKey2)) {
                if (useKey2) {
                    c.label_key     = c.label_literal;
                    c.label_literal = "";
                } else {
                    c.label_literal = c.label_key;
                    c.label_key     = "";
                }
                changed = true;
            }
            if (useKey2) {
                if (inputTextString("##label_key", c.label_key)) changed = true;
            } else {
                if (inputTextString("##label_literal", c.label_literal)) changed = true;
            }
            if (inputTextString(I18n::T("editor.panel.dialog_inspector.condition_lua").c_str(),
                                  c.condition_lua)) changed = true;
            if (inputTextString(I18n::T("editor.panel.dialog_inspector.on_select_lua").c_str(),
                                  c.on_select_lua)) changed = true;
            if (ImGui::SmallButton(I18n::T("editor.panel.dialog_inspector.remove_choice").c_str())) {
                toRemove = static_cast<int>(i);
            }
        }
        ImGui::PopID();
    }
    if (toRemove >= 0) {
        line.choices.erase(line.choices.begin() + toRemove);
        changed = true;
    }

    if (changed) {
        asset->writeLine(sel, line);  // auto-sync sockets
        editor.notifyLineEdited();
    }

    ImGui::End();
}

} // namespace Mood
