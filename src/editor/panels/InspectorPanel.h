#pragma once

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;

class InspectorPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Inspector"; }

    /// @brief Inyecta EditorUI para leer la entidad seleccionada.
    ///        Non-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

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
    bool m_editedThisFrame = false;
};

} // namespace Mood
