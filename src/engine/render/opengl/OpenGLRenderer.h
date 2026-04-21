#pragma once

// Implementacion OpenGL de IRenderer. Mantenerla fina: la idea es que la API
// esconda OpenGL del resto del motor. Estado complejo (Z-buffer, blending,
// culling) se va agregando cuando los hitos lo pidan.

#include "engine/render/IRenderer.h"

namespace Mood {

class OpenGLRenderer final : public IRenderer {
public:
    void init() override;
    void beginFrame(const ClearValues& clear) override;
    void endFrame() override;
    void setViewport(i32 x, i32 y, u32 width, u32 height) override;
    void drawMesh(const IMesh& mesh, const IShader& shader) override;
};

} // namespace Mood
