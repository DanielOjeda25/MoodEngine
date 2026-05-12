// F2H51 Bloque H: render del `InventoryComponent` en el Inspector.
//
// Secciones:
//   1. Dropdown del layout mode (FlatList / Grid2D / EquipmentSlots).
//   2. Capacity por modo:
//      - FlatList: max_items spinner.
//      - Grid2D: spinners width x height.
//      - EquipmentSlots: lista editable de slot names + tag_filter.
//   3. Entries (lista de items): tabla con itemPath + quantity + slot_index + remove.
//   4. Drop target acepta payload "MOOD_ITEM_ASSET" del ItemBrowser
//      (path logico del item) -> state.add(id, 1, am).
//   5. Boton "Vaciar inventario" (clear).

#include "editor/panels/scene/InspectorPanel.h"

#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"
#include "engine/inventory/InventoryState.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <cstring>
#include <string>

namespace Mood {

namespace {

const char* k_modeLabels[] = { "FlatList", "Grid2D", "EquipmentSlots" };

int modeToInt(Inventory::LayoutMode m) {
    switch (m) {
        case Inventory::LayoutMode::FlatList:       return 0;
        case Inventory::LayoutMode::Grid2D:         return 1;
        case Inventory::LayoutMode::EquipmentSlots: return 2;
    }
    return 0;
}

Inventory::LayoutMode intToMode(int v) {
    if (v == 1) return Inventory::LayoutMode::Grid2D;
    if (v == 2) return Inventory::LayoutMode::EquipmentSlots;
    return Inventory::LayoutMode::FlatList;
}

} // namespace

void InspectorPanel::renderInventorySection(Entity e) {
    auto& inv = e.getComponent<InventoryComponent>();
    Inventory::State& st = inv.state;

    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.inventory.header").c_str(),
            ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    // -------- 1. Mode dropdown --------
    int modeIdx = modeToInt(st.mode);
    if (ImGui::Combo(I18n::T("editor.panel.inspector.inventory.mode").c_str(),
                      &modeIdx, k_modeLabels, IM_ARRAYSIZE(k_modeLabels))) {
        st.mode = intToMode(modeIdx);
        m_editedThisFrame = true;
    }

    // -------- 2. Capacity por modo --------
    ImGui::Indent();
    if (st.mode == Inventory::LayoutMode::FlatList) {
        int max_items = st.config.max_items;
        if (ImGui::InputInt("max_items", &max_items)) {
            if (max_items < 1) max_items = 1;
            if (max_items > 999) max_items = 999;
            st.config.max_items = max_items;
            m_editedThisFrame = true;
        }
    } else if (st.mode == Inventory::LayoutMode::Grid2D) {
        int w = st.config.grid_width;
        int h = st.config.grid_height;
        if (ImGui::InputInt("grid_width", &w)) {
            if (w < 1) w = 1;
            if (w > 32) w = 32;
            st.config.grid_width = w;
            m_editedThisFrame = true;
        }
        if (ImGui::InputInt("grid_height", &h)) {
            if (h < 1) h = 1;
            if (h > 32) h = 32;
            st.config.grid_height = h;
            m_editedThisFrame = true;
        }
    } else {
        // EquipmentSlots: lista editable.
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.inventory.equipment_slots").c_str());
        size_t toRemove = static_cast<size_t>(-1);
        for (size_t i = 0; i < st.config.equipment_slots.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            auto& slot = st.config.equipment_slots[i];
            char nameBuf[64];
            std::strncpy(nameBuf, slot.name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            ImGui::PushItemWidth(120.0f);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
                slot.name = nameBuf;
                m_editedThisFrame = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();

            char tagBuf[64];
            std::strncpy(tagBuf, slot.tag_filter.c_str(), sizeof(tagBuf) - 1);
            tagBuf[sizeof(tagBuf) - 1] = '\0';
            ImGui::PushItemWidth(120.0f);
            if (ImGui::InputTextWithHint("##tag",
                    I18n::T("editor.panel.inspector.inventory.tag_filter_hint").c_str(),
                    tagBuf, sizeof(tagBuf))) {
                slot.tag_filter = tagBuf;
                m_editedThisFrame = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) toRemove = i;
            ImGui::PopID();
        }
        if (toRemove != static_cast<size_t>(-1)) {
            st.config.equipment_slots.erase(st.config.equipment_slots.begin() + toRemove);
            m_editedThisFrame = true;
        }
        if (ImGui::Button(I18n::T("editor.panel.inspector.inventory.add_slot").c_str())) {
            st.config.equipment_slots.push_back({"", ""});
            m_editedThisFrame = true;
        }
    }
    ImGui::Unindent();

    ImGui::TextDisabled("Capacity: %d", st.slotCapacity());

    // -------- 3. Entries (items pre-cargados en el inventario) --------
    ImGui::Separator();
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.inventory.entries").c_str());

    if (m_assets != nullptr) {
        if (ImGui::BeginTable("##entries", 4, ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("item",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("qty",   ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("slot",  ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 24.0f);

            size_t toRemove = static_cast<size_t>(-1);
            for (size_t i = 0; i < st.entries.size(); ++i) {
                auto& en = st.entries[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(m_assets->itemPathOf(en.itemId).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                int qty = en.quantity;
                if (ImGui::InputInt("##q", &qty, 0, 0)) {
                    if (qty < 1) qty = 1;
                    en.quantity = qty;
                    m_editedThisFrame = true;
                }
                ImGui::PopItemWidth();
                ImGui::TableSetColumnIndex(2);
                if (st.mode == Inventory::LayoutMode::FlatList) {
                    ImGui::TextDisabled("-");
                } else {
                    ImGui::PushItemWidth(-1);
                    int slot = en.slot_index;
                    if (ImGui::InputInt("##s", &slot, 0, 0)) {
                        en.slot_index = slot;
                        m_editedThisFrame = true;
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("x")) toRemove = i;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (toRemove != static_cast<size_t>(-1)) {
                st.entries.erase(st.entries.begin() + toRemove);
                m_editedThisFrame = true;
            }
        }
    } else {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.inventory.no_am").c_str());
    }

    // -------- 4. Drop target para agregar items del browser --------
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Button(I18n::T("editor.panel.inspector.inventory.drop_here").c_str(),
                   ImVec2(-1, 28));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOOD_ITEM_ASSET")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path != nullptr && m_assets != nullptr) {
                const ItemAssetId id = m_assets->loadItem(path);
                if (id != m_assets->missingItemId()) {
                    st.add(id, 1, *m_assets);
                    m_editedThisFrame = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // -------- 5. Clear --------
    if (ImGui::Button(I18n::T("editor.panel.inspector.inventory.clear").c_str())) {
        st.clear();
        m_editedThisFrame = true;
    }

    ImGui::Separator();
}

} // namespace Mood
