#pragma once

// Interfaz abstracta de una malla geometrica. La creacion concreta la hace
// cada backend (p.ej. OpenGLMesh) porque el layout de vertices y los buffers
// dependen de la API grafica.

#include "core/Types.h"

namespace Mood {

class IMesh {
public:
    virtual ~IMesh() = default;

    /// @brief Activa el mesh para ser dibujado (bind de VAO/VBO en OpenGL).
    virtual void bind() const = 0;

    /// @brief Desactiva el mesh tras dibujarlo.
    virtual void unbind() const = 0;

    /// @brief Cantidad de vertices que `drawMesh` debe consumir.
    virtual u32 vertexCount() const = 0;
};

} // namespace Mood
