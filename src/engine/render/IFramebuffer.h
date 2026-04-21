#pragma once

// Interfaz abstracta de un framebuffer offscreen. En Hito 2 se usa para
// renderizar el viewport del editor a una textura que luego ImGui muestra
// dentro del panel Viewport con ImGui::Image.

#include "engine/render/RendererTypes.h"
#include "core/Types.h"

namespace Mood {

class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;

    /// @brief Activa el framebuffer: los proximos draw calls escriben aqui.
    virtual void bind() = 0;

    /// @brief Restaura el framebuffer por defecto (el de la ventana).
    virtual void unbind() = 0;

    /// @brief Cambia el tamano del framebuffer. Si el tamano coincide con el
    ///        actual, la implementacion puede no hacer nada.
    virtual void resize(u32 width, u32 height) = 0;

    /// @brief Ancho y alto actuales del framebuffer.
    virtual u32 width() const = 0;
    virtual u32 height() const = 0;

    /// @brief Handle opaco del color attachment, para pasarlo a ImGui::Image.
    virtual TextureHandle colorAttachmentHandle() const = 0;
};

} // namespace Mood
