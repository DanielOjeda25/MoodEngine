#include "systems/PostProcessPass.h"

#include "core/Log.h"
#include "engine/render/IShader.h"
#include "engine/render/opengl/OpenGLFramebuffer.h"
#include "engine/render/opengl/OpenGLShader.h"

namespace Mood {

PostProcessPass::PostProcessPass() {
    m_shader = std::make_unique<OpenGLShader>(
        "shaders/post_process.vert", "shaders/post_process.frag");

    // VAO vacio: el vertex shader produce posiciones desde gl_VertexID,
    // sin VBO. Pero OpenGL Core Profile requiere un VAO bound para que
    // glDrawArrays no se queje, asi que mantenemos uno trivial.
    glGenVertexArrays(1, &m_vao);

    Log::render()->info("PostProcessPass inicializado");
}

PostProcessPass::~PostProcessPass() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

void PostProcessPass::apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
                             f32 exposure, TonemapMode tonemap) {
    dst.bind(); // setea viewport al tamano de dst

    // El pase no necesita depth test ni blend (sobreescribe todos los
    // pixeles). Guardamos el state para restaurarlo despues.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);

    m_shader->bind();
    m_shader->setInt  ("uHdrTex",   0);
    m_shader->setFloat("uExposure", exposure);
    m_shader->setInt  ("uTonemap",  static_cast<i32>(tonemap));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.glColorTextureId());

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (depthEnabled) glEnable(GL_DEPTH_TEST);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
}

} // namespace Mood
