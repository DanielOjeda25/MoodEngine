#pragma once

// Implementacion OpenGL de IFramebuffer. Crea un FBO con color attachment
// (textura) + depth renderbuffer, lista para ser mostrada dentro de ImGui.

#include "engine/render/IFramebuffer.h"
#include "engine/render/RendererTypes.h"
#include "core/Types.h"

#include <glad/gl.h>

namespace Mood {

class OpenGLFramebuffer final : public IFramebuffer {
public:
    OpenGLFramebuffer(u32 width, u32 height);
    ~OpenGLFramebuffer() override;

    OpenGLFramebuffer(const OpenGLFramebuffer&) = delete;
    OpenGLFramebuffer& operator=(const OpenGLFramebuffer&) = delete;

    void bind() override;
    void unbind() override;
    void resize(u32 width, u32 height) override;

    u32 width() const override { return m_width; }
    u32 height() const override { return m_height; }

    TextureHandle colorAttachmentHandle() const override;

private:
    /// @brief Libera los handles actuales y recrea el FBO con `m_width/m_height`.
    void invalidate();

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRbo = 0;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace Mood
