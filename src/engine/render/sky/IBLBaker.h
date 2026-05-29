#pragma once

// F3H31: GPU IBL bake en runtime — irradiance convolution + prefilter GGX.
//
// Reemplaza el pipeline offline F2H86 (tools/bake_ibl.py) cuando el
// EnvironmentComponent esta en modo Procedural. El bake corre en GPU
// con 2 shaders dedicados (ibl_irradiance.frag + ibl_prefilter.frag).
//
// Outputs:
//   - Irradiance cubemap: 32x32x6 RGBA16F, 1 mip. Para diffuse IBL.
//   - Prefilter cubemap : 128x128x6 RGBA16F, 5 mips. Para specular IBL.
//                          Cada mip es un roughness:
//                            mip 0 (128) = roughness 0.00
//                            mip 1 (64)  = roughness 0.25
//                            mip 2 (32)  = roughness 0.50
//                            mip 3 (16)  = roughness 0.75
//                            mip 4 (8)   = roughness 1.00
//
// Costo tipico: ~50-150ms en GPU mid-range (depende sampleCount).
// No corre every-frame — solo cuando el sky cubemap cambia.

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <memory>

namespace Mood {

class OpenGLShader;

class IBLBaker {
public:
    static constexpr int k_irradianceSize  = 32;
    static constexpr int k_prefilterSize   = 128;
    static constexpr int k_prefilterMips   = 5;
    static constexpr int k_prefilterSamples = 1024;

    IBLBaker();
    ~IBLBaker();

    IBLBaker(const IBLBaker&) = delete;
    IBLBaker& operator=(const IBLBaker&) = delete;

    /// @brief Resultado del bake: 2 cubemaps GL nativos.
    /// Los handles los DEBE liberar el caller con glDeleteTextures (o
    /// adoptarlos en OpenGLCubemapTexture via AdoptHandleTag).
    struct Result {
        GLuint irradiance = 0;
        GLuint prefilter  = 0;
    };

    /// @brief Bakea irradiance + prefilter del cubemap `envCubemap`.
    /// @param envCubemap Cubemap GL del cielo (procedural o cargado).
    /// @return Result con handles GL listos. Si algun shader fallo en el
    ///         ctor del IBLBaker, devuelve {0, 0}.
    Result bake(GLuint envCubemap);

private:
    void initEmptyVao();

    GLuint createCubemap(int size, int mipLevels);

    std::unique_ptr<OpenGLShader> m_irradianceShader;
    std::unique_ptr<OpenGLShader> m_prefilterShader;

    GLuint m_fbo = 0;
    GLuint m_emptyVao = 0;
};

} // namespace Mood
