#pragma once

// ItemPropertyEditorPanel (F2H51 Bloque G): edita los campos del item
// actualmente seleccionado en el `ItemBrowserPanel`. Renderiza todos
// los campos del schema `.mooditem`:
//
// - Identity: id (readonly), name (toggle literal/key), description (toggle).
// - Visual: icon_path, model_path (text inputs en v1; drop target diferido).
// - Categorization: tags como chips agregables/removibles.
// - Stats: tabla key-value libre (add/remove rows).
// - Stack: checkbox stackable + spinner max_stack.
// - Slot size: spinners width x height (relevante solo para layout grid_2d).
//
// Save explicito (boton). v1 sin auto-save — el flag `m_dirty` cambia el
// titulo a "* Item Property Editor" para que el dev sepa que hay cambios
// sin guardar.

#include "editor/panels/IPanel.h"
#include "editor/panels/assets/AssetEditTracker.h"  // F2H84
#include "engine/inventory/ItemAsset.h"

#include <filesystem>
#include <string>

namespace Mood {

class EditorUI;

class ItemPropertyEditorPanel : public IPanel {
public:
    ItemPropertyEditorPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Item Property Editor"; }
    const char* category() const override { return "Gameplay"; }
    // F2H78: guarda el .mooditem con Ctrl+S al tener foco (gatea el global).
    bool consumesSaveShortcut() const override {
        return m_windowFocused && !m_loadedPath.empty();
    }
    // F2H85: Ctrl+Shift+S → "Guardar como" (pide ruta nueva). Aplica si
    // el panel tiene foco y hay un item cargado para basar la copia.
    bool consumesSaveAsShortcut() const override {
        return m_windowFocused && !m_loadedPath.empty();
    }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    /// @brief Carga el asset del path dado a memoria. Resetea m_dirty +
    ///        rederiva m_useNameKey/m_useDescKey segun los campos del
    ///        asset.
    void loadFromPath(const std::filesystem::path& fsPath);

    /// @brief Si m_loadedPath cambia entre frames (porque el browser
    ///        selecciono otro item), recarga.
    void syncWithBrowserSelection();

    // ----- Render por seccion -----

    void drawIdentitySection();
    void drawVisualSection();
    void drawTagsSection();
    void drawStatsSection();
    void drawStackSection();
    void drawSlotSizeSection();
    void drawSaveBar();

    /// @brief F2H78: escribe el item al disco + refresca el browser.
    ///        Compartido por el boton "Guardar" y el Ctrl+S contextual.
    bool saveToDisk();

    /// @brief F2H85: abre `pfd::save_file` con default = path actual
    ///        (sufijo `_copy` si ya existe) + extension `.mooditem`, copia
    ///        el item a la ruta elegida, hace switch al item nuevo (el
    ///        browser muestra el copy como cargado). Compartido por
    ///        Ctrl+Shift+S y por un futuro item de menu.
    bool saveAsToDisk();

    EditorUI*                  m_ui = nullptr;
    AssetEditTracker           m_editTracker;    // F2H84

    std::filesystem::path      m_loadedPath;     // empty = no asset cargado
    Inventory::Asset           m_loaded;
    bool                       m_dirty = false;
    bool                       m_windowFocused = false; // F2H78: foco ult. render

    // Toggles UI por field (no se persisten — derivados del state):
    bool                       m_useNameKey = false;
    bool                       m_useDescKey = false;

    // Buffers de los inputs de "agregar":
    char                       m_newTagBuf[64] = {};
    char                       m_newStatKeyBuf[64] = {};
    float                      m_newStatValue = 0.0f;
};

} // namespace Mood
