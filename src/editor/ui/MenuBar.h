#pragma once

// Barra de menu superior del editor. Todos los items estan cableados a su
// accion real; el modal "No implementado" historico de Hito 1 fue removido
// en break-A9 (auditoria pre-Fase 3) junto con su flag.

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
    bool m_showPreferencesPopup = false;  // F2H76: request de abrir
    bool m_prefsOpen = false;             // F2H79: estado abierto (boton X del titlebar)
};

} // namespace Mood
