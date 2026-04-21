#pragma once

// Implementacion OpenGL de IMesh. Encapsula un VAO + VBO con datos de vertices
// interleaved. Por ahora no se usan EBOs: los meshes se dibujan con
// `glDrawArrays`.

#include "engine/render/IMesh.h"
#include "engine/render/RendererTypes.h"
#include "core/Types.h"

#include <glad/gl.h>

#include <vector>

namespace Mood {

class OpenGLMesh final : public IMesh {
public:
    /// @brief Crea un mesh a partir de datos interleaved de vertices.
    /// @param vertices Buffer plano de floats (ej. [px,py,pz, r,g,b, ...]).
    /// @param attributes Layout por vertice. La suma de `componentCount` debe
    ///        dividir exactamente el tamano de `vertices`.
    OpenGLMesh(const std::vector<f32>& vertices,
               const std::vector<VertexAttribute>& attributes);

    ~OpenGLMesh() override;

    OpenGLMesh(const OpenGLMesh&) = delete;
    OpenGLMesh& operator=(const OpenGLMesh&) = delete;

    void bind() const override;
    void unbind() const override;
    u32 vertexCount() const override { return m_vertexCount; }

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    u32 m_vertexCount = 0;
};

} // namespace Mood
