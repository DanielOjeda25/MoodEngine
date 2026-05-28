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

#include "core/UserSettings.h"
#include "editor/panels/IPanel.h"

namespace Mood {

class UserPreferencesPanel : public IPanel {
public:
    UserPreferencesPanel() { visible = false; }  // default hidden, opens via menu

    void onImGuiRender() override;
    const char* name() const override { return "Preferences"; }
    const char* category() const override { return "Project"; }

private:
    /// F3H26: categorías del sidebar tipo Blender. Cada item del lateral
    /// renderea una vista distinta a la derecha — más legible que el
    /// TabBar horizontal previo que tenía 2 tabs gigantes.
    enum class Category {
        General       = 0,  ///< Tema + idioma (per-instalación).
        Viewport      = 1,  ///< Ortho zoom + gizmos + click/drag threshold + smooth view + render mode.
        Assets        = 2,  ///< Thumbnail resolution + hover preview delay + inspector category.
        Performance   = 3,  ///< Stats overlay + profiler ring buffer.
        Notifications = 4,  ///< Toasts + autosave del mapa.
    };

    /// F3H26: helpers para cada sección. `cfg`, `dirty`, `saveNow` los
    /// comparten todos — son refs a locales de onImGuiRender.
    void drawSidebar();
    void drawGeneral();
    void drawViewport(UserSettings::EditorSettings& cfg,
                      const UserSettings::EditorSettings& defaults,
                      bool& dirty, bool& saveNow);
    void drawAssets(UserSettings::EditorSettings& cfg,
                    const UserSettings::EditorSettings& defaults,
                    bool& dirty, bool& saveNow);
    void drawPerformance(UserSettings::EditorSettings& cfg,
                         const UserSettings::EditorSettings& defaults,
                         bool& dirty, bool& saveNow);
    void drawNotifications(UserSettings::EditorSettings& cfg,
                           const UserSettings::EditorSettings& defaults,
                           bool& dirty, bool& saveNow);

    Category m_activeCategory = Category::General;

    /// F3H24: tracking de cambios entre apertura y cierre del panel.
    /// Setea true cualquier setter que muta UserSettings; al detectar
    /// `visible` transición true→false, si hay cambios emite un toast
    /// "Preferencias guardadas" y resetea el flag.
    bool m_changedSinceOpen = false;
    bool m_wasVisibleLastFrame = false;
};

} // namespace Mood
