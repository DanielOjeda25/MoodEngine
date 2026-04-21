#include "engine/render/opengl/OpenGLMesh.h"

#include "core/Assert.h"

namespace Mood {

OpenGLMesh::OpenGLMesh(const std::vector<f32>& vertices,
                       const std::vector<VertexAttribute>& attributes) {
    // Calcular stride (total de componentes por vertice) para validar la
    // consistencia de los datos de entrada y para los glVertexAttribPointer.
    u32 stride = 0;
    for (const auto& attr : attributes) {
        stride += attr.componentCount;
    }
    MOOD_ASSERT(stride > 0, "OpenGLMesh: attribute layout vacio");
    MOOD_ASSERT(vertices.size() % stride == 0,
                "OpenGLMesh: vertices.size() no es multiplo del stride");

    m_vertexCount = static_cast<u32>(vertices.size() / stride);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(f32)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    // Configurar cada atributo. Los offsets se acumulan en bytes a medida que
    // se recorren los componentes declarados.
    GLuint offsetBytes = 0;
    for (const auto& attr : attributes) {
        glEnableVertexAttribArray(attr.location);
        glVertexAttribPointer(attr.location,
                              static_cast<GLint>(attr.componentCount),
                              GL_FLOAT,
                              GL_FALSE,
                              static_cast<GLsizei>(stride * sizeof(f32)),
                              reinterpret_cast<const void*>(static_cast<uintptr_t>(offsetBytes)));
        offsetBytes += attr.componentCount * sizeof(f32);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

OpenGLMesh::~OpenGLMesh() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void OpenGLMesh::bind() const {
    glBindVertexArray(m_vao);
}

void OpenGLMesh::unbind() const {
    glBindVertexArray(0);
}

} // namespace Mood
