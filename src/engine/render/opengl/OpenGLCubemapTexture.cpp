#include "engine/render/opengl/OpenGLCubemapTexture.h"

#include <stb_image.h>

#include <stdexcept>
#include <string>

namespace Mood {

OpenGLCubemapTexture::OpenGLCubemapTexture(const std::array<std::string, 6>& paths,
                                            bool sRgb) {
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    // OpenGL espera origen en la esquina superior izquierda al cargar
    // caras de cubemap (a diferencia de las texturas 2D donde flippeamos
    // verticalmente). NO setear `stbi_set_flip_vertically_on_load` aca:
    // las caras del skybox ya quedaron generadas con la orientacion
    // correcta por el script Python (origin top-left, igual al cubemap).
    stbi_set_flip_vertically_on_load(false);

    // Internal format: SRGB8 si los PNG estan gamma-encoded (color data
    // tipo skybox o IBL); RGBA8 si son numericos (LUT, etc.). Sin sRGB
    // los pixeles se interpretan como linear en el shader y al pasar por
    // el post-process gamma-encode terminan duplicados (todo muy claro).
    const GLint internalFmt = sRgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    for (u32 i = 0; i < 6; ++i) {
        int w = 0, h = 0, channels = 0;
        unsigned char* data =
            stbi_load(paths[i].c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (data == nullptr) {
            // Restaurar el flag global antes de tirar para no contaminar
            // las cargas de textura 2D que vienen despues.
            stbi_set_flip_vertically_on_load(true);
            glDeleteTextures(1, &m_id);
            m_id = 0;
            throw std::runtime_error(
                std::string("OpenGLCubemapTexture: no se pudo cargar '") +
                paths[i] + "': " + (stbi_failure_reason() ? stbi_failure_reason() : "?"));
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFmt,
                     w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    // Filtros + wrap: clamp_to_edge en S/T/R evita seams visibles entre
    // caras adyacentes cuando la sample direction cae cerca del borde.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    // Restaurar flip para texturas 2D que vendran despues (mismo proceso
    // de stb).
    stbi_set_flip_vertically_on_load(true);
}

OpenGLCubemapTexture::OpenGLCubemapTexture(
        const std::vector<std::array<std::string, 6>>& mips,
        bool sRgb) {
    if (mips.empty()) {
        throw std::runtime_error("OpenGLCubemapTexture: mips vacio");
    }
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    stbi_set_flip_vertically_on_load(false);

    const GLint internalFmt = sRgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    for (u32 lvl = 0; lvl < mips.size(); ++lvl) {
        for (u32 face = 0; face < 6; ++face) {
            int w = 0, h = 0, channels = 0;
            unsigned char* data = stbi_load(mips[lvl][face].c_str(),
                                             &w, &h, &channels, STBI_rgb_alpha);
            if (data == nullptr) {
                stbi_set_flip_vertically_on_load(true);
                glDeleteTextures(1, &m_id);
                m_id = 0;
                throw std::runtime_error(
                    std::string("OpenGLCubemapTexture (mip): no se pudo cargar '") +
                    mips[lvl][face] + "': " +
                    (stbi_failure_reason() ? stbi_failure_reason() : "?"));
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                         static_cast<GLint>(lvl), internalFmt,
                         w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // Limitar el LOD al ultimo mip que cargamos (sino GL puede pedir mips
    // que no existen y el shader devuelve negro).
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(mips.size() - 1));

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    stbi_set_flip_vertically_on_load(true);

    m_mipLevels = static_cast<u32>(mips.size());
}

OpenGLCubemapTexture::~OpenGLCubemapTexture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
    }
}

void OpenGLCubemapTexture::bind(u32 slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
}

} // namespace Mood
