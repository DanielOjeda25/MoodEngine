#include "engine/render/opengl/OpenGLCubemapTexture.h"

#include <stb_image.h>

#include <stdexcept>
#include <string>

namespace Mood {

OpenGLCubemapTexture::OpenGLCubemapTexture(const std::array<std::string, 6>& paths) {
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);

    // OpenGL espera origen en la esquina superior izquierda al cargar
    // caras de cubemap (a diferencia de las texturas 2D donde flippeamos
    // verticalmente). NO setear `stbi_set_flip_vertically_on_load` aca:
    // las caras del skybox ya quedaron generadas con la orientacion
    // correcta por el script Python (origin top-left, igual al cubemap).
    stbi_set_flip_vertically_on_load(false);

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
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8,
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
