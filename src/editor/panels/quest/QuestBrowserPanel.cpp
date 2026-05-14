#include "editor/panels/quest/QuestBrowserPanel.h"

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"
#include "engine/quest/QuestAsset.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <system_error>

namespace Mood {

namespace {
constexpr const char* k_questsDir   = "assets/quests";
constexpr const char* k_noCategory  = "(sin categoría)";
constexpr float       k_cardWidth   = 180.0f;
constexpr float       k_cardHeight  = 72.0f;

/// @brief Plantillas iniciales de quest. Viven SOLO en el editor — el motor
///        no conoce "main" vs "side" como categorias hardcoded; las plantillas
///        son sugerencias para que el dev arranque rapido y customice libremente.
enum class NewQuestTemplate { Empty=0, Fetch, Talk, Reach, Custom };

void applyTemplate(Quest::Asset& a, NewQuestTemplate t) {
    using Quest::Objective;
    using Quest::ObjectiveType;
    switch (t) {
        case NewQuestTemplate::Fetch: {
            a.category = "side";
            Objective o;
            o.id = "collect_target";
            o.name_literal = "Conseguí el objeto";
            o.type = ObjectiveType::Collect;
            o.item_path = "items/example.mooditem";
            o.min_quantity = 1;
            a.objectives = {o};
            break;
        }
        case NewQuestTemplate::Talk: {
            a.category = "side";
            Objective o;
            o.id = "talk_npc";
            o.name_literal = "Hablá con el NPC";
            o.type = ObjectiveType::Talk;
            o.var_name = "spoke_to_target";
            a.objectives = {o};
            break;
        }
        case NewQuestTemplate::Reach: {
            a.category = "side";
            Objective o;
            o.id = "reach_zone";
            o.name_literal = "Llegá a la zona";
            o.type = ObjectiveType::Reach;
            o.var_name = "entered_zone";
            a.objectives = {o};
            break;
        }
        case NewQuestTemplate::Custom: {
            a.category = "side";
            Objective o;
            o.id = "custom";
            o.name_literal = "Objective custom";
            o.type = ObjectiveType::CustomLua;
            o.custom_predicate = "false";  // dev edita
            a.objectives = {o};
            break;
        }
        case NewQuestTemplate::Empty:
        default:
            break;
    }
}
} // namespace

std::optional<std::filesystem::path> QuestBrowserPanel::questsDir() const {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / k_questsDir;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}

void QuestBrowserPanel::refresh() {
    loadEntries();
    m_refreshed = true;
}

void QuestBrowserPanel::loadEntries() {
    namespace fs = std::filesystem;
    m_entries.clear();
    const auto dir = questsDir();
    if (!dir.has_value()) return;
    std::error_code ec;
    fs::directory_iterator it(*dir, ec);
    if (ec) return;
    for (const auto& dirent : it) {
        if (!dirent.is_regular_file()) continue;
        if (dirent.path().extension() != Quest::k_fileExtension) continue;
        auto asset = Quest::Asset::loadFromFile(dirent.path());
        if (!asset.has_value()) continue;
        Entry e;
        e.path = dirent.path();
        e.displayName = !asset->name_literal.empty()
            ? asset->name_literal
            : asset->id;
        e.category = asset->category.empty() ? k_noCategory : asset->category;
        e.objectiveCount = asset->objectives.size();
        m_entries.push_back(std::move(e));
    }
    std::sort(m_entries.begin(), m_entries.end(),
                [](const Entry& a, const Entry& b) {
                    return a.path.filename() < b.path.filename();
                });
}

bool QuestBrowserPanel::entryMatchesFilter(const Entry& e) const {
    if (m_searchBuf[0] != '\0') {
        std::string lo = e.displayName;
        std::transform(lo.begin(), lo.end(), lo.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        std::string needle = m_searchBuf;
        std::transform(needle.begin(), needle.end(), needle.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (lo.find(needle) == std::string::npos) return false;
    }
    if (!m_categoryFilter.empty() && e.category != m_categoryFilter) {
        return false;
    }
    return true;
}

void QuestBrowserPanel::onImGuiRender() {
    if (!visible) return;
    if (!m_refreshed) refresh();

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    drawToolbar();
    drawFilters();
    ImGui::Separator();
    drawGrid();
    drawNewQuestPopup();
    drawRenamePopup();

    ImGui::End();
}

void QuestBrowserPanel::drawToolbar() {
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.new").c_str())) {
        m_newPopupOpen = true;
        m_newId.clear();
        m_newTemplateIdx = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.refresh").c_str())) {
        refresh();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu", m_entries.size());
}

void QuestBrowserPanel::drawFilters() {
    ImGui::PushItemWidth(180.0f);
    ImGui::InputTextWithHint("##search",
        I18n::T("editor.panel.quest_browser.search_hint").c_str(),
        m_searchBuf, sizeof(m_searchBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();

    std::set<std::string> allCats;
    for (const Entry& e : m_entries) allCats.insert(e.category);
    ImGui::PushItemWidth(160.0f);
    const std::string anyLabel = I18n::T("editor.panel.quest_browser.category_any");
    const char* preview = m_categoryFilter.empty() ? anyLabel.c_str()
                                                   : m_categoryFilter.c_str();
    if (ImGui::BeginCombo("##catfilter", preview)) {
        if (ImGui::Selectable(anyLabel.c_str(), m_categoryFilter.empty())) {
            m_categoryFilter.clear();
        }
        for (const std::string& c : allCats) {
            const bool sel = (c == m_categoryFilter);
            if (ImGui::Selectable(c.c_str(), sel)) {
                m_categoryFilter = c;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
}

void QuestBrowserPanel::drawGrid() {
    const float avail = ImGui::GetContentRegionAvail().x;
    int perRow = static_cast<int>(avail / (k_cardWidth + 8.0f));
    if (perRow < 1) perRow = 1;

    bool emptyVisible = true;
    int col = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        if (!entryMatchesFilter(e)) continue;
        emptyVisible = false;

        ImGui::PushID(static_cast<int>(i));
        const bool selected = (e.path == m_selectedPath);
        if (selected) {
            const ImVec4 col4 = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, col4);
        }
        ImGui::BeginChild("##card", ImVec2(k_cardWidth, k_cardHeight), true);
        ImGui::TextWrapped("%s", e.displayName.c_str());
        ImGui::TextDisabled("%s  (%zu obj.)", e.category.c_str(), e.objectiveCount);
        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton("##sel",
                ImVec2(k_cardWidth - 4.0f, k_cardHeight - 4.0f))) {
            m_selectedPath = e.path;
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem(I18n::T("editor.panel.quest_browser.ctx.rename").c_str())) {
                openRenameFor(e);
            }
            if (ImGui::MenuItem(I18n::T("editor.panel.quest_browser.ctx.duplicate").c_str())) {
                duplicateEntry(e);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.panel.quest_browser.ctx.delete").c_str())) {
                deleteEntry(e);
            }
            ImGui::EndPopup();
        }
        ImGui::EndChild();
        if (selected) ImGui::PopStyleColor();

        ImGui::PopID();
        ++col;
        if (col < perRow) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }
    if (emptyVisible) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.quest_browser.empty").c_str());
    }
}

void QuestBrowserPanel::drawNewQuestPopup() {
    if (!m_newPopupOpen) return;
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(I18n::T("editor.panel.quest_browser.new_title").c_str(),
                       &m_newPopupOpen,
                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(I18n::T("editor.panel.quest_browser.new_prompt").c_str());
    char buf[128];
    std::strncpy(buf, m_newId.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##new_id", buf, sizeof(buf))) {
        m_newId = buf;
    }
    const bool idEmpty = m_newId.empty();
    const auto badPos = m_newId.find_first_of("/\\:*?\"<>|");
    if (idEmpty) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s",
            I18n::T("editor.panel.quest_browser.new_error.empty").c_str());
    } else if (badPos != std::string::npos) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s '%c'",
            I18n::T("editor.panel.quest_browser.new_error.bad_char").c_str(),
            m_newId[badPos]);
    }
    const std::string templateLabels[5] = {
        I18n::T("editor.panel.quest_browser.template.empty"),
        I18n::T("editor.panel.quest_browser.template.fetch"),
        I18n::T("editor.panel.quest_browser.template.talk"),
        I18n::T("editor.panel.quest_browser.template.reach"),
        I18n::T("editor.panel.quest_browser.template.custom"),
    };
    ImGui::TextUnformatted(I18n::T("editor.panel.quest_browser.template_label").c_str());
    if (ImGui::BeginCombo("##template", templateLabels[m_newTemplateIdx].c_str())) {
        for (int i = 0; i < 5; ++i) {
            const bool sel = (i == m_newTemplateIdx);
            if (ImGui::Selectable(templateLabels[i].c_str(), sel)) {
                m_newTemplateIdx = i;
            }
        }
        ImGui::EndCombo();
    }
    const bool idOk = !idEmpty && badPos == std::string::npos;
    ImGui::BeginDisabled(!idOk);
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.create").c_str())) {
        const auto dir = questsDir();
        if (dir.has_value()) {
            const auto path = *dir / (m_newId + std::string(Quest::k_fileExtension));
            Quest::Asset a;
            a.id = m_newId;
            a.name_literal = m_newId;
            applyTemplate(a, static_cast<NewQuestTemplate>(m_newTemplateIdx));
            if (a.saveToFile(path)) {
                Log::editor()->info("[QuestBrowser] creado '{}'",
                                     path.generic_string());
                refresh();
                m_selectedPath = path;
                m_newPopupOpen = false;
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.cancel").c_str())) {
        m_newPopupOpen = false;
    }
    ImGui::End();
}

void QuestBrowserPanel::drawRenamePopup() {
    if (m_renameTarget.empty()) return;
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::Begin(I18n::T("editor.panel.quest_browser.rename_title").c_str(),
                       &open,
                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) m_renameTarget.clear();
        return;
    }
    ImGui::Text("%s: %s",
        I18n::T("editor.panel.quest_browser.rename_prompt").c_str(),
        m_renameTarget.filename().string().c_str());
    char buf[128];
    std::strncpy(buf, m_renameNewId.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##rename_id", buf, sizeof(buf))) {
        m_renameNewId = buf;
    }
    const bool rEmpty = m_renameNewId.empty();
    const auto rBadPos = m_renameNewId.find_first_of("/\\:*?\"<>|");
    if (rEmpty) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s",
            I18n::T("editor.panel.quest_browser.new_error.empty").c_str());
    } else if (rBadPos != std::string::npos) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s '%c'",
            I18n::T("editor.panel.quest_browser.new_error.bad_char").c_str(),
            m_renameNewId[rBadPos]);
    }
    const bool idOk = !rEmpty && rBadPos == std::string::npos;
    ImGui::BeginDisabled(!idOk);
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.rename_confirm").c_str())) {
        namespace fs = std::filesystem;
        const auto dir = questsDir();
        if (dir.has_value()) {
            const auto dst = *dir / (m_renameNewId + std::string(Quest::k_fileExtension));
            std::error_code ec;
            auto loaded = Quest::Asset::loadFromFile(m_renameTarget);
            if (loaded.has_value()) {
                loaded->id = m_renameNewId;
                if (loaded->saveToFile(dst)) {
                    fs::remove(m_renameTarget, ec);
                    Log::editor()->info("[QuestBrowser] renombrado '{}' -> '{}'",
                                         m_renameTarget.generic_string(),
                                         dst.generic_string());
                    if (m_selectedPath == m_renameTarget) m_selectedPath = dst;
                    refresh();
                }
            }
        }
        m_renameTarget.clear();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.quest_browser.cancel").c_str())) {
        m_renameTarget.clear();
    }
    ImGui::End();
    if (!open) m_renameTarget.clear();
}

