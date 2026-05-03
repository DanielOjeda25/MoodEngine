#pragma once

// Wrapper RAII de GL_SHADER_STORAGE_BUFFER (Hito 18). SSBOs reemplazan a
// los uniform arrays cuando el contenido es dinamico en tamano (luces,
// tiles, indices del light grid). Vienen con GL 4.3+ — el motor pide GL
// 4.5 Core, asi que esta disponible.
//
// Uso tipico:
//   OpenGLSSBO buf;
//   buf.upload(myData, byteSize);
//   buf.bind(/*binding=*/0);    // matchea `layout(std430, binding=0)`
//
// upload() reusa el storage si el size es <= al previo (orphan + sub-data
// para evitar GPU stalls); creep up si supera el tamano. No hay
// double-buffering interno — el caller decide si necesita N buffers.

#include "core/Types.h"

#include <glad/gl.h>

namespace Mood {

class OpenGLSSBO {
public:
    OpenGLSSBO();
    ~OpenGLSSBO();

    OpenGLSSBO(const OpenGLSSBO&) = delete;
    OpenGLSSBO& operator=(const OpenGLSSBO&) = delete;

    /// @brief Sube `bytes` bytes desde `data`. Si `bytes` <= la capacidad
    ///        actual, hace `glBufferSubData` (reusa storage). Si crece,
    ///        re-llama `glBufferData` con GL_DYNAMIC_DRAW. Llamar una
    ///        vez por frame es lo esperado.
    /// @note  Pasar `data=nullptr` con `bytes>0` reserva storage sin
    ///        inicializar (util cuando el shader es solo write).
    void upload(const void* data, GLsizeiptr bytes);

    /// @brief Bindea al `binding` index del layout
    ///        `layout(std430, binding=N) buffer X { ... };` del shader.
    void bind(GLuint binding) const;

    /// @brief GLuint raw (debug / fallback).
    GLuint glHandle() const { return m_id; }

    /// @brief Capacidad actual del storage en bytes (>= 0).
    GLsizeiptr capacityBytes() const { return m_capacity; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_capacity = 0;
};

} // namespace Mood
