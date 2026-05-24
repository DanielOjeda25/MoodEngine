#pragma once

// Panel "User Preferences" (F3H2): edita la config per-instalacion del dev
// (vive en %APPDATA%\MoodEngine\settings.json — modulo `UserSettings`).
// Floating window centered + size fijo (estilo Unity Preferences), no
// dockeable, no resizable — gemelo del ProjectSettingsPanel de F3H1.
// Default oculto, se abre desde `Edit > Preferences...` en MenuBar.
//
// F3H2 chasis: una seccion "General". F3H7: TabBar reintroducido con
// 2 tabs (General + Editor). Editor tab = sensibilidades del editor
// (zoom orto, gizmo size, click/drag threshold).
//
// El panel NO almacena estado: lee y muta directo `UserSettings::*` y
// `I18n::currentLanguage()`. Cada cambio se aplica live (al ImGui
// style / diccionario i18n / al next frame de los call-sites) y se
// persiste al disco via `UserSettings::save()` — sin boton OK/Cancel
// (settings.json es chico, UX estilo Unity).

#include "editor/panels/IPanel.h"

namespace Mood {

class UserPreferencesPanel : public IPanel {
public:
    UserPreferencesPanel() { visible = false; }  // default hidden, opens via menu

    void onImGuiRender() override;
    const char* name() const override { return "Preferences"; }
    const char* category() const override { return "Project"; }

private:
    void drawGeneralTab();
    void drawEditorTab();  // F3H7
};

} // namespace Mood
