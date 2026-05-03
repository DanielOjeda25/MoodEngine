#pragma once

// Cubemap texture para skyboxes y reflexiones (Hito 15 Bloque 1).
//
// Carga 6 caras (cada una un PNG cuadrado del mismo tamano) en un
// `GL_TEXTURE_CUBE_MAP`. Filtros LINEAR + clamp_to_edge en S/T/R para
// evitar seams visibles entre caras.
//
// Convencion de paths: las 6 caras se pasan en orden positive-X, negative-X,
// positive-Y, negative-Y, positive-Z, negative-Z (mismo orden que los
// targets de OpenGL `GL_TEXTURE_CUBE_MAP_POSITIVE_X` + offset).
//
// Por ahora no abstraemos en una `ICubemapTexture`: tenemos un solo
// backend de render y un solo skybox. La interfaz se puede extraer cuando
// aparezca un segundo consumidor (reflexiones IBL en Hito 17).

#include "core/Types.h"

#include <glad/gl.h>

#include <array>
#include <string>
#include <vector>

namespace Mood {

class OpenGLCubemapTexture {
public:
    /// @brief Carga 6 archivos PNG (uno por cara) en un cubemap. Si alguno
    ///        falla, lanza `std::runtime_error` con el path culpable.
    /// @param paths Array de 6 paths del filesystem en orden:
    ///              [+X, -X, +Y, -Y, +Z, -Z].
    /// @param sRgb  Si true (default), las texturas se cargan como
    ///              `GL_SRGB8_ALPHA8` y GL aplica decode automatico al
    ///              samplear: los PNG con colores tipo skybox / IBL son
    ///              sRGB-encoded y necesitan esta conversion para sumarse
    ///              correctamente en el espacio linear del shader. Pasar
    ///              `false` para datos numericos puros (BRDF LUT, etc.).
    explicit OpenGLCubemapTexture(const std::array<std::string, 6>& paths,
                                   bool sRgb = true);

    /// @brief Hito 17: cubemap con mip chain explicita para IBL prefilter.
    ///        Cada `mips[i]` es el set de 6 caras de un nivel de mip
    ///        (mips[0] = mip 0, mips[N-1] = mip mas bajo). Sample con
    ///        `textureLod(cube, dir, lod)` en el shader.
    /// @param mips Vector de mip levels; cada nivel es un array de 6 paths.
    /// @param sRgb Igual que el ctor de single-mip.
    explicit OpenGLCubemapTexture(
        const std::vector<std::array<std::string, 6>>& mips,
        bool sRgb = true);

    ~OpenGLCubemapTexture();

    OpenGLCubemapTexture(const OpenGLCubemapTexture&) = delete;
    OpenGLCubemapTexture& operator=(const OpenGLCubemapTexture&) = delete;

    /// @brief Bindea el cubemap al texture unit `slot`.
    void bind(u32 slot = 0) const;

    /// @brief GLuint del cubemap (raw, no encapsulado en TextureHandle: este
    ///        cubemap no se muestra desde ImGui).
    GLuint glHandle() const { return m_id; }

    /// @brief Cantidad de niveles de mip (1 si se construyo desde 6 PNGs sin
    ///        chain). Sirve al shader para clamping del LOD del prefilter.
    u32 mipLevels() const { return m_mipLevels; }

private:
    GLuint m_id = 0;
    u32 m_mipLevels = 1;
};

} // namespace Mood
