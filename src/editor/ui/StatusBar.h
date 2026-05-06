#pragma once

// Barra de estado inferior del editor. Muestra FPS, modo actual y un mensaje
// libre de estado.

#include "core/Types.h"
#include "editor/application/EditorMode.h"

#include <string>

namespace Mood {

class StatusBar {
public:
    /// @brief Dibuja la status bar. Se posiciona pegada al borde inferior de
    ///        la viewport principal.
    void draw(EditorMode mode);

    void setFps(f32 fps) { m_fps = fps; }
    void setMessage(std::string msg) { m_message = std::move(msg); }
    /// @brief F2H16: nombre del ultimo comando ejecutado (Blender-style
    ///        "Last Operator"). Sirve para que el dev sepa que va a
    ///        deshacer Ctrl+Z. Vacio = no muestra nada.
    void setLastCommand(std::string name) { m_lastCommand = std::move(name); }

private:
    f32 m_fps = 0.0f;
    std::string m_message = "Listo";
    std::string m_lastCommand;
};

} // namespace Mood
