#pragma once

// OpenGLParticleRenderer (Hito 29 Bloque 2): renderiza los emisores
// `ParticleEmitterComponent` de la `Scene` como billboards instanciados.
//
// El `ParticleSystem` ya avanzo la simulacion CPU (positions, ages,
// alive, etc.). Este renderer:
//   1) Por emisor, comprime las particulas vivas en un buffer compacto
//      de Instance{center, size, color} aplicando lerp size/color por
//      `age/lifetime`.
//   2) Lo sube al VBO dinamico (orphan + glBufferSubData con growth 2x).
//   3) Bindea la textura del emisor (o missing si no hay) + setea
//      blend (alpha o aditivo segun `em.additive`) y dibuja con
//      `glDrawArraysInstanced(GL_TRIANGLES, 0, 6, n)`.
//
// Estado GL preservado: depth write OFF y blend ON dentro del scope
// del render; se restauran al volver. El depth TEST queda activo (las
// particulas se ocluyen por geometria opaca pero no escriben en el
// depth buffer asi se mezclan correctamente entre si).

#include "core/Types.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>

#include <vector>

namespace Mood {

class AssetManager;
class Scene;

class OpenGLParticleRenderer {
public:
    OpenGLParticleRenderer();
    ~OpenGLParticleRenderer();

    OpenGLParticleRenderer(const OpenGLParticleRenderer&) = delete;
    OpenGLParticleRenderer& operator=(const OpenGLParticleRenderer&) = delete;

    /// @brief Itera todos los emisores de la `scene` y los dibuja al
    ///        framebuffer actualmente bindeado. El caller asegura que
    ///        depth test esta activo y el FB destino es el HDR de la
    ///        escena.
    void render(Scene& scene,
                AssetManager& assets,
                const glm::mat4& view,
                const glm::mat4& projection);

private:
    struct Instance {
        f32 center[3];
        f32 size;
        f32 color[4];
    };

    std::vector<Instance> m_cpu;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizeiptr m_vboCapacityBytes = 0;

    GLuint m_program = 0;
    GLint m_uView = -1;
    GLint m_uProjection = -1;
    GLint m_uTexture = -1;
};

} // namespace Mood
