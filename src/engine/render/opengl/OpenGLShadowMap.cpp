#include "engine/render/opengl/OpenGLShadowMap.h"

#include "core/Log.h"

#include <stdexcept>

namespace Mood {

OpenGLShadowMap::OpenGLShadowMap(u32 size) : m_size(size) {
    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depthTexture);

    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp to border + border color blanco (depth=1) para que samples
    // fuera del frustum de la luz se interpreten como "totalmente
    // iluminado" (no en sombra). Sin esto, los pixeles fuera del shadow
    // map quedarian en sombra negra.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr GLfloat borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Modo sombra hardware: cuando el lit shader samplea con sampler2DShadow,
    // GL hace la comparacion automatica (depth_ref vs depth_stored) +
    // PCF en hardware sobre los 4 taps mas cercanos.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_depthTexture, 0);
    // Sin color attachment: dejamos explicito que no leemos ni escribimos
    // ningun color buffer, asi el FBO es completo.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &m_depthTexture);
        glDeleteFramebuffers(1, &m_fbo);
        m_depthTexture = 0;
        m_fbo = 0;
        throw std::runtime_error(
            "OpenGLShadowMap: framebuffer depth-only incompleto");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    Log::render()->info("OpenGLShadowMap inicializado ({} x {})", m_size, m_size);
}

OpenGLShadowMap::~OpenGLShadowMap() {
    if (m_depthTexture != 0) glDeleteTextures(1, &m_depthTexture);
    if (m_fbo != 0)          glDeleteFramebuffers(1, &m_fbo);
}

void OpenGLShadowMap::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
}

void OpenGLShadowMap::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
