#include "engine/render/backend/opengl/OpenGLFramebuffer.h"

#include "core/Log.h"

#include <stdexcept>

namespace Mood {

OpenGLFramebuffer::OpenGLFramebuffer(u32 width, u32 height, Format format,
                                       bool withNormalRT) {
    m_width = width;
    m_height = height;
    m_format = format;
    // Normal RT solo aplica en modo HDR — el LDR final no lo necesita.
    m_withNormalRT = withNormalRT && (format == Format::HDR);
    invalidate();
}

OpenGLFramebuffer::~OpenGLFramebuffer() {
    if (m_depthRbo != 0) {
        glDeleteRenderbuffers(1, &m_depthRbo);
    }
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
    }
    if (m_normalTexture != 0) {
        glDeleteTextures(1, &m_normalTexture);
    }
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
    }
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
    }
}

void OpenGLFramebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void OpenGLFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFramebuffer::resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        // Tamano degenerado: ignorar silenciosamente para no crashear al
        // colapsar un panel.
        return;
    }
    if (width == m_width && height == m_height) {
        return;
    }
    m_width = width;
    m_height = height;
    invalidate();
}

TextureHandle OpenGLFramebuffer::colorAttachmentHandle() const {
    // Se devuelve como void*: ImGui::Image espera un ImTextureID que en el
    // backend OpenGL3 de ImGui es el GLuint de la textura como void*.
    return reinterpret_cast<TextureHandle>(static_cast<uintptr_t>(m_colorTexture));
}

void OpenGLFramebuffer::invalidate() {
    // Destruir los recursos anteriores si existian (resize recrea desde cero
    // para mantener el codigo simple; en un motor maduro se puede reutilizar
    // la textura con glTexImage2D si el formato no cambia).
    if (m_depthRbo != 0) {
        glDeleteRenderbuffers(1, &m_depthRbo);
        m_depthRbo = 0;
    }
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    if (m_normalTexture != 0) {
        glDeleteTextures(1, &m_normalTexture);
        m_normalTexture = 0;
    }
    if (m_colorTexture != 0) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Color attachment: RGBA8 (LDR) o RGBA16F (HDR).
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    if (m_format == Format::HDR) {
        // Internal format float, datos los manda el shader; el `type` no
        // importa porque el buffer arranca clear, igual lo seteamos en
        // FLOAT por consistencia.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                     0, GL_RGBA, GL_FLOAT, nullptr);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_colorTexture, 0);

    // F2H61: Normal RT opcional (G-buffer parcial para SSR). RGBA16F para
    // mantener precision de [-1,1] sin packing tricks. El alpha indica si
    // el pixel fue escrito por un shader que emite normal (PBR variantes);
    // shaders no-PBR (skybox/particles/debug) no escriben location 1 y
    // dejan el alpha en 0 — el SSR usa eso como flag "no reflejar aca".
    if (m_withNormalRT) {
        glGenTextures(1, &m_normalTexture);
        glBindTexture(GL_TEXTURE_2D, m_normalTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                     0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D, m_normalTexture, 0);

        // Habilitar ambos draw buffers: location 0 = color, location 1 = normal.
        const GLenum drawBufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, drawBufs);
    }

    // Depth attachment:
    //   - HDR (F2H56): textura DEPTH24_STENCIL8 sampleable (SSAOPass lee
    //     depth para reconstruir posiciones y calcular oclusion).
    //   - LDR: renderbuffer (mas eficiente; el viewport FB final no necesita
    //     samplear su propio depth).
    if (m_format == Format::HDR) {
        glGenTextures(1, &m_depthTexture);
        glBindTexture(GL_TEXTURE_2D, m_depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
                     static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                     0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, m_depthTexture, 0);
    } else {
        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                              static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depthRbo);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("OpenGLFramebuffer: framebuffer incompleto");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