void QuestBrowserPanel::openRenameFor(const Entry& e) {
    m_renameTarget = e.path;
    m_renameNewId = e.path.stem().string();
}

void QuestBrowserPanel::duplicateEntry(const Entry& e) {
    namespace fs = std::filesystem;
    const auto dir = questsDir();
    if (!dir.has_value()) return;
    const std::string baseId = e.path.stem().string();
    std::string newId = baseId + "_copy";
    int n = 2;
    while (fs::exists(*dir / (newId + std::string(Quest::k_fileExtension)))) {
        newId = baseId + "_copy" + std::to_string(n++);
    }
    auto loaded = Quest::Asset::loadFromFile(e.path);
    if (!loaded.has_value()) return;
    loaded->id = newId;
    const auto dst = *dir / (newId + std::string(Quest::k_fileExtension));
    if (loaded->saveToFile(dst)) {
        Log::editor()->info("[QuestBrowser] duplicado '{}' -> '{}'",
                             e.path.generic_string(), dst.generic_string());
        refresh();
        m_selectedPath = dst;
    }
}

void QuestBrowserPanel::deleteEntry(const Entry& e) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove(e.path, ec);
    if (ec) {
        Log::editor()->warn("[QuestBrowser] no se pudo borrar '{}': {}",
                             e.path.generic_string(), ec.message());
        return;
    }
    Log::editor()->info("[QuestBrowser] borrado '{}'", e.path.generic_string());
    if (m_selectedPath == e.path) m_selectedPath.clear();
    refresh();
}

} // namespace Mood
