#include "editor/workspace/WorkspaceManager.h"

#include <algorithm>  // F2H23: std::any_of

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
    // F2H23 polish: "Modelar" -> "Layout" (pedido del dev), workspace
    // "Optimizar" eliminado (era para benchmark Fase 1, no flujo
    // cotidiano del dev de juegos). Los iniLayout vacios fuerzan que
    // la primera activacion pase por DockBuilder para construir el
    // layout default.
    return {
        Workspace{"Layout",     {}},
        Workspace{"Programar",  {}},
        Workspace{"Materiales", {}},
    };
}

/// @brief F2H22: migra nombres viejos del workspace (F2H7 originales)
///        al esquema F2H22 orientado a tareas. Si el nombre es
///        actualmente reconocido como nuevo, se preserva tal cual.
///        El `iniLayout` se conserva intacto (la migracion es solo
///        del label).
///        F2H23 polish: la migracion ahora apunta de F2H22 -> F2H23:
///        "Modelar" -> "Layout" (revert del rename) y filtra
///        "Optimizar"/"Profile" (ya no existe el workspace; iniLayout
///        del usuario para esos se descarta — el indice se queda con
///        los 3 workspaces actuales).
std::string migrateWorkspaceName(const std::string& oldName) {
    if (oldName == "Layout")    return "Layout";     // F2H7 original
    if (oldName == "Modelar")   return "Layout";     // F2H22 -> F2H23
    if (oldName == "Scripting") return "Programar";
    if (oldName == "Materials") return "Materiales";
    return oldName;  // ya nuevo o custom — preservar.
    // Nota: "Profile" / "Optimizar" caen aqui — el filtro en
    // setWorkspaces() los descarta (workspace ya no existe).
}

/// @brief F2H23 polish: workspaces validos del esquema actual. Para
///        filtrar entries del .moodproj que apuntan a workspaces
///        eliminados (Optimizar/Profile).
bool isValidWorkspaceName(const std::string& name) {
    return name == "Layout" || name == "Programar" || name == "Materiales";
}

} // namespace

WorkspaceManager::WorkspaceManager()
    : m_workspaces(defaultWorkspaces()), m_activeIndex(0) {}

void WorkspaceManager::setWorkspaces(std::vector<Workspace> workspaces) {
    if (workspaces.empty()) {
        // Defensivo: nunca dejar la lista vacia.
        m_workspaces = defaultWorkspaces();
        m_activeIndex = 0;
        return;
    }

    // F2H22+F2H23: aplicar migracion de nombres viejos -> nuevos +
    // filtrar workspaces obsoletos (Optimizar/Profile ya no existe).
    // Preserva iniLayout intacto para los validos.
    std::vector<Workspace> migrated;
    migrated.reserve(workspaces.size());
    for (auto& ws : workspaces) {
        ws.name = migrateWorkspaceName(ws.name);
        if (isValidWorkspaceName(ws.name)) {
            migrated.push_back(std::move(ws));
        }
        // workspace obsoleto (ej. Profile/Optimizar): se descarta.
    }

    // Si tras la migracion el set quedo vacio o le faltan workspaces,
    // completar con defaults para garantizar los 3 estandar.
    if (migrated.empty()) {
        m_workspaces = defaultWorkspaces();
    } else {
        // Asegurar que los 3 workspaces estandar existen. Si el
        // .moodproj solo trae 2, agregar el faltante con iniLayout
        // vacio.
        const auto defaults = defaultWorkspaces();
        for (const auto& def : defaults) {
            const bool present = std::any_of(
                migrated.begin(), migrated.end(),
                [&](const Workspace& w) { return w.name == def.name; });
            if (!present) migrated.push_back(def);
        }
        m_workspaces = std::move(migrated);
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
