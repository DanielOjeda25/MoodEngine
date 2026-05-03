#pragma once

// Interfaz abstracta del renderer. El resto del motor y del editor hablan
// contra esta API, sin saber de OpenGL ni de Vulkan. Permite futuros backends
// sin refactorizar el resto del motor.

#include "engine/render/rhi/IFramebuffer.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/IShader.h"
#include "engine/render/rhi/RendererTypes.h"
#include "core/Types.h"

namespace Mood {

/// @brief F2H2: contadores per-frame para el Performance HUD. Solo cuentan
///        las draws que pasan por `IRenderer::drawMesh` (cuerpo opaco PBR +
///        skinned + shadow). Pases con calls glDraw* directos (skybox = 1,
///        particles = 1 instanced, debug = 1-2) NO se contabilizan.
struct FrameStats {
    u32 drawCalls = 0;
    u32 triangles = 0;
};

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

    /// @brief F2H4: dibuja N instancias de la misma malla en un solo
    ///        draw call. Las matrices `model` viajan como atributo de
    ///        instancia (locations 5-8 = mat4) en `instanceData`. El
    ///        caller es duenio de subir esos datos a un buffer GL antes
    ///        de llamar (ver `OpenGLInstanceBuffer`). El shader debe
    ///        leer `aModel` desde locations 5-8 con divisor=1.
    ///
    ///        Counter `frameStats().drawCalls` se incrementa en 1 (es
    ///        un solo draw call); `frameStats().triangles` se incrementa
    ///        en `instanceCount * (vertexCount/3)`.
    virtual void drawMeshInstanced(const IMesh& mesh, const IShader& shader,
                                     u32 instanceCount) = 0;

    /// @brief F2H2: contadores acumulados durante el frame actual. Reset en
    ///        cada `beginFrame`. Util para el Performance HUD del editor.
    virtual FrameStats frameStats() const = 0;
};

} // namespace Mood
