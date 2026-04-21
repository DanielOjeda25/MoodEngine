#pragma once

// Interfaz abstracta del renderer. El resto del motor y del editor hablan
// contra esta API, sin saber de OpenGL ni de Vulkan. Permite futuros backends
// sin refactorizar el resto del motor.

#include "engine/render/IFramebuffer.h"
#include "engine/render/IMesh.h"
#include "engine/render/IShader.h"
#include "engine/render/RendererTypes.h"
#include "core/Types.h"

namespace Mood {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// @brief Llamar una sola vez tras crear el contexto grafico.
    virtual void init() = 0;

    /// @brief Prepara el frame (clear del framebuffer actualmente bindeado).
    virtual void beginFrame(const ClearValues& clear) = 0;

    /// @brief Finaliza el frame actual.
    virtual void endFrame() = 0;

    /// @brief Define el viewport del framebuffer actualmente bindeado.
    virtual void setViewport(i32 x, i32 y, u32 width, u32 height) = 0;

    /// @brief Dibuja una malla con el shader indicado. Ambos deben estar
    ///        listos (VAO creado, program linkado).
    virtual void drawMesh(const IMesh& mesh, const IShader& shader) = 0;
};

} // namespace Mood
