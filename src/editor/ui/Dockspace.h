#pragma once

// Configura una ventana ImGui "host" ocupando toda la viewport principal que
// aloja el DockSpace central del editor. El menu bar se dibuja dentro de esta
// ventana host para que quede integrado con el area acoplable.

namespace Mood {

class Dockspace {
public:
    /// @brief Comienza la ventana host. Debe llamarse antes de dibujar el
    ///        menu bar y los paneles. Devuelve true si la ventana quedo
    ///        abierta (siempre true actualmente, pero se devuelve por
    ///        simetria con el patron ImGui::Begin).
    bool begin();

    /// @brief Cierra la ventana host abierta con begin().
    void end();

    /// @brief Fuerza que el proximo `begin()` reconstruya el layout por
    ///        defecto ignorando lo persistido en `imgui.ini`. Util para el
    ///        item "Ver > Restablecer layout" cuando se agregan paneles
    ///        nuevos o el dev reorganizo y quiere volver a empezar.
    void requestResetToDefault() { m_resetRequested = true; }

private:
    bool m_buildAttempted = false;
    bool m_resetRequested = false;
};

} // namespace Mood
