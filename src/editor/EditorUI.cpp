#include "editor/EditorUI.h"

namespace Mood {

EditorUI::EditorUI() {
    m_panels = {&m_viewport, &m_inspector, &m_assetBrowser, &m_console};
}

void EditorUI::draw(bool& requestQuit) {
    // Status bar primero: usa BeginViewportSideBar que reserva una franja
    // inferior del viewport. Al dibujarse ANTES del dockspace, este toma
    // solo el area disponible restante y los paneles docked no se solapan
    // con la status bar.
    m_statusBar.draw(m_mode);

    if (m_dockspace.begin()) {
        m_menuBar.draw(*this, requestQuit);
    }
    m_dockspace.end();

    for (IPanel* panel : m_panels) {
        if (panel->visible) {
            panel->onImGuiRender();
        }
    }
}

} // namespace Mood
