#include "editor/panels/quest/QuestPropertyEditorPanel.h"

#include "core/Log.h"
#include "editor/panels/quest/QuestBrowserPanel.h"
#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"

#include <imgui.h>

#include <cstring>
#include <vector>

namespace Mood {

namespace {

/// @brief Helper: char buf <-> std::string round-trip para InputText.
///        Patron usado en todas las secciones para que la edicion solo
///        marque dirty si realmente cambia.
bool inputString(const char* label, std::string& s, int bufsize = 256) {
    std::vector<char> buf(bufsize);
    std::strncpy(buf.data(), s.c_str(), buf.size() - 1);
    buf[buf.size() - 1] = '\0';
    if (ImGui::InputText(label, buf.data(), buf.size())) {
        s = buf.data();
        return true;
    }
    return false;
}

bool inputStringMultiline(const char* label, std::string& s,
                           const ImVec2& size, int bufsize = 1024) {
    std::vector<char> buf(bufsize);
    std::strncpy(buf.data(), s.c_str(), buf.size() - 1);
    buf[buf.size() - 1] = '\0';
    if (ImGui::InputTextMultiline(label, buf.data(), buf.size(), size)) {
        s = buf.data();
        return true;
    }
    return false;
}

} // namespace

void QuestPropertyEditorPanel::loadFromPath(const std::filesystem::path& fsPath) {
    auto loaded = Quest::Asset::loadFromFile(fsPath);
    if (!loaded.has_value()) {
        Log::editor()->warn("[QuestPropertyEditor] no se pudo cargar '{}'",
                              fsPath.generic_string());
        m_loaded = Quest::Asset{};
        m_loadedPath.clear();
        return;
    }
    m_loaded = std::move(*loaded);
    m_loadedPath = fsPath;
    m_dirty = false;
    m_useNameKey = !m_loaded.name_key.empty();
    m_useDescKey = !m_loaded.description_key.empty();
}

void QuestPropertyEditorPanel::syncWithBrowserSelection() {
    if (!m_ui) return;
    const auto& sel = m_ui->questBrowser().selectedPath();
    if (sel == m_loadedPath) return;
    if (sel.empty()) {
        m_loadedPath.clear();
        m_loaded = Quest::Asset{};
        m_dirty = false;
        return;
    }
    loadFromPath(sel);
}

void QuestPropertyEditorPanel::onImGuiRender() {
    if (!visible) return;
    syncWithBrowserSelection();

    const std::string title = m_dirty
        ? (std::string("* ") + name())
        : std::string(name());

    if (!ImGui::Begin(title.c_str(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_loadedPath.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.quest_editor.no_selection").c_str());
        ImGui::End();
        return;
    }

    ImGui::Text("%s",
        I18n::T("editor.panel.quest_editor.file").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_loadedPath.filename().string().c_str());
    ImGui::Separator();

    drawIdentitySection();
    ImGui::Separator();
    drawObjectivesSection();
    ImGui::Separator();
    drawRewardsSection();
    ImGui::Separator();
    drawSaveBar();

    ImGui::End();
}

// =============================================================
// Identity
// =============================================================

void QuestPropertyEditorPanel::drawIdentitySection() {
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.quest_editor.section.identity").c_str());

    ImGui::Text("id");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_loaded.id.c_str());

    ImGui::Checkbox(I18n::T("editor.panel.quest_editor.name_use_key").c_str(),
                     &m_useNameKey);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.quest_editor.toggle_key_tooltip").c_str());
    }
    if (m_useNameKey) {
        if (inputString("name_key", m_loaded.name_key)) m_dirty = true;
    } else {
        if (inputString("name_literal", m_loaded.name_literal)) m_dirty = true;
    }

    ImGui::Checkbox(I18n::T("editor.panel.quest_editor.desc_use_key").c_str(),
                     &m_useDescKey);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.quest_editor.toggle_key_tooltip").c_str());
    }
    if (m_useDescKey) {
        if (inputString("description_key", m_loaded.description_key)) m_dirty = true;
    } else {
        if (inputStringMultiline("description_literal",
                                   m_loaded.description_literal,
                                   ImVec2(0, 60))) m_dirty = true;
    }

    if (inputString("category", m_loaded.category, 64)) m_dirty = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.quest_editor.category_tooltip").c_str());
    }
}

// =============================================================
// Objectives
// =============================================================

