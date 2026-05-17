#pragma once

// Implementacion OpenGL de IFramebuffer. Crea un FBO con color attachment
// (textura) + depth renderbuffer, lista para ser mostrada dentro de ImGui.

#include "engine/render/rhi/IFramebuffer.h"
#include "engine/render/rhi/RendererTypes.h"
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

    OpenGLFramebuffer(u32 width, u32 height, Format format = Format::LDR,
                       bool withNormalRT = false);
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

    /// @brief F2H56: GLuint del depth attachment cuando es textura (modo HDR).
    ///        Devuelve 0 si el depth es renderbuffer (modo LDR). Usado por
    ///        SSAOPass para samplear depth del scene FB.
    GLuint glDepthTextureId() const { return m_depthTexture; }

    /// @brief F2H61: GLuint del normal RT (G-buffer parcial para SSR).
    ///        Devuelve 0 si el FB se construyo sin `withNormalRT`. Solo
    ///        valido en modo HDR — los PBR shaders escriben view-space
    ///        normal al location 1; otros shaders (skybox/particles/debug)
    ///        no tocan el location 1 y dejan los pixels con el clear (0,0,0,0)
    ///        — el SSR usa alpha < 0.5 como flag "no-PBR / skip".
    GLuint glNormalTextureId() const { return m_normalTexture; }

    /// @brief Formato del color attachment (LDR/HDR).
    Format format() const { return m_format; }

private:
    /// @brief Libera los handles actuales y recrea el FBO con `m_width/m_height`.
    void invalidate();

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRbo = 0;          // LDR: depth-stencil renderbuffer
    GLuint m_depthTexture = 0;      // HDR (F2H56): depth como textura sampleable
    GLuint m_normalTexture = 0;     // HDR (F2H61): view-space normal RT, solo si withNormalRT
    u32 m_width = 0;
    u32 m_height = 0;
    Format m_format = Format::LDR;
    bool   m_withNormalRT = false;
};

} // namespace Mood
