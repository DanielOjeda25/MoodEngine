#include "editor/EditorUI.h"

namespace Mood {

EditorUI::EditorUI() {
    m_panels = {&m_viewport, &m_inspector, &m_assetBrowser};
}

void EditorUI::draw(bool& requestQuit) {
    if (m_dockspace.begin()) {
        m_menuBar.draw(*this, requestQuit);
    }
    m_dockspace.end();

    for (IPanel* panel : m_panels) {
        if (panel->visible) {
            panel->onImGuiRender();
        }
    }

    m_statusBar.draw(m_mode);
}

} // namespace Mood
