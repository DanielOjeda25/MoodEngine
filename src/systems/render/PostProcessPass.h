#pragma once

// PostProcessPass (Hito 15 Bloque 3): toma un framebuffer HDR (RGBA16F)
// y lo presenta a un framebuffer LDR (RGBA8) aplicando exposicion +
// tonemap + gamma. El segundo es el que ImGui muestra dentro del
// ViewportPanel.
//
// Diseno minimo: un shader (`shaders/post_process.{vert,frag}`) + un
// VAO vacio (el vertex shader genera el fullscreen triangle desde
// `gl_VertexID`, sin VBO). `apply` bindea el dst FB, lee el src color
// como sampler2D, y dibuja 3 vertices.

#include "core/Types.h"

#include <glad/gl.h>

#include <memory>

namespace Mood {

class IShader;
class OpenGLFramebuffer;

enum class TonemapMode : i32 {
    None     = 0,
    Reinhard = 1,
    ACES     = 2,
};

class PostProcessPass {
public:
    PostProcessPass();
    ~PostProcessPass();

    PostProcessPass(const PostProcessPass&) = delete;
    PostProcessPass& operator=(const PostProcessPass&) = delete;

    /// @brief Lee el color de `src` (HDR), aplica `exp2(exposure) * c` +
    ///        tonemap + gamma, y escribe a `dst` (LDR). El depth se ignora.
    /// @param src Framebuffer HDR (RGBA16F). Su `glColorTextureId` se
    ///            samplea como sampler2D.
    /// @param dst Framebuffer LDR (RGBA8). Es el que ImGui muestra.
    /// @param exposure En EVs (factor `2^exposure`). Default 0 = sin scale.
    /// @param tonemap 0=None (clamp), 1=Reinhard, 2=ACES.
    void apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
               f32 exposure, TonemapMode tonemap);

private:
    std::unique_ptr<IShader> m_shader;
    GLuint m_vao = 0; // VAO vacio para gl_VertexID-based fullscreen triangle
};

} // namespace Mood
