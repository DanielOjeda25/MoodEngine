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
    // F2H44: el `name` ahora es un **ID ASCII estable** (no un label
    // visible). Razon: el label se traduce en runtime via `T("workspace.<id>")`
    // (ver MenuBar.cpp). Asi cambiar de idioma no rompe persistencia
    // del activo en `.moodproj` (que se guarda por `name`/ID).
    // Migration de IDs viejos (espanol/legacy) en `migrateWorkspaceName`.
    return {
        Workspace{"layout",     {}},
        Workspace{"scripting",  {}},
        Workspace{"materials",  {}},
        Workspace{"map_editor", {}},
        Workspace{"narrative",  {}},  // F2H46: Sub-fase 2.5 (dialogs/quests/inventory)
        Workspace{"gameplay",   {}},  // F2H51: Sub-fase 2.5 Bloque 1 (Inventario)
    };
}

/// @brief F2H22+F2H23+F2H44: migra nombres viejos del workspace al ID
///        ASCII actual. Acumula tres generaciones de mapeo:
///          F2H7 originales: ya eran IDs ingles → mapean al actual.
///          F2H22 task-oriented (espanol): "Modelar"/"Programar"/etc.
///          F2H44 normalize a IDs ASCII: el label visible viene de T().
///        El `iniLayout` se conserva intacto en todos los casos.
std::string migrateWorkspaceName(const std::string& oldName) {
    // F2H44 IDs nuevos (ya migrados): preservar.
    if (oldName == "layout"     ) return "layout";
    if (oldName == "scripting"  ) return "scripting";
    if (oldName == "materials"  ) return "materials";
    if (oldName == "map_editor" ) return "map_editor";
    if (oldName == "narrative"  ) return "narrative";   // F2H46
    if (oldName == "gameplay"   ) return "gameplay";    // F2H51
    // F2H22-F2H23 labels espanoles: mappear a IDs F2H44.
    if (oldName == "Layout"          ) return "layout";      // F2H7/F2H23
    if (oldName == "Modelar"         ) return "layout";      // F2H22 → F2H23
    if (oldName == "Programar"       ) return "scripting";   // F2H22
    if (oldName == "Materiales"      ) return "materials";   // F2H22
    if (oldName == "Editor de mapas" ) return "map_editor";  // F2H28
    // F2H22 ingles original (algunos saves muy antiguos):
    if (oldName == "Scripting"  ) return "scripting";
    if (oldName == "Materials"  ) return "materials";
    return oldName;  // custom — preservar (filtrado por isValidWorkspaceName).
    // Nota: "Profile" / "Optimizar" caen aqui — el filtro en
    // setWorkspaces() los descarta (workspace ya no existe).
}

/// @brief F2H23 polish + F2H44: workspaces validos del esquema actual.
///        Comparacion contra IDs ASCII (no labels visibles).
bool isValidWorkspaceName(const std::string& name) {
    return name == "layout"    || name == "scripting"
        || name == "materials" || name == "map_editor"
        || name == "narrative"     // F2H46
        || name == "gameplay";     // F2H51
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
