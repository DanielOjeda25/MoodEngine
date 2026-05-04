#pragma once

// Estado global del editor: que camara esta activa, si se capturan
// entradas como FPS, etc. Compartido entre EditorApplication, EditorUI y
// los sub-componentes de la UI (MenuBar, StatusBar).

namespace Mood {

enum class EditorMode {
    /// Editor Mode: camara orbital, paneles interactivos, mouse libre.
    Editor,
    /// Play Mode: camara FPS, WASD + mouse capturado. Sale con Esc.
    Play,
};

/// @brief Acciones de proyecto que la UI solicita y EditorApplication atiende.
enum class ProjectAction {
    None,
    NewProject,
    OpenProject,
    Save,
    SaveAs,
    CloseProject,
    PackageProject, // Hito 21 Bloque 5: empaqueta el proyecto activo
    NewScript,      // Hito 22 Bloque 3: crea assets/scripts/<nombre>.lua
    // F2H8: gestion multi-mapa intra-proyecto.
    NewMap,
    SaveMapAs,
    SetCurrentMapAsDefault,
    DeleteCurrentMap,
    AddBoxBrush, // F2H11: agrega un brush CSG Box al mapa actual
};

} // namespace Mood
