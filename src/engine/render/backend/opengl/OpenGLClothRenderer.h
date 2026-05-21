#pragma once

// F2H75 Bloque E: renderer de telas (cloth). Dibuja el buffer interleaved
// que el `ClothSystem` produce cada frame en `ClothComponent::renderVertices`
// (pos.xyz + normal.xyz por vertice, triangulos expandidos, world space).
//
// Mesh dinamico: VBO `GL_DYNAMIC_DRAW` re-subido por cloth con el patron
// orphan + glBufferSubData (mismo que OpenGLParticleRenderer). Lit simple
// (1 direccional + ambiente) con el shader `cloth.{vert,frag}`, doble cara
// (las telas se ven de ambos lados -> cull face OFF en el scope del draw).
//
// Va en el frame DESPUES de la geometria opaca (para ocluirse por el mundo)
// — invocado por SceneRenderer junto al particle renderer.

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class Scene;

class OpenGLClothRenderer {
public:
    OpenGLClothRenderer();
    ~OpenGLClothRenderer();

    OpenGLClothRenderer(const OpenGLClothRenderer&) = delete;
    OpenGLClothRenderer& operator=(const OpenGLClothRenderer&) = delete;

    /// @brief Dibuja todas las telas de la scene al FB actual. `lightDir`
    ///        es la direccion HACIA donde apunta el sol (world); si
    ///        `lightIntensity <= 0` (no hay sun en la escena) usa un fill
    ///        default desde arriba para que la tela no quede negra.
    void render(Scene& scene,
                const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& lightDir,
                const glm::vec3& lightColor,
                f32 lightIntensity);

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizeiptr m_vboCapacityBytes = 0;

    GLuint m_program = 0;
    GLint m_uView = -1;
    GLint m_uProjection = -1;
    GLint m_uColor = -1;
    GLint m_uLightDir = -1;
    GLint m_uLightColor = -1;
    GLint m_uLightIntensity = -1;
    GLint m_uAmbient = -1;
};

} // namespace Mood
