#pragma once

// SSAOPass (F2H56 Bloque B): Screen-Space Ambient Occlusion.
//
// Algoritmo: Crytek 2007 base + reconstruccion de normal del depth via
// ddx/ddy (Filament/Godot 4 standard). Shaders portados de Filament
// (Apache 2.0) + Godot 4 (MIT). Detalle del algoritmo en `shaders/ssao.frag`.
//
// Flow del pass:
//   1. Lee depth del scene HDR FB + matrices proj/invProj de la camara.
//   2. Computa AO factor [0..1] por pixel (16 samples hemisferio
//      rotados por IGN noise) -> escribe a R8 FB.
//   3. Blur 4x4 cajita del AO factor -> escribe a otro R8 FB.
//   4. Composite: src HDR * AO factor -> dst HDR (modulacion del color).
//
// Limitacion v1: multiplica el color HDR final, no solo el componente
// ambient. Visualmente similar al integrarlo en PBR shader, pero menos
// correcto fisicamente. Refactor diferido a hito futuro si emerge.

#include "core/Types.h"

#include <glm/mat4x4.hpp>
#include <glad/gl.h>

#include <memory>

namespace Mood {

class IShader;
class OpenGLFramebuffer;

class SSAOPass {
public:
    SSAOPass();
    ~SSAOPass();

    SSAOPass(const SSAOPass&) = delete;
    SSAOPass& operator=(const SSAOPass&) = delete;

    /// @brief Recrea los FBs internos si las dimensiones cambiaron.
    void resize(u32 width, u32 height);

    /// @brief Aplica SSAO: lee `src` HDR + su depth texture, escribe
    ///        `dst` HDR (= src color * AO factor).
    /// @return true si SSAO se aplico; false si size invalido (caller
    ///         debe usar `src` directo para el pass siguiente).
    bool apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
               const glm::mat4& proj, const glm::mat4& invProj,
               f32 radius, f32 intensity);

private:
    void ssaoPass(GLuint depthTexId, const glm::mat4& proj,
                  const glm::mat4& invProj, f32 radius, f32 intensity);
    void blurPass();
    void compositePass(GLuint srcHdrId, OpenGLFramebuffer& dst);

    std::unique_ptr<IShader> m_ssaoShader;
    std::unique_ptr<IShader> m_blurShader;
    std::unique_ptr<IShader> m_compositeShader;

    // AO factor a half-res (R8) -- buena calidad para v1 sin costar mucho.
    std::unique_ptr<OpenGLFramebuffer> m_aoRawFb;
    std::unique_ptr<OpenGLFramebuffer> m_aoBlurFb;

    GLuint m_vao = 0;
    u32    m_currentWidth = 0;
    u32    m_currentHeight = 0;
};

} // namespace Mood
