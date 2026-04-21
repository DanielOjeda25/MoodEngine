#include "engine/render/opengl/OpenGLFramebuffer.h"

#include "core/Log.h"

#include <stdexcept>

namespace Mood {

OpenGLFramebuffer::OpenGLFramebuffer(u32 width, u32 height) {
    m_width = width;
    m_height = height;
    invalidate();
}

OpenGLFramebuffer::~OpenGLFramebuffer() {
    if (m_depthRbo != 0) {
        glDeleteRenderbuffers(1, &m_depthRbo);
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

    // Color attachment: textura RGBA8.
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_colorTexture, 0);

    // Depth attachment: renderbuffer (no lo necesitamos para samplear, solo
    // para el Z-buffer al dibujar).
    glGenRenderbuffers(1, &m_depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_depthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("OpenGLFramebuffer: framebuffer incompleto");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
