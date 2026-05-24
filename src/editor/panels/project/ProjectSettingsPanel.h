#pragma once

// Panel "Project Settings" (F3H1): edita la config per-proyecto que vive
// en .moodproj > "settings" (struct ProjectSettings). Dockeable, default
// oculto — se abre desde `Edit > Project Settings...` en MenuBar o desde
// `View > Project > Project Settings`.
//
// F3H1 (chasis): tabs General (2 fields prueba) + 3 placeholders para
// las sub-secciones que se llenan en F3H4+ (SpawnDefaults / Rendering /
// Physics).
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
    void drawGeneralTab(ProjectSettings& settings);
    void drawPlaceholderTab();

    EditorUI* m_ui = nullptr;
};

} // namespace Mood
