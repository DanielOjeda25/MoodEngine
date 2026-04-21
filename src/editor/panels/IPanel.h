#pragma once

// Interfaz base comun a todos los paneles acoplables del editor.

#include <string>

namespace Mood {

class IPanel {
public:
    virtual ~IPanel() = default;

    /// @brief Se llama una vez por frame mientras el panel este visible.
    ///        Aqui el panel emite sus llamadas a ImGui.
    virtual void onImGuiRender() = 0;

    /// @brief Nombre legible del panel, usado en el menu "Ver" y en el titulo
    ///        de la ventana ImGui.
    virtual const char* name() const = 0;

    /// @brief Toggle de visibilidad. El EditorUI revisa este flag antes de
    ///        invocar onImGuiRender.
    bool visible = true;
};

} // namespace Mood
