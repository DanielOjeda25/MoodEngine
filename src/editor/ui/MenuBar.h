#pragma once

// Barra de menu superior del editor. En Hito 1 casi todos los items son
// placeholders: muestran un popup "No implementado" o registran un log.

namespace Mood {

class EditorUI;

class MenuBar {
public:
    /// @brief Dibuja la menu bar dentro de la ventana host del dockspace.
    ///        Debe llamarse mientras la ventana host este activa (Begin/End).
    ///        Recibe referencia al EditorUI para poder tocar visibilidad de
    ///        paneles desde el menu "Ver".
    void draw(EditorUI& ui, bool& requestQuit);

private:
    bool m_showAboutPopup = false;
    bool m_showNotImplementedPopup = false;
};

} // namespace Mood
