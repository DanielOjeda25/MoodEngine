#pragma once

// F2H64 Bloque A: framebuffer dedicado al pase OIT (Order-Independent
// Transparency Weighted Blended, McGuire & Bavoil 2013). Dos color
// attachments: `accumColor` RGBA16F (loc 0) acumula radiance * weight,
// `revealage` R16F (loc 1) acumula el producto de (1 - opacity). El
// depth attachment es la textura del scene FB (compartida via
// glFramebufferTexture2D con el texture id externo) para que el depth-test
// funcione contra los opacos sin blit ni resolve. El composite pass lee
// los dos attachments y los mezcla sobre el scene color.

#include "core/Types.h"

#include <glad/gl.h>

namespace Mood {

class OpenGLOitFramebuffer {
public:
    /// @brief Construye el FBO con dos color attachments (accumColor
    ///        RGBA16F + revealage R16F) y attachea `sharedDepthTextureId`
    ///        como depth-stencil compartido. El caller (SceneRenderer) es
    ///        dueño del depth texture; este FBO solo lo referencia.
    OpenGLOitFramebuffer(u32 width, u32 height, GLuint sharedDepthTextureId);
    ~OpenGLOitFramebuffer();

    OpenGLOitFramebuffer(const OpenGLOitFramebuffer&) = delete;
    OpenGLOitFramebuffer& operator=(const OpenGLOitFramebuffer&) = delete;

    void bind();
    void unbind();

    /// @brief Recrea los attachments con el nuevo size + re-attachea el
    ///        depth externo (el caller pasa el id actualizado porque el
    ///        scene FB puede haber recreado su depth texture en el resize).
    void resize(u32 width, u32 height, GLuint sharedDepthTextureId);

    u32 width() const { return m_width; }
    u32 height() const { return m_height; }

    GLuint glAccumTextureId()    const { return m_accumTexture; }
    GLuint glRevealageTextureId() const { return m_revealageTexture; }

private:
    void invalidate(GLuint sharedDepthTextureId);

    GLuint m_fbo = 0;
    GLuint m_accumTexture = 0;
    GLuint m_revealageTexture = 0;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace Mood
