#pragma once

// F2H60 (= F2H22 del plan original): depth-only framebuffer + textura
// 2D ARRAY para Cascade Shadow Maps. Reemplaza al `OpenGLShadowMap` de
// Hito 16 cuando se usan multiples cascadas.
//
// Diseno:
//   - 1 FBO compartido. Por cada cascada, antes de renderear, se
//     re-attacha el layer correspondiente del array como depth target
//     via `glFramebufferTextureLayer`. Mas simple que N FBOs.
//   - 1 textura `GL_TEXTURE_2D_ARRAY` con `cascadeCount` layers de
//     `size x size` cada uno. El lit shader lo samplea con
//     `sampler2DArrayShadow` (PCF hardware compare por capa).
//   - Comparison mode + filtering + clamp-to-border iguales que el
//     `OpenGLShadowMap` single. Sin diferencias funcionales por layer
//     (todas las cascadas comparten resolution + format).
//
// Texture format / filtering / wrap / compare son identicos a
// OpenGLShadowMap para preservar el comportamiento del sampleShadow del
// lit.frag pre-CSM.

#include "core/Types.h"

#include <glad/gl.h>

namespace Mood {

class OpenGLShadowMapArray {
public:
    /// @brief Crea el FBO + textura 2D array `size x size x cascadeCount`.
    /// @throws std::runtime_error si el FB queda incompleto en el primer
    ///         layer probado.
    OpenGLShadowMapArray(u32 size, u32 cascadeCount);

    ~OpenGLShadowMapArray();

    OpenGLShadowMapArray(const OpenGLShadowMapArray&) = delete;
    OpenGLShadowMapArray& operator=(const OpenGLShadowMapArray&) = delete;

    /// @brief Bindea el FBO + re-attacha `layerIdx` del array como depth
    ///        target + setea el viewport a `size x size`. Llamar antes
    ///        de renderear cada cascada.
    ///
    ///        F2H64: `withColor=true` ademas attachea el layer del color
    ///        attachment (sombras tintadas). Cuando false (default,
    ///        backward-compat), DrawBuffer queda en GL_NONE como en F2H60.
    void bindLayerAsTarget(u32 layerIdx, bool withColor = false) const;

    /// @brief Restaura el framebuffer por defecto.
    void unbind() const;

    /// @brief GLuint de la textura 2D-array para que el lit shader la
    ///        bindee como `sampler2DArrayShadow`.
    GLuint glDepthTextureId() const { return m_depthTexture; }

    /// @brief F2H64: GLuint de la color array RGBA8 para sombras tintadas.
    ///        El pbr.frag la samplea como `sampler2DArray` y multiplica
    ///        la dir light por shadowColor.rgb.
    GLuint glColorTextureId() const { return m_colorTexture; }

    u32 size()         const { return m_size; }
    u32 cascadeCount() const { return m_cascadeCount; }

private:
    GLuint m_fbo            = 0;
    GLuint m_depthTexture   = 0;
    GLuint m_colorTexture   = 0;   // F2H64: RGBA8 array para sombras tintadas
    u32    m_size           = 0;
    u32    m_cascadeCount   = 0;
};

} // namespace Mood
