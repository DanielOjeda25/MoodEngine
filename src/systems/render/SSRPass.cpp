#include "systems/render/SSRPass.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"

#include <algorithm>

namespace Mood {

SSRPass::SSRPass() {
    // Reusamos `ssao.vert` (fullscreen triangle generado desde gl_VertexID)
    // -- mismo patron que SSAOPass / BloomPass / ColorGradingPass.
    m_shader = std::make_unique<OpenGLShader>(
        "shaders/ssao.vert", "shaders/ssr.frag");

    glGenVertexArrays(1, &m_vao);

    Log::render()->info("SSRPass inicializado");
}

SSRPass::~SSRPass() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

bool SSRPass::apply(OpenGLFramebuffer& srcColor,
                     OpenGLFramebuffer& srcGbuffer,
                     OpenGLFramebuffer& dst,
                     const glm::mat4& proj, const glm::mat4& invProj,
                     i32 maxSteps, f32 thickness, f32 stepSize, f32 intensity) {
    if (intensity <= 0.0f) return false;
    // Sin normal RT no hay G-buffer para el ray marching.
    if (srcGbuffer.glNormalTextureId() == 0) return false;
    // Sin depth texture tampoco. SSAO ya requeria esto -- en HDR siempre
    // hay depth texture (post-F2H56).
    if (srcGbuffer.glDepthTextureId() == 0) return false;

    intensity = std::clamp(intensity, 0.0f, 1.0f);
    maxSteps  = std::clamp(maxSteps, 1, 256);
    thickness = std::clamp(thickness, 0.001f, 5.0f);
    stepSize  = std::clamp(stepSize, 0.001f, 5.0f);

    // Guardar GL state que vamos a tocar.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (blendEnabled) glDisable(GL_BLEND);

    dst.bind();  // setea viewport al tamano de dst

    m_shader->bind();
    m_shader->setInt("uSceneColor", 0);
    m_shader->setInt("uSceneDepth", 1);
    m_shader->setInt("uSceneNormal", 2);
    m_shader->setMat4("uProj", proj);
    m_shader->setMat4("uInvProj", invProj);
    m_shader->setFloat("uIntensity", intensity);
    m_shader->setInt("uMaxSteps", maxSteps);
    m_shader->setFloat("uThickness", thickness);
    m_shader->setFloat("uStepSize", stepSize);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcColor.glColorTextureId());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, srcGbuffer.glDepthTextureId());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, srcGbuffer.glNormalTextureId());

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
