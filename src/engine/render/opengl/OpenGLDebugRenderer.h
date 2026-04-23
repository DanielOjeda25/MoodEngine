#pragma once

// Debug renderer para lineas y AABBs (Hito 4 Bloque 5). Se acumula una lista
// de vertices por frame y se flushea con GL_LINES al final. Comparte el
// contexto GL del resto del RHI; vive junto al OpenGL backend pero se puede
// extraer a una interfaz IDebugRenderer cuando aparezca un segundo backend.

#include "core/Types.h"
#include "core/math/AABB.h"

#include <glad/gl.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mood {

class OpenGLDebugRenderer {
public:
    OpenGLDebugRenderer();
    ~OpenGLDebugRenderer();

    OpenGLDebugRenderer(const OpenGLDebugRenderer&) = delete;
    OpenGLDebugRenderer& operator=(const OpenGLDebugRenderer&) = delete;

    /// @brief Encola una linea del punto `a` al `b` con el color dado.
    void drawLine(const glm::vec3& a, const glm::vec3& b,
                  const glm::vec3& color = glm::vec3(1.0f, 0.8f, 0.1f));

    /// @brief Encola las 12 aristas de una AABB en world coords.
    void drawAabb(const AABB& box,
                  const glm::vec3& color = glm::vec3(1.0f, 0.8f, 0.1f));

    /// @brief Sube los vertices acumulados a la GPU, los dibuja con GL_LINES
    ///        y limpia el buffer de CPU. Llamar una vez por frame, al final
    ///        de la escena.
    void flush(const glm::mat4& view, const glm::mat4& projection);

    /// @brief Cuantas lineas acumuladas hay (no vertices) sin flush.
    usize pendingLines() const { return m_cpu.size() / 2; }

private:
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
    };

    std::vector<Vertex> m_cpu;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizeiptr m_vboCapacityBytes = 0;

    GLuint m_program = 0;
    GLint m_uView = -1;
    GLint m_uProjection = -1;
};

} // namespace Mood