bool QuestPropertyEditorPanel::drawObjective(size_t idx, Quest::Objective& o) {
    using Quest::ObjectiveType;
    ImGui::PushID(static_cast<int>(idx));

    // Header collapsable: muestra el id + type para que el dev identifique
    // rapido en una lista larga.
    const char* typeStr = Quest::objectiveTypeToString(o.type);
    const std::string header = (o.id.empty() ? "(sin id)" : o.id) +
                                std::string(" [") + typeStr + "]";
    bool deleteMe = false;
    if (ImGui::CollapsingHeader(header.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        if (inputString("id", o.id, 64)) m_dirty = true;
        if (inputString("name_literal", o.name_literal)) m_dirty = true;
        if (inputStringMultiline("description_literal",
                                   o.description_literal,
                                   ImVec2(0, 40))) m_dirty = true;

        // Type dropdown
        const char* typeLabels[4] = {
            "collect", "talk", "reach", "custom_lua"
        };
        int typeIdx = static_cast<int>(o.type);
        if (ImGui::Combo("type", &typeIdx, typeLabels, 4)) {
            o.type = static_cast<ObjectiveType>(typeIdx);
            m_dirty = true;
        }

        // Campos type-specific. Solo mostramos los que aplican al type
        // actual — los demas valores quedan preservados (no se borran)
        // por si el dev cambia de mente.
        switch (o.type) {
            case ObjectiveType::Collect:
                if (inputString("item_path", o.item_path)) m_dirty = true;
                if (ImGui::InputInt("min_quantity", &o.min_quantity)) {
                    if (o.min_quantity < 1) o.min_quantity = 1;
                    m_dirty = true;
                }
                break;
            case ObjectiveType::Talk:
            case ObjectiveType::Reach:
                if (inputString("var_name", o.var_name, 128)) m_dirty = true;
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.quest_editor.var_hint").c_str());
                break;
            case ObjectiveType::CustomLua:
                if (inputStringMultiline("custom_predicate",
                                           o.custom_predicate,
                                           ImVec2(0, 50))) m_dirty = true;
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.quest_editor.custom_hint").c_str());
                break;
        }

        if (ImGui::Button(I18n::T("editor.panel.quest_editor.remove").c_str())) {
            deleteMe = true;
        }
    }
    ImGui::PopID();
    return deleteMe;
}

void QuestPropertyEditorPanel::drawObjectivesSection() {
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.quest_editor.section.objectives").c_str());

    size_t toRemove = static_cast<size_t>(-1);
    for (size_t i = 0; i < m_loaded.objectives.size(); ++i) {
        if (drawObjective(i, m_loaded.objectives[i])) {
            toRemove = i;
        }
    }
    if (toRemove != static_cast<size_t>(-1)) {
        m_loaded.objectives.erase(m_loaded.objectives.begin() + toRemove);
        m_dirty = true;
    }

    if (ImGui::Button(I18n::T("editor.panel.quest_editor.add_objective").c_str())) {
        Quest::Objective o;
        o.id = "objective_" + std::to_string(m_loaded.objectives.size() + 1);
        o.name_literal = o.id;
        o.type = Quest::ObjectiveType::CustomLua;
        o.custom_predicate = "false";
        m_loaded.objectives.push_back(std::move(o));
        m_dirty = true;
    }
}

// =============================================================
// Rewards
// =============================================================

bool QuestPropertyEditorPanel::drawReward(size_t idx, Quest::Reward& r) {
    using Quest::RewardType;
    ImGui::PushID(static_cast<int>(idx + 10000));  // offset para evitar colision con objectives

    const char* typeStr = Quest::rewardTypeToString(r.type);
    const std::string header = std::string("Reward #") + std::to_string(idx + 1) +
                                " [" + typeStr + "]";
    bool deleteMe = false;
    if (ImGui::CollapsingHeader(header.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* typeLabels[3] = { "item", "var", "lua" };
        int typeIdx = static_cast<int>(r.type);
        if (ImGui::Combo("type", &typeIdx, typeLabels, 3)) {
            r.type = static_cast<RewardType>(typeIdx);
            m_dirty = true;
        }

        switch (r.type) {
            case RewardType::Item:
                if (inputString("item_path", r.item_path)) m_dirty = true;
                if (ImGui::InputInt("quantity", &r.quantity)) {
                    if (r.quantity < 1) r.quantity = 1;
                    m_dirty = true;
                }
                break;
            case RewardType::Var:
                if (inputString("var_name", r.var_name, 128)) m_dirty = true;
                if (inputString("var_value", r.var_value, 256)) m_dirty = true;
                break;
            case RewardType::Lua:
                if (inputStringMultiline("lua_script", r.lua_script,
                                           ImVec2(0, 60))) m_dirty = true;
                break;
        }

        if (ImGui::Button(I18n::T("editor.panel.quest_editor.remove").c_str())) {
            deleteMe = true;
        }
    }
    ImGui::PopID();
    return deleteMe;
}

void QuestPropertyEditorPanel::drawRewardsSection() {
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.quest_editor.section.rewards").c_str());

    size_t toRemove = static_cast<size_t>(-1);
    for (size_t i = 0; i < m_loaded.rewards.size(); ++i) {
        if (drawReward(i, m_loaded.rewards[i])) {
            toRemove = i;
        }
    }
    if (toRemove != static_cast<size_t>(-1)) {
        m_loaded.rewards.erase(m_loaded.rewards.begin() + toRemove);
        m_dirty = true;
    }

    if (ImGui::Button(I18n::T("editor.panel.quest_editor.add_reward").c_str())) {
        Quest::Reward r;
        r.type = Quest::RewardType::Item;
        m_loaded.rewards.push_back(std::move(r));
        m_dirty = true;
    }
}

// =============================================================
// Save bar
// =============================================================

void QuestPropertyEditorPanel::drawSaveBar() {
    ImGui::BeginDisabled(!m_dirty);
    if (ImGui::Button(I18n::T("editor.panel.quest_editor.save").c_str())) {
        if (m_loaded.saveToFile(m_loadedPath)) {
            Log::editor()->info("[QuestPropertyEditor] guardado '{}'",
                                  m_loadedPath.generic_string());
            m_dirty = false;
            if (m_ui) m_ui->questBrowser().refresh();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_dirty);
    if (ImGui::Button(I18n::T("editor.panel.quest_editor.revert").c_str())) {
        loadFromPath(m_loadedPath);
    }
    ImGui::EndDisabled();
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s",
            I18n::T("editor.panel.quest_editor.unsaved").c_str());
    }
}

} // namespace Mood
