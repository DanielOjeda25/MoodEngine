#include "systems/render/ColorGradingPass.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"

#include <algorithm>

namespace Mood {

ColorGradingPass::ColorGradingPass() {
    // Reusamos `bloom.vert` que ya tiene el fullscreen triangle desde
    // gl_VertexID -- mismo patron que SSAOPass / BloomPass. Si en el
    // futuro emerge necesidad de un vert custom (eje agregado al UV),
    // se split a un .vert propio.
    m_shader = std::make_unique<OpenGLShader>(
        "shaders/bloom.vert", "shaders/color_grading.frag");

    glGenVertexArrays(1, &m_vao);

    Log::render()->info("ColorGradingPass inicializado");
}

ColorGradingPass::~ColorGradingPass() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

bool ColorGradingPass::apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
                              GLuint lutTextureId, f32 intensity) {
    if (lutTextureId == 0) return false;
    if (intensity <= 0.0f) return false;  // sin contribucion
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    // Guardar GL state que vamos a tocar.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (blendEnabled) glDisable(GL_BLEND);

    dst.bind();  // setea viewport al tamano de dst

    m_shader->bind();
    m_shader->setInt("uHdrTex", 0);
    m_shader->setInt("uLutTex", 1);
    m_shader->setFloat("uIntensity", intensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.glColorTextureId());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lutTextureId);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Reset active texture al slot 0 para no contaminar passes siguientes.
    glActiveTexture(GL_TEXTURE0);

    // Restaurar state.
    if (blendEnabled) glEnable(GL_BLEND);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
    return true;
}

} // namespace Mood
