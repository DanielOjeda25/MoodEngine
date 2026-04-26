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
    /// @brief Formato del color attachment.
    ///   - LDR (default): `GL_RGBA8`. Lo que muestra ImGui directo.
    ///   - HDR (Hito 15): `GL_RGBA16F`. Permite valores > 1.0 sin clip,
    ///     necesario para pasar por exposicion + tonemap. NO se le puede
    ///     pasar como ImTextureID a `ImGui::Image` (asume sRGB); usar la
    ///     textura LDR del post-process pass para mostrar.
    enum class Format { LDR = 0, HDR = 1 };

    OpenGLFramebuffer(u32 width, u32 height, Format format = Format::LDR);
    ~OpenGLFramebuffer() override;

    OpenGLFramebuffer(const OpenGLFramebuffer&) = delete;
    OpenGLFramebuffer& operator=(const OpenGLFramebuffer&) = delete;

    void bind() override;
    void unbind() override;
    void resize(u32 width, u32 height) override;

    u32 width() const override { return m_width; }
    u32 height() const override { return m_height; }

    TextureHandle colorAttachmentHandle() const override;

    /// @brief GLuint del color attachment, util para shaders del motor que
    ///        samplean este FB (post-process). NO se expone via IFramebuffer
    ///        para no filtrar el tipo concreto del backend.
    GLuint glColorTextureId() const { return m_colorTexture; }

    /// @brief Formato del color attachment (LDR/HDR).
    Format format() const { return m_format; }

private:
    /// @brief Libera los handles actuales y recrea el FBO con `m_width/m_height`.
    void invalidate();

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRbo = 0;
    u32 m_width = 0;
    u32 m_height = 0;
    Format m_format = Format::LDR;
};

} // namespace Mood
