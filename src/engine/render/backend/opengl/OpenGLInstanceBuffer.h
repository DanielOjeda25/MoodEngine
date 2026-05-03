#pragma once

// VBO de atributos de instancia para `glDrawArraysInstanced` (F2H4). El
// uso tipico es:
//
//   m_instanceBuffer->upload(matrices.data(), matrices.size() * sizeof(glm::mat4));
//   m_instanceBuffer->bindAsAttributeMat4(/*location=*/5);
//   m_renderer->drawMeshInstanced(mesh, shader, matrices.size());
//
// La estrategia de upload es **orphan-then-fill** (`glBufferData` con
// `GL_DYNAMIC_DRAW`): el driver asigna un buffer fresco cada frame, lo
// que evita CPU/GPU sync stalls cuando el GPU todavia esta usando el
// buffer del frame anterior. Para 836 mat4 (~50KB) la transferencia es
// micro-segundos.
//
// API minima — extender solo si emerge necesidad concreta (persistent
// mapped buffers, ring buffers, multi-attribute layout).

#include "core/Types.h"

namespace Mood {

class OpenGLInstanceBuffer {
public:
    OpenGLInstanceBuffer();
    ~OpenGLInstanceBuffer();

    OpenGLInstanceBuffer(const OpenGLInstanceBuffer&) = delete;
    OpenGLInstanceBuffer& operator=(const OpenGLInstanceBuffer&) = delete;

    /// @brief Sube `sizeBytes` bytes desde `data` al VBO. Estrategia
    ///        orphan: invalida el contenido anterior (si lo habia) y
    ///        asigna almacenamiento fresco — evita stalls del driver.
    void upload(const void* data, u32 sizeBytes);

    /// @brief Bindea el VBO actual a las 4 locations consecutivas
    ///        `[startLocation, startLocation+3]` como atributos `mat4`
    ///        con `glVertexAttribDivisor(*, 1)`. Asume que un VAO ya
    ///        esta bindeado (tipicamente el del mesh a dibujar).
    void bindAsAttributeMat4(u32 startLocation) const;

private:
    u32 m_vbo{0};
};

} // namespace Mood
