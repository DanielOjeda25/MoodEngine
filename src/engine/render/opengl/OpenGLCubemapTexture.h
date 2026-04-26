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

namespace Mood {

class OpenGLCubemapTexture {
public:
    /// @brief Carga 6 archivos PNG (uno por cara) en un cubemap. Si alguno
    ///        falla, lanza `std::runtime_error` con el path culpable.
    /// @param paths Array de 6 paths del filesystem en orden:
    ///              [+X, -X, +Y, -Y, +Z, -Z].
    explicit OpenGLCubemapTexture(const std::array<std::string, 6>& paths);

    ~OpenGLCubemapTexture();

    OpenGLCubemapTexture(const OpenGLCubemapTexture&) = delete;
    OpenGLCubemapTexture& operator=(const OpenGLCubemapTexture&) = delete;

    /// @brief Bindea el cubemap al texture unit `slot`.
    void bind(u32 slot = 0) const;

    /// @brief GLuint del cubemap (raw, no encapsulado en TextureHandle: este
    ///        cubemap no se muestra desde ImGui).
    GLuint glHandle() const { return m_id; }

private:
    GLuint m_id = 0;
};

} // namespace Mood
