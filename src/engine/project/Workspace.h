#pragma once

// Workspace estilo Blender (F2H7). Cada workspace guarda un layout
// serializado de ImGui (qué paneles visibles, dónde, en qué tamaño).
// Switching = cambiar el iniLayout activo y dejar que ImGui re-aplique.
//
// El struct es PURO (no toca ImGui directo). Las llamadas a
// `ImGui::SaveIniSettingsToMemory` / `LoadIniSettingsFromMemory` viven
// en el caller (UI layer); el WorkspaceManager solo guarda los strings.

#include "core/Types.h"

#include <string>

namespace Mood {

struct Workspace {
    /// F2H44: ID ASCII estable del workspace. Valores actuales:
    /// `"layout"` / `"scripting"` / `"materials"` / `"map_editor"`. El
    /// label visible para la pestana viene de `T("workspace.<id>")` y
    /// cambia con el idioma activo. El ID se persiste en `.moodproj`
    /// — `WorkspaceManager::migrateWorkspaceName` mappea labels viejos
    /// (espanol pre-F2H44, F2H22 originales) a los IDs nuevos.
    std::string name;

    /// Layout completo de ImGui serializado (output de
    /// `ImGui::SaveIniSettingsToMemory`). Vacío = se construye un
    /// default con `ImGui::DockBuilder` cuando el workspace se active
    /// por primera vez.
    std::string iniLayout;
};

} // namespace Mood
