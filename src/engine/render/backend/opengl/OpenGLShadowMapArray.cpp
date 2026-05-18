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
    glGenTextures(1, &m_colorTexture);  // F2H64: color array para sombras tintadas

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

    // F2H64: color array RGBA8 paralela. Mismo size + cascadeCount que
    // la depth array. Border blanco (1,1,1,1) = "sin tinte" para muestras
    // fuera del frustum -- la luz pasa intacta como en F2H60.
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_colorTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 static_cast<GLsizei>(m_size),
                 static_cast<GLsizei>(m_size),
                 static_cast<GLsizei>(m_cascadeCount),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Validar el FBO attachando el layer 0 (los demas layers comparten
    // formato/size asi que si layer 0 da complete, los demas tambien).
    // F2H64: validamos el modo con color attachment (mas exigente que
    // depth-only) para detectar incompatibilidad temprano.
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_depthTexture, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              m_colorTexture, 0, 0);
    const GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &m_depthTexture);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteFramebuffers(1, &m_fbo);
        m_depthTexture = 0;
        m_colorTexture = 0;
        m_fbo = 0;
        throw std::runtime_error(
            "OpenGLShadowMapArray: framebuffer depth+color array incompleto");
    }

    // Reset a depth-only por default (el opaque pass de F2H60 sigue
    // funcionando sin cambios -- DrawBuffer NONE no escribe color).
    glDrawBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    Log::render()->info(
        "OpenGLShadowMapArray inicializado ({} x {} x {} layers)",
        m_size, m_size, m_cascadeCount);
}

OpenGLShadowMapArray::~OpenGLShadowMapArray() {
    if (m_depthTexture != 0) glDeleteTextures(1, &m_depthTexture);
    if (m_colorTexture != 0) glDeleteTextures(1, &m_colorTexture);
    if (m_fbo != 0)          glDeleteFramebuffers(1, &m_fbo);
}

void OpenGLShadowMapArray::bindLayerAsTarget(u32 layerIdx, bool withColor) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    // Re-attach el layer requerido. glFramebufferTextureLayer es la
    // forma standard de seleccionar un layer especifico de una textura
    // 2D-array como target del FBO. No requiere geometry shader ni
    // gl_Layer (esos serian alternativos pero mas complejos).
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_depthTexture,
                              0,
                              static_cast<GLint>(layerIdx));
    if (withColor) {
        // F2H64: re-attach el color layer + habilitar drawBuffer 0 para
        // que el shader tinted escriba al RGBA8 array. El opaque pass
        // (withColor=false) deja drawBuffer en GL_NONE como F2H60.
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  m_colorTexture,
                                  0,
                                  static_cast<GLint>(layerIdx));
        const GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, drawBufs);
    } else {
        glDrawBuffer(GL_NONE);
    }
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
}

void OpenGLShadowMapArray::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mood
