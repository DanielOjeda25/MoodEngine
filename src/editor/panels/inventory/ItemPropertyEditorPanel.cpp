#include "editor/panels/inventory/ItemPropertyEditorPanel.h"

#include "core/Log.h"
#include "editor/panels/inventory/ItemBrowserPanel.h"
#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace Mood {

void ItemPropertyEditorPanel::loadFromPath(const std::filesystem::path& fsPath) {
    auto loaded = Inventory::Asset::loadFromFile(fsPath);
    if (!loaded.has_value()) {
        Log::editor()->warn("[ItemPropertyEditor] no se pudo cargar '{}'",
                              fsPath.generic_string());
        m_loaded = Inventory::Asset{};
        m_loadedPath.clear();
        return;
    }
    m_loaded = std::move(*loaded);
    m_loadedPath = fsPath;
    m_dirty = false;
    // Toggle defaults: si name_key no esta vacio, asumir que el dev quiere editarlo;
    // si solo hay literal, mostrar literal.
    m_useNameKey = !m_loaded.name_key.empty();
    m_useDescKey = !m_loaded.description_key.empty();
}

void ItemPropertyEditorPanel::syncWithBrowserSelection() {
    if (!m_ui) return;
    const auto& sel = m_ui->itemBrowser().selectedPath();
    if (sel == m_loadedPath) return;
    if (sel.empty()) {
        m_loadedPath.clear();
        m_loaded = Inventory::Asset{};
        m_dirty = false;
        return;
    }
    loadFromPath(sel);
}

void ItemPropertyEditorPanel::onImGuiRender() {
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
            I18n::T("editor.panel.item_editor.no_selection").c_str());
        ImGui::End();
        return;
    }

    ImGui::Text("%s",
        I18n::T("editor.panel.item_editor.file").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_loadedPath.filename().string().c_str());
    ImGui::Separator();

    drawIdentitySection();
    ImGui::Separator();
    drawVisualSection();
    ImGui::Separator();
    drawTagsSection();
    ImGui::Separator();
    drawStatsSection();
    ImGui::Separator();
    drawStackSection();
    ImGui::Separator();
    drawSlotSizeSection();
    ImGui::Separator();
    drawSaveBar();

    ImGui::End();
}

// =============================================================
// Identity
// =============================================================

void ItemPropertyEditorPanel::drawIdentitySection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.identity").c_str());

    // id: readonly (el dev renombra desde el browser).
    ImGui::Text("id");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_loaded.id.c_str());

    // name: toggle key/literal. Tooltip explica el caso de uso (localizacion).
    ImGui::Checkbox(I18n::T("editor.panel.item_editor.name_use_key").c_str(), &m_useNameKey);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.item_editor.toggle_key_tooltip").c_str());
    }

    char buf[256];
    if (m_useNameKey) {
        std::strncpy(buf, m_loaded.name_key.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("name_key", buf, sizeof(buf))) {
            m_loaded.name_key = buf;
            m_dirty = true;
        }
    } else {
        std::strncpy(buf, m_loaded.name_literal.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("name_literal", buf, sizeof(buf))) {
            m_loaded.name_literal = buf;
            m_dirty = true;
        }
    }

    // description: toggle + multiline.
    ImGui::Checkbox(I18n::T("editor.panel.item_editor.desc_use_key").c_str(), &m_useDescKey);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.item_editor.toggle_key_tooltip").c_str());
    }
    char descBuf[1024];
    if (m_useDescKey) {
        std::strncpy(descBuf, m_loaded.description_key.c_str(), sizeof(descBuf) - 1);
        descBuf[sizeof(descBuf) - 1] = '\0';
        if (ImGui::InputText("description_key", descBuf, sizeof(descBuf))) {
            m_loaded.description_key = descBuf;
            m_dirty = true;
        }
    } else {
        std::strncpy(descBuf, m_loaded.description_literal.c_str(), sizeof(descBuf) - 1);
        descBuf[sizeof(descBuf) - 1] = '\0';
        if (ImGui::InputTextMultiline("description_literal", descBuf, sizeof(descBuf),
                                         ImVec2(0, 60))) {
            m_loaded.description_literal = descBuf;
            m_dirty = true;
        }
    }
}

// =============================================================
// Visual
// =============================================================

void ItemPropertyEditorPanel::drawVisualSection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.visual").c_str());

    char buf[256];
    std::strncpy(buf, m_loaded.icon_path.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("icon_path", buf, sizeof(buf))) {
        m_loaded.icon_path = buf;
        m_dirty = true;
    }

    std::strncpy(buf, m_loaded.model_path.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("model_path", buf, sizeof(buf))) {
        m_loaded.model_path = buf;
        m_dirty = true;
    }
}

// =============================================================
// Tags
// =============================================================

