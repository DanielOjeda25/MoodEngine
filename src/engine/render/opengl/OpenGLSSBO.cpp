#include "engine/render/opengl/OpenGLSSBO.h"

namespace Mood {

OpenGLSSBO::OpenGLSSBO() {
    glGenBuffers(1, &m_id);
}

OpenGLSSBO::~OpenGLSSBO() {
    if (m_id != 0) {
        glDeleteBuffers(1, &m_id);
    }
}

void OpenGLSSBO::upload(const void* data, GLsizeiptr bytes) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
    if (bytes <= m_capacity) {
        // Reusa storage: orphan + subdata. Evita stall si la GPU
        // todavia esta leyendo el frame anterior (driver-managed).
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, data);
    } else {
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
        m_capacity = bytes;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void OpenGLSSBO::bind(GLuint binding) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_id);
}

} // namespace Mood
