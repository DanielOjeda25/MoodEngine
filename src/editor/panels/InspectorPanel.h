#pragma once

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;
class AssetManager;

class InspectorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Inspector"; }

    /// @brief Inyecta EditorUI para leer la entidad seleccionada.
    ///        Non-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    /// @brief Inyecta el AssetManager para poblar dropdowns (texturas, audio)
    ///        sin acoplar Inspector a EditorApplication. Non-owning.
    void setAssetManager(AssetManager* am) { m_assets = am; }

    /// @brief Flag de cambio: true cuando el usuario edito un componente
    ///        este frame. `EditorApplication` lo consume tras `ui.draw()`
    ///        para llamar `markDirty()`.
    bool consumeEditedFlag() {
        const bool r = m_editedThisFrame;
        m_editedThisFrame = false;
        return r;
    }

private:
    EditorUI* m_ui = nullptr;
    AssetManager* m_assets = nullptr;
    bool m_editedThisFrame = false;
};

} // namespace Mood
