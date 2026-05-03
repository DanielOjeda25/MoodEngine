#include "engine/render/backend/opengl/OpenGLTexture.h"

#include "core/Log.h"

#include <stb_image.h>

#include <stdexcept>

namespace Mood {

void OpenGLTexture::uploadDecoded(unsigned char* pixels, int w, int h, int channels,
                                     const std::string& sourceForLog) {
    // Mapear canales a formato GL. Soportamos 1 (R), 3 (RGB), 4 (RGBA).
    GLenum dataFormat = 0;
    GLint internalFormat = 0;
    switch (channels) {
        case 1:
            dataFormat = GL_RED;
            internalFormat = GL_R8;
            break;
        case 3:
            dataFormat = GL_RGB;
            internalFormat = GL_RGB8;
            break;
        case 4:
            dataFormat = GL_RGBA;
            internalFormat = GL_RGBA8;
            break;
        default:
            throw std::runtime_error("OpenGLTexture: cantidad de canales no soportada (" +
                                     std::to_string(channels) + ") en " + sourceForLog);
    }

    m_width = static_cast<u32>(w);
    m_height = static_cast<u32>(h);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    // Alineacion de filas: por defecto 4 bytes. Formatos RGB (3 bpp) con
    // anchos no multiplos de 4 rompen si no ajustamos.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                 0, dataFormat, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    Log::render()->info("Textura cargada: {} ({}x{}, {} canales)",
                         sourceForLog, m_width, m_height, channels);
}

OpenGLTexture::OpenGLTexture(const std::string& path) {
    // stb_image carga con origen arriba-izquierda; OpenGL espera abajo-izquierda
    // para samplear con V=0 en el borde inferior. Pedimos el flip al cargar.
    stbi_set_flip_vertically_on_load(true);

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, 0);
    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("stbi_load fallo para '" + path + "': " +
                                 (reason != nullptr ? reason : "desconocido"));
    }
    try {
        uploadDecoded(pixels, w, h, channels, path);
    } catch (...) {
        stbi_image_free(pixels);
        throw;
    }
    stbi_image_free(pixels);
}

OpenGLTexture::OpenGLTexture(const std::vector<u8>& bytes, const std::string& nameForLog) {
    // Mismo flip que la carga desde disco (consistencia con todas las
    // texturas que pasan por OpenGLTexture).
    stbi_set_flip_vertically_on_load(true);

    int w = 0;
    int h = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(),
                                              static_cast<int>(bytes.size()),
                                              &w, &h, &channels, 0);
    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("stbi_load_from_memory fallo para '" + nameForLog + "': " +
                                 (reason != nullptr ? reason : "desconocido"));
    }
    try {
        uploadDecoded(pixels, w, h, channels, nameForLog);
    } catch (...) {
        stbi_image_free(pixels);
        throw;
    }
    stbi_image_free(pixels);
}

OpenGLTexture::~OpenGLTexture() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

void OpenGLTexture::bind(u32 slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}

void OpenGLTexture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

TextureHandle OpenGLTexture::handle() const {
    return reinterpret_cast<TextureHandle>(static_cast<uintptr_t>(m_texture));
}

} // namespace Mood
