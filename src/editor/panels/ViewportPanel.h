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

private:
    IFramebuffer* m_framebuffer = nullptr;
    u32 m_desiredWidth = 0;
    u32 m_desiredHeight = 0;
};

} // namespace Mood
