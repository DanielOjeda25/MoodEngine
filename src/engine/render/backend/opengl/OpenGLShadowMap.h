#pragma once

// Depth-only framebuffer para shadow mapping (Hito 16 Bloque 1).
//
// Diseno: una textura `GL_DEPTH_COMPONENT24` adjunta como
// `GL_DEPTH_ATTACHMENT`, sin color attachment (`glDrawBuffer(GL_NONE)`
// + `glReadBuffer(GL_NONE)`). El shader fragment esta vacio — solo
// queremos que se escriba el depth automatico al pasar por las
// transformaciones del shadow_depth.vert.
//
// Tamano default 2048x2048: suficiente para una sala 8x8 con muros
// + props sin shadow acne visible. Resizable si entra polish futuro
// (toggle low/high quality, CSM).
//
// NO se abstrae como `IFramebuffer` porque su API es distinta (no tiene
// color attachment) y un solo consumidor (`ShadowPass`) lo va a usar.
// Si entra deferred / GBuffer (Hito 18), ahi se replantea.

#include "core/Types.h"

#include <glad/gl.h>

namespace Mood {

class OpenGLShadowMap {
public:
    /// @brief Crea el FBO + textura de profundidad cuadrada de `size` x `size`.
    /// @throws std::runtime_error si el FB queda incompleto.
    explicit OpenGLShadowMap(u32 size = 2048);

    ~OpenGLShadowMap();

    OpenGLShadowMap(const OpenGLShadowMap&) = delete;
    OpenGLShadowMap& operator=(const OpenGLShadowMap&) = delete;

    /// @brief Bindea el FBO + viewport a `size x size`. Despues de esto,
    ///        cualquier draw call escribe SOLO al depth (color descartado).
    void bind() const;

    /// @brief Restaura el framebuffer por defecto (el de la ventana).
    void unbind() const;

    /// @brief GLuint de la textura del depth attachment, para que el lit
    ///        shader lo samplee como `sampler2DShadow` o `sampler2D`.
    GLuint glDepthTextureId() const { return m_depthTexture; }

    u32 size() const { return m_size; }

private:
    GLuint m_fbo = 0;
    GLuint m_depthTexture = 0;
    u32 m_size = 0;
};

} // namespace Mood
