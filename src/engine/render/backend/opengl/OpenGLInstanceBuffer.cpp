#include "engine/render/backend/opengl/OpenGLInstanceBuffer.h"

#include <glad/gl.h>

#include <glm/mat4x4.hpp>

namespace Mood {

OpenGLInstanceBuffer::OpenGLInstanceBuffer() {
    glGenBuffers(1, &m_vbo);
}

OpenGLInstanceBuffer::~OpenGLInstanceBuffer() {
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
}

void OpenGLInstanceBuffer::upload(const void* data, u32 sizeBytes) {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Orphan: pasamos size pero data null primero permite al driver dejar
    // ir el buffer anterior. Despues subimos los datos. Mas simple aun
    // (y equivalente en MSVC + NVIDIA driver actual): un solo glBufferData
    // — el driver detecta el cambio de size y orphana automaticamente.
    glBufferData(GL_ARRAY_BUFFER, sizeBytes, data, GL_DYNAMIC_DRAW);
}

void OpenGLInstanceBuffer::bindAsAttributeMat4(u32 startLocation) const {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Una mat4 = 4 columnas vec4. Cada columna es un atributo aparte;
    // GL no soporta nativamente "mat4 attribute" en una location.
    const GLsizei stride = static_cast<GLsizei>(sizeof(glm::mat4));
    for (u32 i = 0; i < 4; ++i) {
        const u32 loc = startLocation + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, stride,
                                reinterpret_cast<const void*>(
                                    static_cast<uintptr_t>(i) * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);
    }
}

} // namespace Mood
