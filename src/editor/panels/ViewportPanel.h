#pragma once

#include "core/Types.h"
#include "editor/panels/IPanel.h"

namespace Mood {

class IFramebuffer;

class ViewportPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Viewport"; }

    /// @brief Asocia el framebuffer cuya color attachment se muestra en el panel.
    ///        La escena se renderiza a este FB desde fuera (EditorApplication).
    void setFramebuffer(IFramebuffer* fb) { m_framebuffer = fb; }

    /// @brief Tamano deseado por el panel en pixeles. Se actualiza al renderizar
    ///        y lo lee el runner del frame siguiente para redimensionar el FB.
    u32 desiredWidth() const { return m_desiredWidth; }
    u32 desiredHeight() const { return m_desiredHeight; }

    /// @brief Delta en pixeles del right-drag del mouse sobre el panel (rotacion).
    ///        Cero si no hay drag activo. Se resetea cada frame al renderizar.
    float cameraRotateDx() const { return m_cameraRotateDx; }
    float cameraRotateDy() const { return m_cameraRotateDy; }

    /// @brief Delta de la rueda del mouse sobre el panel (zoom). Cero si no aplica.
    float cameraWheel() const { return m_cameraWheel; }

private:
    IFramebuffer* m_framebuffer = nullptr;
    u32 m_desiredWidth = 0;
    u32 m_desiredHeight = 0;

    float m_cameraRotateDx = 0.0f;
    float m_cameraRotateDy = 0.0f;
    float m_cameraWheel = 0.0f;
    bool m_rightDragging = false;
};

} // namespace Mood
