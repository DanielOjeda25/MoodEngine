#include "editor/panels/inventory/ItemBrowserPanel.h"

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"
#include "engine/inventory/ItemAsset.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <system_error>

namespace Mood {

namespace {
constexpr const char* k_itemsDir   = "assets/items";
constexpr const char* k_noCategory = "(sin categoría)";
// Tamano de cada card del grid (px). Ajustable; v1 chico/legible.
constexpr float       k_cardWidth  = 140.0f;
constexpr float       k_cardHeight = 64.0f;

/// @brief F2H51 fix #4: plantillas de item con tags + stats pre-armados.
///        Las plantillas viven SOLO en el editor — el motor no conoce
///        las categorias (engine-grade). El dev del juego puede usar
///        una como punto de partida y customizar libremente.
enum class NewItemTemplate { Empty=0, Weapon, Potion, Armor, Quest, Misc };

void applyTemplate(Inventory::Asset& a, NewItemTemplate t) {
    switch (t) {
        case NewItemTemplate::Weapon:
            a.tags  = {"weapon"};
            a.stats = {{"damage", 0.0f}, {"weight", 0.0f}, {"durability_max", 100.0f}};
            a.stack = {false, 1};
            break;
        case NewItemTemplate::Potion:
            a.tags  = {"consumable"};
            a.stats = {{"heal_amount", 0.0f}, {"weight", 0.0f}};
            a.stack = {true, 10};
            break;
        case NewItemTemplate::Armor:
            a.tags  = {"armor"};
            a.stats = {{"defense", 0.0f}, {"weight", 0.0f}, {"durability_max", 100.0f}};
            a.stack = {false, 1};
            break;
        case NewItemTemplate::Quest:
            a.tags  = {"quest_item"};
            a.stats = {};
            a.stack = {false, 1};
            break;
        case NewItemTemplate::Misc:
            a.tags  = {"misc"};
            a.stats = {{"weight", 0.0f}};
            a.stack = {true, 99};
            break;
        case NewItemTemplate::Empty:
        default:
            // sin modificar (queda con los defaults del ctor del Asset)
            break;
    }
}
} // namespace

std::optional<std::filesystem::path> ItemBrowserPanel::itemsDir() const {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / k_itemsDir;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}

void ItemBrowserPanel::refresh() {
    loadEntries();
    m_refreshed = true;
}

void ItemBrowserPanel::loadEntries() {
    namespace fs = std::filesystem;
    m_entries.clear();
    const auto dir = itemsDir();
    if (!dir.has_value()) return;
    std::error_code ec;
    fs::directory_iterator it(*dir, ec);
    if (ec) return;
    for (const auto& dirent : it) {
        if (!dirent.is_regular_file()) continue;
        if (dirent.path().extension() != Inventory::k_fileExtension) continue;
        auto asset = Inventory::Asset::loadFromFile(dirent.path());
        if (!asset.has_value()) continue;  // parse error ya logueado
        Entry e;
        e.path = dirent.path();
        e.displayName = !asset->name_literal.empty()
            ? asset->name_literal
            : asset->id;
        e.category = asset->tags.empty() ? k_noCategory : asset->tags.front();
        e.tags = std::move(asset->tags);
        m_entries.push_back(std::move(e));
    }
    std::sort(m_entries.begin(), m_entries.end(),
                [](const Entry& a, const Entry& b) {
                    return a.path.filename() < b.path.filename();
                });
}

bool ItemBrowserPanel::entryMatchesFilter(const Entry& e) const {
    // Search match: substring case-insensitive sobre displayName.
    if (m_searchBuf[0] != '\0') {
        std::string lo = e.displayName;
        std::transform(lo.begin(), lo.end(), lo.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        std::string needle = m_searchBuf;
        std::transform(needle.begin(), needle.end(), needle.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (lo.find(needle) == std::string::npos) return false;
    }
    // Tag filter: la entry debe tener ese tag.
    if (!m_tagFilter.empty()) {
        if (std::find(e.tags.begin(), e.tags.end(), m_tagFilter) == e.tags.end()) {
            return false;
        }
    }
    return true;
}

void ItemBrowserPanel::onImGuiRender() {
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
    drawNewItemPopup();
    drawRenamePopup();

    ImGui::End();
}

void ItemBrowserPanel::drawToolbar() {
    if (ImGui::Button(I18n::T("editor.panel.item_browser.new").c_str())) {
        m_newPopupOpen = true;
        m_newId.clear();
        m_newTemplateIdx = 0;  // default "Vacío"
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.item_browser.refresh").c_str())) {
        refresh();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu", m_entries.size());
}

void ItemBrowserPanel::drawFilters() {
    ImGui::PushItemWidth(180.0f);
    ImGui::InputTextWithHint("##search",
        I18n::T("editor.panel.item_browser.search_hint").c_str(),
        m_searchBuf, sizeof(m_searchBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Recolectar tags unicos de TODAS las entries para el combo.
    std::set<std::string> allTags;
    for (const Entry& e : m_entries) {
        for (const std::string& t : e.tags) allTags.insert(t);
    }
    ImGui::PushItemWidth(160.0f);
    const std::string allTagsLabel = I18n::T("editor.panel.item_browser.tag_any");
    const char* preview = m_tagFilter.empty() ? allTagsLabel.c_str() : m_tagFilter.c_str();
    if (ImGui::BeginCombo("##tagfilter", preview)) {
        if (ImGui::Selectable(allTagsLabel.c_str(), m_tagFilter.empty())) {
            m_tagFilter.clear();
        }
        for (const std::string& t : allTags) {
            const bool sel = (t == m_tagFilter);
            if (ImGui::Selectable(t.c_str(), sel)) {
                m_tagFilter = t;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
}

void ItemBrowserPanel::drawGrid() {
    namespace fs = std::filesystem;

    // Cuantas cards por fila? Float a int floor con piso 1.
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
        // Texto compacto: nombre arriba, categoria abajo.
        ImGui::TextWrapped("%s", e.displayName.c_str());
        ImGui::TextDisabled("%s", e.category.c_str());
        // Selectable transparente cubriendo toda la card.
        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton("##sel",
                ImVec2(k_cardWidth - 4.0f, k_cardHeight - 4.0f))) {
            m_selectedPath = e.path;
        }
        // F2H51 H: drag source emite payload con el path logico relativo
        // a `assets/` (que es lo que loadItem espera). El InspectorPanel
        // section de InventoryComponent acepta este payload.
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            namespace fs = std::filesystem;
            const auto rel = fs::relative(e.path, fs::current_path() / "assets");
            const std::string payload = rel.generic_string();
            ImGui::SetDragDropPayload("MOOD_ITEM_ASSET", payload.c_str(),
                                        payload.size() + 1);
            ImGui::TextUnformatted(e.displayName.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem(I18n::T("editor.panel.item_browser.ctx.rename").c_str())) {
                openRenameFor(e);
            }
            if (ImGui::MenuItem(I18n::T("editor.panel.item_browser.ctx.duplicate").c_str())) {
                duplicateEntry(e);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(I18n::T("editor.panel.item_browser.ctx.delete").c_str())) {
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
            I18n::T("editor.panel.item_browser.empty").c_str());
    }
}

void ItemBrowserPanel::drawNewItemPopup() {
    if (!m_newPopupOpen) return;
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(I18n::T("editor.panel.item_browser.new_title").c_str(),
                       &m_newPopupOpen,
                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(I18n::T("editor.panel.item_browser.new_prompt").c_str());
    char buf[128];
    std::strncpy(buf, m_newId.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##new_id", buf, sizeof(buf))) {
        m_newId = buf;
    }
    // F2H52 D fix: feedback explicito si el ID no es valido. Antes el
    // boton "Crear" se quedaba gris sin razon visible — el dev pensaba
    // que el editor no respondia. Ahora le decimos exactamente que
    // arreglar. Permitimos espacios (validos en filesystems modernos);
    // solo bloqueamos los chars que de verdad rompen el filename.
    const bool idEmpty = m_newId.empty();
    const auto badPos = m_newId.find_first_of("/\\:*?\"<>|");
    if (idEmpty) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s",
            I18n::T("editor.panel.item_browser.new_error.empty").c_str());
    } else if (badPos != std::string::npos) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s '%c'",
            I18n::T("editor.panel.item_browser.new_error.bad_char").c_str(),
            m_newId[badPos]);
    }
    // F2H51 fix #4: dropdown de plantilla. El dev elige una categoria
    // pre-armada (Arma / Pocion / Armadura / Quest / Objeto) que poblara
    // tags + stats + stack rules con valores iniciales razonables.
    const std::string templateLabels[6] = {
        I18n::T("editor.panel.item_browser.template.empty"),
        I18n::T("editor.panel.item_browser.template.weapon"),
        I18n::T("editor.panel.item_browser.template.potion"),
        I18n::T("editor.panel.item_browser.template.armor"),
        I18n::T("editor.panel.item_browser.template.quest"),
        I18n::T("editor.panel.item_browser.template.misc"),
    };
    ImGui::TextUnformatted(I18n::T("editor.panel.item_browser.template_label").c_str());
    if (ImGui::BeginCombo("##template", templateLabels[m_newTemplateIdx].c_str())) {
        for (int i = 0; i < 6; ++i) {
            const bool sel = (i == m_newTemplateIdx);
            if (ImGui::Selectable(templateLabels[i].c_str(), sel)) {
                m_newTemplateIdx = i;
            }
        }
        ImGui::EndCombo();
    }
    const bool idOk = !idEmpty && badPos == std::string::npos;
    ImGui::BeginDisabled(!idOk);
    if (ImGui::Button(I18n::T("editor.panel.item_browser.create").c_str())) {
        const auto dir = itemsDir();
        if (dir.has_value()) {
            const auto path = *dir / (m_newId + std::string(Inventory::k_fileExtension));
            Inventory::Asset a;
            a.id = m_newId;
            a.name_literal = m_newId;  // default sensato: id como nombre visible
            // Placeholder icon path — no enforcement de existencia, solo
            // sugerencia para que el dev sepa donde dropear su icono.
            a.icon_path = "icons/items/placeholder.png";
            // Aplicar plantilla seleccionada.
            applyTemplate(a, static_cast<NewItemTemplate>(m_newTemplateIdx));
            if (a.saveToFile(path)) {
                Log::editor()->info("[ItemBrowser] creado '{}'",
                                     path.generic_string());
                refresh();
                m_selectedPath = path;
                m_newPopupOpen = false;
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.item_browser.cancel").c_str())) {
        m_newPopupOpen = false;
    }
    ImGui::End();
}

void ItemBrowserPanel::drawRenamePopup() {
    if (m_renameTarget.empty()) return;
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::Begin(I18n::T("editor.panel.item_browser.rename_title").c_str(),
                       &open,
                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) m_renameTarget.clear();
        return;
    }
    ImGui::Text("%s: %s",
        I18n::T("editor.panel.item_browser.rename_prompt").c_str(),
        m_renameTarget.filename().string().c_str());
    char buf[128];
    std::strncpy(buf, m_renameNewId.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##rename_id", buf, sizeof(buf))) {
        m_renameNewId = buf;
    }
    // F2H52 D fix: paridad con drawNewItemPopup — mensaje rojo si el
    // ID es invalido, espacios permitidos.
    const bool rEmpty = m_renameNewId.empty();
    const auto rBadPos = m_renameNewId.find_first_of("/\\:*?\"<>|");
    if (rEmpty) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s",
            I18n::T("editor.panel.item_browser.new_error.empty").c_str());
    } else if (rBadPos != std::string::npos) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "%s '%c'",
            I18n::T("editor.panel.item_browser.new_error.bad_char").c_str(),
            m_renameNewId[rBadPos]);
    }
    const bool idOk = !rEmpty && rBadPos == std::string::npos;
    ImGui::BeginDisabled(!idOk);
    if (ImGui::Button(I18n::T("editor.panel.item_browser.rename_confirm").c_str())) {
        namespace fs = std::filesystem;
        const auto dir = itemsDir();
        if (dir.has_value()) {
            const auto dst = *dir / (m_renameNewId + std::string(Inventory::k_fileExtension));
            std::error_code ec;
            // Cargar, mutar id, guardar al nuevo path, borrar el viejo.
            auto loaded = Inventory::Asset::loadFromFile(m_renameTarget);
            if (loaded.has_value()) {
                loaded->id = m_renameNewId;
                if (loaded->saveToFile(dst)) {
                    fs::remove(m_renameTarget, ec);
                    Log::editor()->info("[ItemBrowser] renombrado '{}' -> '{}'",
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
    if (ImGui::Button(I18n::T("editor.panel.item_browser.cancel").c_str())) {
        m_renameTarget.clear();
    }
    ImGui::End();
    if (!open) m_renameTarget.clear();
}

void ItemBrowserPanel::openRenameFor(const Entry& e) {
    m_renameTarget = e.path;
    m_renameNewId = e.path.stem().string();
}

void ItemBrowserPanel::duplicateEntry(const Entry& e) {
    namespace fs = std::filesystem;
    const auto dir = itemsDir();
    if (!dir.has_value()) return;
    // Generar id unico: <orig>_copy, _copy2, _copy3...
    const std::string baseId = e.path.stem().string();
    std::string newId = baseId + "_copy";
    int n = 2;
    while (fs::exists(*dir / (newId + std::string(Inventory::k_fileExtension)))) {
        newId = baseId + "_copy" + std::to_string(n++);
    }
    auto loaded = Inventory::Asset::loadFromFile(e.path);
    if (!loaded.has_value()) return;
    loaded->id = newId;
    const auto dst = *dir / (newId + std::string(Inventory::k_fileExtension));
    if (loaded->saveToFile(dst)) {
        Log::editor()->info("[ItemBrowser] duplicado '{}' -> '{}'",
                             e.path.generic_string(), dst.generic_string());
        refresh();
        m_selectedPath = dst;
    }
}

void ItemBrowserPanel::deleteEntry(const Entry& e) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove(e.path, ec);
    if (ec) {
        Log::editor()->warn("[ItemBrowser] no se pudo borrar '{}': {}",
                             e.path.generic_string(), ec.message());
        return;
    }
    Log::editor()->info("[ItemBrowser] borrado '{}'", e.path.generic_string());
    if (m_selectedPath == e.path) m_selectedPath.clear();
    refresh();
}

} // namespace Mood
