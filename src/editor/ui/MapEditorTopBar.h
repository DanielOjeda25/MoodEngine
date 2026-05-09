#pragma once

// F2H30 Bloque C: top toolbar especifica del workspace "Editor de
// mapas". Botones para sub-modos (Object / Vertex / Edge / Face) +
// pincel poligonal. Layout horizontal en la franja superior del
// dockspace cuando el workspace activo es "Editor de mapas".
//
// Estilo Hammer/TrenchBroom: el dev tiene siempre a la vista que
// modo de edicion esta activo, sin tener que mirar la status bar.
//
// Las acciones emiten `request*` al EditorUI que EditorApplication
// consume en `run()` (mismo patron que la Toolbar lateral F2H22).

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;

class MapEditorTopBar : public IPanel {
public:
    MapEditorTopBar() {
        // Arranca invisible — applyDefaultVisibilityForWorkspace lo
        // hace visible cuando el workspace activo es "Editor de mapas".
        visible = false;
    }

    void onImGuiRender() override;
    const char* name() const override { return "Map Tools"; }
    const char* category() const override { return "Scene"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    EditorUI* m_ui = nullptr;
};

} // namespace Mood
