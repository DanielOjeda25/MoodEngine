#pragma once

// Manager de workspaces estilo Blender (F2H7). Mantiene la lista de
// workspaces + cuál es el activo + helpers de switching y persistencia.
//
// PURO: no depende de ImGui ni de nlohmann/json. La capa de UI llama
// `captureCurrentLayout(ini)` con el resultado de
// `SaveIniSettingsToMemory`, y al activar un workspace lee
// `activeWorkspace().iniLayout` para pasárselo a
// `LoadIniSettingsFromMemory`. La persistencia a `.moodproj` la hace
// `ProjectSerializer` leyendo `workspaces()` directamente.

#include "core/Types.h"
#include "editor/workspace/Workspace.h"

#include <string>
#include <vector>

namespace Mood {

class WorkspaceManager {
public:
    /// Construye el manager con los 4 workspaces default vacíos
    /// (Layout / Scripting / Profile / Materials). El layout real se
    /// llena al activar cada uno por primera vez (vía DockBuilder).
    WorkspaceManager();

    /// Reemplaza la lista actual de workspaces por la pasada. Si está
    /// vacía, deja los defaults. Si no, los workspaces vienen del
    /// `.moodproj` deserializado. El activo queda en 0.
    void setWorkspaces(std::vector<Workspace> workspaces);

    /// Cambia el workspace activo. Out-of-bounds index ignorado
    /// silenciosamente. NO toca ImGui — el caller lee
    /// `activeWorkspace().iniLayout` después y lo aplica.
    void setActiveByIndex(int idx);

    /// Captura un layout ImGui (output de `SaveIniSettingsToMemory`)
    /// dentro del workspace activo. Pensado para llamar JUSTO ANTES
    /// de `setActiveByIndex` con el layout actual del editor.
    void captureCurrentLayout(std::string iniBuffer);

    /// Acceso a los workspaces (read-only) — para UI de tabs +
    /// serializer.
    const std::vector<Workspace>& workspaces() const { return m_workspaces; }
    int activeIndex() const { return m_activeIndex; }
    usize count() const { return m_workspaces.size(); }

    /// Acceso al workspace activo. Si la lista está vacía (no debería
    /// ocurrir tras el ctor), devuelve un workspace dummy estático.
    const Workspace& activeWorkspace() const;
    Workspace& activeWorkspace();

private:
    std::vector<Workspace> m_workspaces;
    int m_activeIndex{0};
};

} // namespace Mood
