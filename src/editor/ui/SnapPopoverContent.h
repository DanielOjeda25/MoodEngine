#pragma once

// F3H28: helper extraido del difunto MapEditorTopBar (F3H6 polish). El
// popover de configuración fina del snap (steps disponibles + umbrales)
// ahora vive como helper reusable. La categoría "Map Tools" del Inspector
// es el único caller actual.

namespace Mood {

class EditorUI;
struct Project;

/// Dibuja el contenido del popover de configuración del snap. El caller
/// es responsable de abrir el popup con ImGui::BeginPopup / EndPopup.
/// Storage en `project->settings.snap`; cambios marcan dirty via `ui`.
void drawSnapPopoverContent(EditorUI* ui, Project* project);

} // namespace Mood
