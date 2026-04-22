#pragma once

// Implementacion OpenGL de ITexture. Carga imagenes desde disco via stb_image.

#include "engine/render/ITexture.h"

#include <glad/gl.h>

#include <string>

namespace Mood {

class OpenGLTexture final : public ITexture {
public:
    /// @brief Carga una imagen desde disco. Formatos soportados por stb_image
    ///        (PNG, JPG, BMP, TGA, GIF, PSD, HDR, PIC).
    /// @throws std::runtime_error si falla la carga o el path no existe.
    explicit OpenGLTexture(const std::string& path);

    ~OpenGLTexture() override;

    OpenGLTexture(const OpenGLTexture&) = delete;
    OpenGLTexture& operator=(const OpenGLTexture&) = delete;

    void bind(u32 slot = 0) const override;
    void unbind() const override;

    u32 width() const override { return m_width; }
    u32 height() const override { return m_height; }
    TextureHandle handle() const override;

private:
    GLuint m_texture = 0;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace Mood