void ItemPropertyEditorPanel::drawTagsSection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.tags").c_str());

    // Chips en linea con X para borrar.
    for (size_t i = 0; i < m_loaded.tags.size(); ) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Button(m_loaded.tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            m_loaded.tags.erase(m_loaded.tags.begin() + i);
            m_dirty = true;
            ImGui::PopID();
            continue;
        }
        ImGui::SameLine();
        ImGui::PopID();
        ++i;
    }
    if (!m_loaded.tags.empty()) ImGui::NewLine();

    // Input para agregar.
    ImGui::PushItemWidth(160.0f);
    ImGui::InputTextWithHint("##new_tag",
        I18n::T("editor.panel.item_editor.tag_hint").c_str(),
        m_newTagBuf, sizeof(m_newTagBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    const bool tagOk = m_newTagBuf[0] != '\0';
    ImGui::BeginDisabled(!tagOk);
    if (ImGui::Button(I18n::T("editor.panel.item_editor.tag_add").c_str())) {
        // Evitar duplicados.
        const std::string newTag = m_newTagBuf;
        const bool dup = std::find(m_loaded.tags.begin(), m_loaded.tags.end(), newTag)
                          != m_loaded.tags.end();
        if (!dup) {
            m_loaded.tags.push_back(newTag);
            m_dirty = true;
        }
        m_newTagBuf[0] = '\0';
    }
    ImGui::EndDisabled();
}

// =============================================================
// Stats
// =============================================================

void ItemPropertyEditorPanel::drawStatsSection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.stats").c_str());

    // Tabla con keys + values (float) + boton X.
    if (ImGui::BeginTable("##stats", 3, ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 24.0f);

        // Iterar entries — map<string,float> es ordenado, ok para UI.
        std::string toRemove;
        for (auto& [k, v] : m_loaded.stats) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(k.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(k.c_str());
            ImGui::PushItemWidth(-1);
            float val = v;
            if (ImGui::InputFloat("##v", &val, 0.0f, 0.0f, "%g")) {
                v = val;
                m_dirty = true;
            }
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("x")) toRemove = k;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (!toRemove.empty()) {
            m_loaded.stats.erase(toRemove);
            m_dirty = true;
        }
    }

    // Add row.
    ImGui::PushItemWidth(160.0f);
    ImGui::InputTextWithHint("##new_stat_key",
        I18n::T("editor.panel.item_editor.stat_key_hint").c_str(),
        m_newStatKeyBuf, sizeof(m_newStatKeyBuf));
    ImGui::SameLine();
    ImGui::PushItemWidth(100.0f);
    ImGui::InputFloat("##new_stat_value", &m_newStatValue, 0.0f, 0.0f, "%g");
    ImGui::PopItemWidth();
    ImGui::PopItemWidth();
    ImGui::SameLine();
    const bool statOk = m_newStatKeyBuf[0] != '\0';
    ImGui::BeginDisabled(!statOk);
    if (ImGui::Button(I18n::T("editor.panel.item_editor.stat_add").c_str())) {
        m_loaded.stats[m_newStatKeyBuf] = m_newStatValue;
        m_newStatKeyBuf[0] = '\0';
        m_newStatValue = 0.0f;
        m_dirty = true;
    }
    ImGui::EndDisabled();
}

// =============================================================
// Stack
// =============================================================

void ItemPropertyEditorPanel::drawStackSection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.stack").c_str());

    bool stackable = m_loaded.stack.stackable;
    if (ImGui::Checkbox("stackable", &stackable)) {
        m_loaded.stack.stackable = stackable;
        if (!stackable) m_loaded.stack.max_stack = 1;
        m_dirty = true;
    }
    ImGui::BeginDisabled(!stackable);
    int maxStack = m_loaded.stack.max_stack;
    if (ImGui::InputInt("max_stack", &maxStack)) {
        if (maxStack < 1) maxStack = 1;
        if (maxStack > 999) maxStack = 999;
        m_loaded.stack.max_stack = maxStack;
        m_dirty = true;
    }
    ImGui::EndDisabled();
}

// =============================================================
// Slot size
// =============================================================

void ItemPropertyEditorPanel::drawSlotSizeSection() {
    ImGui::TextDisabled("%s", I18n::T("editor.panel.item_editor.section.slot_size").c_str());

    int w = m_loaded.slot_size.width;
    int h = m_loaded.slot_size.height;
    if (ImGui::InputInt("width", &w)) {
        if (w < 1) w = 1;
        if (w > 16) w = 16;
        m_loaded.slot_size.width = w;
        m_dirty = true;
    }
    if (ImGui::InputInt("height", &h)) {
        if (h < 1) h = 1;
        if (h > 16) h = 16;
        m_loaded.slot_size.height = h;
        m_dirty = true;
    }
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.item_editor.slot_size_hint").c_str());
}

// =============================================================
// Save bar
// =============================================================

void ItemPropertyEditorPanel::drawSaveBar() {
    ImGui::BeginDisabled(!m_dirty);
    if (ImGui::Button(I18n::T("editor.panel.item_editor.save").c_str())) {
        if (m_loaded.saveToFile(m_loadedPath)) {
            Log::editor()->info("[ItemPropertyEditor] guardado '{}'",
                                  m_loadedPath.generic_string());
            m_dirty = false;
            // Refrescar el browser para que displayName/categoria se
            // actualicen tras cambios al name_literal o tags.
            if (m_ui) m_ui->itemBrowser().refresh();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_dirty);
    if (ImGui::Button(I18n::T("editor.panel.item_editor.revert").c_str())) {
        loadFromPath(m_loadedPath);
    }
    ImGui::EndDisabled();
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s",
            I18n::T("editor.panel.item_editor.unsaved").c_str());
    }
}

} // namespace Mood
