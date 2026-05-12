#pragma once

// ItemBrowserPanel (F2H51 Bloque F): lista todos los `.mooditem` del
// proyecto activo + filtros (search + tag) + acciones (new / duplicate
// / rename / delete). Click sobre una card selecciona el item; el
// `ItemPropertyEditorPanel` (Bloque G) consume la seleccion para
// renderizar los campos editables.
//
// El path donde viven los items es `<cwd>/assets/items/` por convencion,
// mismo patron que `assets/dialogs/` (F2H47) o `assets/materials/`
// (F2H42).
//
// Grid layout v1: cards de ~120x80 px con nombre del item arriba y
// categoria abajo (primer tag o "(sin categoría)"). El icono real
// requiere pipeline de texturas — diferido al render del Property
// Editor (Bloque G).

#include "editor/panels/IPanel.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class EditorUI;

class ItemBrowserPanel : public IPanel {
public:
    ItemBrowserPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Item Browser"; }
    const char* category() const override { return "Assets"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief Re-escanea el directorio `assets/items/` y reparsea los
    ///        `.mooditem` para refrescar nombres + tags. Llamar tras
    ///        crear/borrar/renombrar un item (o cuando cambia el proyecto).
    void refresh();

    /// @brief Path del item seleccionado actualmente. Vacio si no hay
    ///        seleccion. El Property Editor (Bloque G) lo lee cada
    ///        frame y renderiza los campos del asset.
    const std::filesystem::path& selectedPath() const { return m_selectedPath; }

private:
    /// @brief Entrada del listado: path + metadata cacheada para no
    ///        reparsear el .mooditem en cada frame.
    struct Entry {
        std::filesystem::path     path;
        std::string               displayName;  // name_literal o id
        std::string               category;     // primer tag o "(sin categoría)"
        std::vector<std::string>  tags;         // para el filtro
    };

    /// @brief Resuelve el directorio `<cwd>/assets/items/`. Crea el
    ///        directorio si no existe (mismo patron que DialogBrowser).
    std::optional<std::filesystem::path> itemsDir() const;

    void loadEntries();
    void drawToolbar();
    void drawFilters();
    void drawGrid();
    void drawNewItemPopup();
    void drawRenamePopup();

    /// @brief true si la entry pasa el filtro de search + tag activo.
    bool entryMatchesFilter(const Entry& e) const;

    /// @brief Acciones del right-click context menu sobre una entry.
    void duplicateEntry(const Entry& e);
    void deleteEntry(const Entry& e);
    void openRenameFor(const Entry& e);

    EditorUI*                          m_ui = nullptr;
    std::vector<Entry>                 m_entries;
    bool                               m_refreshed = false;

    // Filtros:
    char                               m_searchBuf[128] = {};
    std::string                        m_tagFilter;       // "" = sin filtro

    // Seleccion:
    std::filesystem::path              m_selectedPath;    // empty = no selection

    // Modales:
    bool                               m_newPopupOpen = false;
    std::string                        m_newId;
    int                                m_newTemplateIdx = 0;  // 0 = vacio
    std::filesystem::path              m_renameTarget;    // empty = popup cerrado
    std::string                        m_renameNewId;
};

} // namespace Mood
