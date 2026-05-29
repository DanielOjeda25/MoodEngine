#pragma once

// F3H31: renderer del sky procedural Hosek-Wilkie a un cubemap dinamico.
//
// Pipeline:
//   1) Mantiene internamente un cubemap GL handle (256x256x6, RGB16F) +
//      un FBO con 6 face attachments.
//   2) `render(sunDir, turbidity, groundAlbedo)`:
//      a) computa coeficientes Hosek-Wilkie CPU-side (HosekWilkie.cpp).
//      b) Para cada face del cubemap: bind face al FBO, calcula
//         invViewProjection de captura, dibuja fullscreen quad con
//         procedural_sky.vert/frag.
//   3) `cubemap()` devuelve el GL handle para que IBLBaker lo procese.
//
// Costo: ~3-5ms por render full (6 faces x 1 fullscreen quad).
// No se llama every-frame; el SceneRenderer lo dispara solo cuando
// `EnvironmentComponent::skyDirty` esta seteado.

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <memory>

namespace Mood {

class OpenGLShader;

class ProceduralSkyRenderer {
public:
    static constexpr int k_cubemapSize = 256;

    ProceduralSkyRenderer();
    ~ProceduralSkyRenderer();

    ProceduralSkyRenderer(const ProceduralSkyRenderer&) = delete;
    ProceduralSkyRenderer& operator=(const ProceduralSkyRenderer&) = delete;

    /// @brief Renderea el sky a las 6 faces del cubemap interno con la
    ///        configuracion dada.
    /// @param sunDirection    direccion del sol normalizada (Y up).
    /// @param turbidity       atmosfera [1, 10].
    /// @param groundAlbedo    albedo del suelo [0, 1] por canal.
    /// @return true si el render se completo. false si el shader no
    ///         pudo cargarse al construir el renderer.
    bool render(const glm::vec3& sunDirection,
                float turbidity,
                const glm::vec3& groundAlbedo);

    /// @brief Devuelve el GL handle del cubemap procedural — listo para
    ///        que IBLBaker lo procese o para usar como skybox visible.
    ///        Generado una sola vez en el ctor y mantenido.
    GLuint cubemap() const { return m_cubemap; }

private:
    void initCubemap();
    void initFbo();

    GLuint m_cubemap = 0;
    GLuint m_fbo = 0;
    GLuint m_emptyVao = 0;  // VAO vacio para fullscreen quad sin vertex buffer

    std::unique_ptr<OpenGLShader> m_shader;
};

} // namespace Mood
