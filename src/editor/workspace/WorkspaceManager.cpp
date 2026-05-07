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
    // F2H22: nombres orientados a TAREAS, no a categorias de panels.
    // Cada workspace describe que hace el dev ahi, no que contiene.
    // Los iniLayout vacios fuerzan que la primera activacion pase
    // por DockBuilder para construir el layout default.
    return {
        Workspace{"Modelar",    {}},
        Workspace{"Programar",  {}},
        Workspace{"Optimizar",  {}},
        Workspace{"Materiales", {}},
    };
}

/// @brief F2H22: migra nombres viejos del workspace (F2H7 originales)
///        al esquema F2H22 orientado a tareas. Si el nombre es
///        actualmente reconocido como nuevo, se preserva tal cual.
///        El `iniLayout` se conserva intacto (la migracion es solo
///        del label).
std::string migrateWorkspaceName(const std::string& oldName) {
    if (oldName == "Layout")    return "Modelar";
    if (oldName == "Scripting") return "Programar";
    if (oldName == "Profile")   return "Optimizar";
    if (oldName == "Materials") return "Materiales";
    return oldName;  // ya nuevo o custom — preservar.
}

} // namespace

WorkspaceManager::WorkspaceManager()
    : m_workspaces(defaultWorkspaces()), m_activeIndex(0) {}

void WorkspaceManager::setWorkspaces(std::vector<Workspace> workspaces) {
    if (workspaces.empty()) {
        // Defensivo: nunca dejar la lista vacia.
        m_workspaces = defaultWorkspaces();
    } else {
        // F2H22: aplicar migracion de nombres viejos -> nuevos antes de
        // aceptar. Preserva iniLayout intacto.
        for (auto& ws : workspaces) {
            ws.name = migrateWorkspaceName(ws.name);
        }
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
