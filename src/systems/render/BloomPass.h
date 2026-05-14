#pragma once

// BloomPass (F2H55 Bloque B): efecto bloom physical-based via mip-chain.
// Algoritmo COD Advanced Warfare 2014 (Sledgehammer/Activision) -
// estandar de la industria desde hace 10 anos. Shaders portados de
// Godot 4 (MIT) y Filament (Apache 2.0).
//
// Flow:
//   1. Downsample chain: src HDR -> mip[0] (con threshold + Karis) ->
//      mip[1] -> ... -> mip[5]. Cada mip es la mitad del anterior.
//   2. Upsample chain con blending aditivo: mip[5] tent-filtered se
//      suma a mip[4], luego mip[4] a mip[3], etc. Al final mip[0]
//      contiene el bloom acumulado de todos los niveles.
//   3. Composite: src + mip[0] * intensity -> dst HDR.
//
// El caller pasa dos FBs HDR del mismo tamano. Si bloom esta apagado
// (intensity == 0) no se llama apply() y el destino debe leer el src
// directamente (la decision queda en SceneRenderer).
//
// Cost a 1920x1080:
//   - 6 mips RGBA16F (1/2, 1/4, ..., 1/64): ~10 MB total
//   - 3 fullscreen passes adicionales: ~1 ms en GPUs modernas
//
// No reentrante. Una instancia por hilo de render.

#include "core/Types.h"

#include <glad/gl.h>

#include <array>
#include <memory>

namespace Mood {

class IShader;
class OpenGLFramebuffer;

class BloomPass {
public:
    BloomPass();
    ~BloomPass();

    BloomPass(const BloomPass&) = delete;
    BloomPass& operator=(const BloomPass&) = delete;

    /// @brief Recrea el mip chain interno si las dimensiones cambiaron.
    ///        Idempotente: llamadas con el mismo size no hacen nada.
    ///        Llamar antes del primer apply() y cada vez que `src`
    ///        cambie de size (resize del viewport).
    void resize(u32 width, u32 height);

    /// @brief Aplica bloom: lee `src` (HDR scene), escribe `dst`
    ///        (HDR scene + bloom contribution).
    /// @param src       Framebuffer HDR (RGBA16F) con el render de la escena.
    /// @param dst       Framebuffer HDR (RGBA16F) del mismo tamano que src.
    ///                  Se sobreescribe completo.
    /// @param threshold Luminance threshold (HDR units). Pixels por debajo
    ///                  se atenuan suavemente. Tipicos 0.8 - 1.2.
    /// @param intensity Blend factor del bloom sobre el HDR. 0 = sin
    ///                  bloom (= src), 1 = peso completo. Default 0.6.
    /// @param radius    Escala del tent filter en upsample. 0.5 - 3.0.
    ///                  Mas alto = halo mas amplio.
    /// @return true si se escribio a `dst` (bloom aplicado). false si se
    ///         skipeo (mip chain no construible -- el caller deberia
    ///         seguir usando `src` para el post-process).
    bool apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
               f32 threshold, f32 intensity, f32 radius);

    /// @brief Cantidad de mips del chain. Constante, expuesto para tests.
    static constexpr u32 mipCount() { return k_mipCount; }

private:
    void downsamplePass(GLuint srcTextureId, u32 srcWidth, u32 srcHeight,
                        OpenGLFramebuffer& dst, bool isFirstPass,
                        f32 threshold);
    void upsamplePass(GLuint srcTextureId, u32 srcWidth, u32 srcHeight,
                      OpenGLFramebuffer& dst, f32 radius);
    void compositePass(GLuint srcHdrId, GLuint bloomTexId,
                       OpenGLFramebuffer& dst, f32 intensity);

    static constexpr u32 k_mipCount = 6;

    std::unique_ptr<IShader> m_downsampleShader;
    std::unique_ptr<IShader> m_upsampleShader;
    std::unique_ptr<IShader> m_compositeShader;

    // mips[0] = src/2, mips[1] = src/4, ..., mips[5] = src/64.
    // Recreados por resize() cada vez que cambian width/height.
    std::array<std::unique_ptr<OpenGLFramebuffer>, k_mipCount> m_mips;

    GLuint m_vao = 0;
    u32    m_currentWidth = 0;
    u32    m_currentHeight = 0;
};

} // namespace Mood
