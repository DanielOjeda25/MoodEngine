#pragma once

// Interfaz abstracta de una textura 2D. La creacion concreta la hace cada
// backend (p.ej. OpenGLTexture carga con stb_image y sube a GPU con glTexImage2D).

#include "engine/render/RendererTypes.h"
#include "core/Types.h"

namespace Mood {

class ITexture {
public:
    virtual ~ITexture() = default;

    /// @brief Activa la textura en la unidad indicada (0..N-1). El shader
    ///        debe tener un sampler2D asociado a esa unidad.
    virtual void bind(u32 slot = 0) const = 0;
    virtual void unbind() const = 0;

    virtual u32 width() const = 0;
    virtual u32 height() const = 0;

    /// @brief Handle opaco, util para pasar a `ImGui::Image` en el editor.
    virtual TextureHandle handle() const = 0;
};

} // namespace Mood
