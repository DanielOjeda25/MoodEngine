#include "editor/workspace/WorkspaceManager.h"

namespace Mood {

namespace {

// Workspace dummy que se devuelve cuando la lista esta vacia. Vive
// como singleton estatico para que la referencia siga valida.
Workspace& dummyWorkspace() {
    static Workspace dummy;
    return dummy;
}

std::vector<Workspace> defaultWorkspaces() {
    // Los iniLayout vacios fuerzan que la primera activacion pase
    // por DockBuilder para construir el layout default.
    return {
        Workspace{"Layout",    {}},
        Workspace{"Scripting", {}},
        Workspace{"Profile",   {}},
        Workspace{"Materials", {}},
    };
}

} // namespace

WorkspaceManager::WorkspaceManager()
    : m_workspaces(defaultWorkspaces()), m_activeIndex(0) {}

void WorkspaceManager::setWorkspaces(std::vector<Workspace> workspaces) {
    if (workspaces.empty()) {
        // Defensivo: nunca dejar la lista vacia.
        m_workspaces = defaultWorkspaces();
    } else {
        m_workspaces = std::move(workspaces);
    }
    m_activeIndex = 0;
}

void WorkspaceManager::setActiveByIndex(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_workspaces.size())) return;
    m_activeIndex = idx;
}

void WorkspaceManager::captureCurrentLayout(std::string iniBuffer) {
    if (m_workspaces.empty()) return;
    if (m_activeIndex < 0 ||
        m_activeIndex >= static_cast<int>(m_workspaces.size())) return;
    m_workspaces[static_cast<usize>(m_activeIndex)].iniLayout =
        std::move(iniBuffer);
}

const Workspace& WorkspaceManager::activeWorkspace() const {
    if (m_workspaces.empty()) return dummyWorkspace();
    if (m_activeIndex < 0 ||
        m_activeIndex >= static_cast<int>(m_workspaces.size())) {
        return dummyWorkspace();
    }
    return m_workspaces[static_cast<usize>(m_activeIndex)];
}

Workspace& WorkspaceManager::activeWorkspace() {
    if (m_workspaces.empty()) return dummyWorkspace();
    if (m_activeIndex < 0 ||
        m_activeIndex >= static_cast<int>(m_workspaces.size())) {
        return dummyWorkspace();
    }
    return m_workspaces[static_cast<usize>(m_activeIndex)];
}

} // namespace Mood
