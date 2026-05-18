#include "engine/render/backend/opengl/OpenGLOitFramebuffer.h"

#include "core/Log.h"

#include <stdexcept>

namespace Mood {

OpenGLOitFramebuffer::OpenGLOitFramebuffer(u32 width, u32 height,
                                            GLuint sharedDepthTextureId) {
    m_width = width;
    m_height = height;
    invalidate(sharedDepthTextureId);
}

OpenGLOitFramebuffer::~OpenGLOitFramebuffer() {
    if (m_accumTexture != 0) {
        glDeleteTextures(1, &m_accumTexture);
    }
    if (m_revealageTexture != 0) {
        glDeleteTextures(1, &m_revealageTexture);
    }
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
    }
}

void OpenGLOitFramebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

void OpenGLOitFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLOitFramebuffer::resize(u32 width, u32 height,
                                   GLuint sharedDepthTextureId) {
    if (width == 0 || height == 0) {
        return;
    }
    if (width == m_width && height == m_height) {
        // Mismo size pero el depth puede haber sido recreado por el scene FB.
        // Re-attachear sin recrear los color textures.
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, sharedDepthTextureId, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    m_width = width;
    m_height = height;
    invalidate(sharedDepthTextureId);
}

void OpenGLOitFramebuffer::invalidate(GLuint sharedDepthTextureId) {
    if (m_accumTexture != 0) {
        glDeleteTextures(1, &m_accumTexture);
        m_accumTexture = 0;
    }
    if (m_revealageTexture != 0) {
        glDeleteTextures(1, &m_revealageTexture);
        m_revealageTexture = 0;
    }
    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Accum: RGBA16F. Acumula sum(rgb * opacity * weight) en RGB y
    // sum(opacity * weight) en alpha (denominador del composite).
    glGenTextures(1, &m_accumTexture);
    glBindTexture(GL_TEXTURE_2D, m_accumTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_accumTexture, 0);

    // Revealage: R16F. Acumula product(1 - opacity) por blending
    // multiplicativo (clear a 1.0, blend ZERO/ONE_MINUS_SRC_COLOR).
    glGenTextures(1, &m_revealageTexture);
    glBindTexture(GL_TEXTURE_2D, m_revealageTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F,
                 static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, m_revealageTexture, 0);

    const GLenum drawBufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBufs);

    // Depth: textura compartida con el scene FB. Solo lectura para
    // depth-test contra opacos; no escribimos depth en OIT.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, sharedDepthTextureId, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("OpenGLOitFramebuffer: framebuffer incompleto");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
