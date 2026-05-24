#pragma once

// Panel "Project Settings" (F3H1): edita la config per-proyecto que vive
// en .moodproj > "settings" (struct ProjectSettings). Floating window
// centered + size fijo (estilo Unity Project Settings), no dockeable, no
// resizable. Default oculto — se abre desde `Edit > Project Settings...`
// en MenuBar.
//
// F3H1 (chasis): una sola seccion "Performance" con Target FPS como
// Combo de presets (30/60/120/144). El TabBar se reintroduce cuando
// los hitos siguientes agreguen mas categorias (Spawn Defaults,
// Rendering, Physics).
//
// El panel NO almacena estado propio del settings: lee y muta directo el
// ProjectSettings del Project activo (via `EditorUI::currentProject()`).
// Si no hay proyecto activo, muestra hint en lugar del form. Cambios
// disparan `EditorUI::requestProjectDirty()` que EditorApplication
// consume para llamar `markDirty()`.

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;
struct ProjectSettings;

class ProjectSettingsPanel : public IPanel {
public:
    ProjectSettingsPanel() { visible = false; }  // default hidden, opens via menu

    void onImGuiRender() override;
    const char* name() const override { return "Project Settings"; }
    const char* category() const override { return "Project"; }

    /// @brief Inyectado por EditorUI ctor para acceder al proyecto + dirty.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    void drawPerformanceSection(ProjectSettings& settings);
    void drawGameplaySection(ProjectSettings& settings);  // F3H4
    void drawCharacterSection(ProjectSettings& settings); // F3H5
    // F3H6 polish: snap se movio al MapEditorTopBar (popover) — no es
    // un setting "del proyecto" en sentido conceptual sino del editor
    // de mapas. Storage sigue en .moodproj > settings.snap (per-project
    // sigue siendo correcto), solo la UI cambio de lugar.

    EditorUI* m_ui = nullptr;
};

} // namespace Mood
