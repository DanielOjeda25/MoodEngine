#pragma once

// Panel "User Preferences" (F3H2): edita la config per-instalacion del dev
// (vive en %APPDATA%\MoodEngine\settings.json — modulo `UserSettings`).
// Floating window centered + size fijo (estilo Unity Preferences), no
// dockeable, no resizable — gemelo del ProjectSettingsPanel de F3H1.
// Default oculto, se abre desde `Edit > Preferences...` en MenuBar.
//
// F3H2 (chasis): una sola seccion "General" con Tema + Idioma. El TabBar
// se reintroduce cuando hitos siguientes sumen Shortcuts (F3H6),
// Autosave (F3H7+), font size, etc.
//
// El panel NO almacena estado: lee y muta directo `UserSettings::theme()`
// y `I18n::currentLanguage()`. Cada cambio se aplica live (al ImGui style
// o al diccionario i18n) y se persiste al disco via `UserSettings::save()`
// — sin boton OK/Cancel (settings.json es chico, UX estilo Unity).

#include "editor/panels/IPanel.h"

namespace Mood {

class UserPreferencesPanel : public IPanel {
public:
    UserPreferencesPanel() { visible = false; }  // default hidden, opens via menu

    void onImGuiRender() override;
    const char* name() const override { return "Preferences"; }
    const char* category() const override { return "Project"; }

private:
    void drawGeneralSection();
};

} // namespace Mood
