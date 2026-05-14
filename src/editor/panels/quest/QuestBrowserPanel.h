#pragma once

// QuestBrowserPanel (F2H53 Bloque D): lista todos los `.moodquest` del
// proyecto activo + filtros (search + category) + acciones (new / duplicate
// / rename / delete). Clon estructural de `ItemBrowserPanel` (F2H51) — la
// diferencia es que el filtro es por `category` (string libre del schema)
// en vez de tags.
//
// Path por convencion: `<cwd>/assets/quests/`. Mismo patron que
// `assets/items/` y `assets/dialogs/`.
//
// El Property Editor (mismo bloque) consume la seleccion via
// `selectedPath()`.

#include "editor/panels/IPanel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class EditorUI;

class QuestBrowserPanel : public IPanel {
public:
    QuestBrowserPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Quest Browser"; }
    const char* category() const override { return "Assets"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief Re-escanea `assets/quests/` y reparsea los `.moodquest` para
    ///        refrescar nombres + categoria. Llamar tras crear / borrar /
    ///        renombrar (o cambio de proyecto).
    void refresh();

    /// @brief Path del quest seleccionado. Vacio = sin seleccion.
    const std::filesystem::path& selectedPath() const { return m_selectedPath; }

private:
    struct Entry {
        std::filesystem::path path;
        std::string displayName;   // name_literal o id
        std::string category;      // category del schema o "(sin categoría)"
        size_t      objectiveCount = 0;
    };

    std::optional<std::filesystem::path> questsDir() const;

    void loadEntries();
    void drawToolbar();
    void drawFilters();
    void drawGrid();
    void drawNewQuestPopup();
    void drawRenamePopup();

    bool entryMatchesFilter(const Entry& e) const;

    void duplicateEntry(const Entry& e);
    void deleteEntry(const Entry& e);
    void openRenameFor(const Entry& e);

    EditorUI*                          m_ui = nullptr;
    std::vector<Entry>                 m_entries;
    bool                               m_refreshed = false;

    // Filtros:
    char                               m_searchBuf[128] = {};
    std::string                        m_categoryFilter;  // "" = sin filtro

    // Seleccion:
    std::filesystem::path              m_selectedPath;

    // Modales:
    bool                               m_newPopupOpen = false;
    std::string                        m_newId;
    int                                m_newTemplateIdx = 0;  // 0 = vacio
    std::filesystem::path              m_renameTarget;
    std::string                        m_renameNewId;
};

} // namespace Mood
