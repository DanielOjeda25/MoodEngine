#include "engine/render/backend/opengl/OpenGLShadowMapArray.h"

#include "core/Log.h"

#include <stdexcept>

namespace Mood {

OpenGLShadowMapArray::OpenGLShadowMapArray(u32 size, u32 cascadeCount)
    : m_size(size), m_cascadeCount(cascadeCount) {
    if (cascadeCount == 0) {
        throw std::runtime_error(
            "OpenGLShadowMapArray: cascadeCount debe ser >= 1");
    }

    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_depthTexture);

    // GL_TEXTURE_2D_ARRAY: una textura "stack" de N slices, cada slice
    // es un depth map size x size. glTexImage3D con target ARRAY usa el
    // tercer dimension como `depth` (numero de layers), no como Z.
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 static_cast<GLsizei>(m_size),
                 static_cast<GLsizei>(m_size),
                 static_cast<GLsizei>(m_cascadeCount),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp to border + border color blanco (depth=1) para que samples
    // fuera del frustum de la luz se interpreten como "totalmente
    // iluminado" (no en sombra). Mismo criterio que OpenGLShadowMap.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr GLfloat borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Hardware compare para sampler2DArrayShadow + PCF 4 taps por layer.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE,
                     GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Validar el FBO attachando el layer 0 (los demas layers comparten
    // formato/size asi que si layer 0 da complete, los demas tambien).
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_depthTexture, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &m_depthTexture);
        glDeleteFramebuffers(1, &m_fbo);
        m_depthTexture = 0;
        m_fbo = 0;
        throw std::runtime_error(
            "OpenGLShadowMapArray: framebuffer depth-array incompleto");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    Log::render()->info(
        "OpenGLShadowMapArray inicializado ({} x {} x {} layers)",
        m_size, m_size, m_cascadeCount);
}

OpenGLShadowMapArray::~OpenGLShadowMapArray() {
    if (m_depthTexture != 0) glDeleteTextures(1, &m_depthTexture);
    if (m_fbo != 0)          glDeleteFramebuffers(1, &m_fbo);
}

void OpenGLShadowMapArray::bindLayerAsTarget(u32 layerIdx) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    // Re-attach el layer requerido. glFramebufferTextureLayer es la
    // forma standard de seleccionar un layer especifico de una textura
    // 2D-array como target del FBO. No requiere geometry shader ni
    // gl_Layer (esos serian alternativos pero mas complejos).
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_depthTexture,
                              0,
                              static_cast<GLint>(layerIdx));
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
}

void OpenGLShadowMapArray::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
