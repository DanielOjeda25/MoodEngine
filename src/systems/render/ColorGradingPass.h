#pragma once

// ColorGradingPass (F2H58 Bloque B): aplica una LUT 2D (256x16, layout
// Unity URP) sobre la escena HDR. Single fullscreen pass entre bloom
// (F2H55) y post-process (tonemap + gamma).
//
// Diferencias con BloomPass / SSAOPass: este pass NO tiene FBs internos
// ni mip chain. Es un single draw call. El caller le pasa src + dst FBs
// HDR del mismo tamano + el handle de la LUT (cargada via AssetManager
// o sintetizada como identity).
//
// Si la LUT es invalida (id == 0 o size != 256x16), el caller debe
// skipearlo y usar el src directo. La clase no chequea la LUT --
// chequea solo que `intensity > 0` (early-out por perf).
//
// No reentrante. Una instancia por hilo de render.

#include "core/Types.h"

#include <glad/gl.h>

#include <memory>

namespace Mood {

class IShader;
class OpenGLFramebuffer;

class ColorGradingPass {
public:
    ColorGradingPass();
    ~ColorGradingPass();

    ColorGradingPass(const ColorGradingPass&) = delete;
    ColorGradingPass& operator=(const ColorGradingPass&) = delete;

    /// @brief Aplica color grading: lee `src` (HDR), mezcla con la LUT
    ///        segun `intensity`, escribe `dst` (HDR).
    /// @param src       Framebuffer HDR origen (post bloom + SSAO).
    /// @param dst       Framebuffer HDR destino (mismo tamano que src).
    /// @param lutTextureId  Handle GL de una textura 2D 256x16 RGBA. Si
    ///                       es 0, el caller deberia skipear esta llamada.
    /// @param intensity Blend factor [0,1]. 0 = sin grade (escribe src
    ///                  intacto a dst). 1 = grade puro.
    /// @return true si se escribio a dst. false si se skipeo (intensity
    ///         clamped 0 o lutTextureId == 0).
    bool apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
               GLuint lutTextureId, f32 intensity);

private:
    std::unique_ptr<IShader> m_shader;
    GLuint                   m_vao = 0;
};

} // namespace Mood
