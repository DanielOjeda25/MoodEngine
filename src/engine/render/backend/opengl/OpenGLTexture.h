#pragma once

// Implementacion OpenGL de ITexture. Carga imagenes desde disco via stb_image.

#include "engine/render/rhi/ITexture.h"

#include <glad/gl.h>

#include <string>
#include <vector>

namespace Mood {

class OpenGLTexture final : public ITexture {
public:
    /// @brief Carga una imagen desde disco. Formatos soportados por stb_image
    ///        (PNG, JPG, BMP, TGA, GIF, PSD, HDR, PIC).
    /// @throws std::runtime_error si falla la carga o el path no existe.
    explicit OpenGLTexture(const std::string& path);

    /// @brief Carga una imagen desde un buffer en memoria (PNG/JPG/etc
    ///        comprimido). Para texturas embedded en .glb extraidas via
    ///        `aiScene::GetEmbeddedTexture`.
    /// @param bytes Buffer con el archivo de imagen completo (PNG, JPG, etc).
    /// @param nameForLog Nombre para los logs (no path real).
    /// @throws std::runtime_error si stb_image no puede decodificar.
    OpenGLTexture(const std::vector<u8>& bytes, const std::string& nameForLog);

    ~OpenGLTexture() override;

    OpenGLTexture(const OpenGLTexture&) = delete;
    OpenGLTexture& operator=(const OpenGLTexture&) = delete;

    void bind(u32 slot = 0) const override;
    void unbind() const override;

    u32 width() const override { return m_width; }
    u32 height() const override { return m_height; }
    TextureHandle handle() const override;

private:
    /// Helper compartido: sube los pixeles ya decodificados (RGBA8 u
    /// otro formato segun `channels`) a una nueva GL texture, configura
    /// filters/wrap/mipmaps, y popula m_texture/m_width/m_height.
    void uploadDecoded(unsigned char* pixels, int w, int h, int channels,
                        const std::string& sourceForLog);

    GLuint m_texture = 0;
    u32 m_width = 0;
    u32 m_height = 0;
};

} // namespace Mood
