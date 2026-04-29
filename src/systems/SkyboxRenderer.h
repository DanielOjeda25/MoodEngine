#pragma once

// SkyboxRenderer (Hito 15 Bloque 1): pinta un cubemap detras de la escena.
//
// Diseno minimo: owns el cubemap (`OpenGLCubemapTexture`), el shader
// `shaders/skybox.{vert,frag}` y un VAO/VBO con un cubo unitario de solo
// posiciones. `draw(view, projection)` dibuja el cubo con depth=1 (truco
// pos.z=w en el vertex shader) — depth test debe estar en LEQUAL. Llamar
// PRIMERO en el frame para que la escena escriba encima.
//
// La translacion de `view` se anula (mat3->mat4) para que el sky no se
// "desplace" cuando la camara se mueve — solo rota.

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

#include <memory>
#include <string>

namespace Mood {

class IShader;
class ITexture;
class OpenGLCubemapTexture;

class SkyboxRenderer {
public:
    /// @brief Tag para el constructor equirectangular (ver factory).
    struct Equirect {};

    /// @brief Construye el renderer cargando el cubemap (6 PNGs en orden
    ///        +X, -X, +Y, -Y, +Z, -Z) + compilando el shader.
    /// @param cubemapDirFs Path del filesystem a la carpeta del skybox
    ///        (ej. `"assets/skyboxes/sky_day"`). Se buscan los archivos
    ///        `px.png`, `nx.png`, `py.png`, `ny.png`, `pz.png`, `nz.png`.
    /// @throws std::runtime_error si falla la carga del cubemap o la
    ///        compilacion del shader.
    explicit SkyboxRenderer(const std::string& cubemapDirFs);

    /// @brief Modo equirectangular: una unica textura 2D panoramica
    ///        (proporcion 2:1) se samplea con (atan2,asin) en el
    ///        fragment shader. Sin seams en los polos del cubemap.
    /// @param equirectPngFs Path al archivo PNG (ej.
    ///        `"assets/skyboxes/sky_kloofendal.png"`).
    SkyboxRenderer(Equirect, const std::string& equirectPngFs);

    ~SkyboxRenderer();

    SkyboxRenderer(const SkyboxRenderer&) = delete;
    SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;

    /// @brief Dibuja el skybox. El depth test debe estar activo con func
    ///        LEQUAL (ver `OpenGLRenderer::beginFrame`). El caller decide
    ///        el orden — debe llamar a `draw` ANTES de la geometria de la
    ///        escena.
    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    void initCubeBuffers();

    std::unique_ptr<OpenGLCubemapTexture> m_cubemap;       // modo cubemap
    std::unique_ptr<ITexture> m_equirect;                  // modo equirectangular
    std::unique_ptr<IShader> m_shader;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    u32 m_vertexCount = 0;
    bool m_isEquirect = false;
};

} // namespace Mood
