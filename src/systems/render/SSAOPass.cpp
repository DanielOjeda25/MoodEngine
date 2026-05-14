#include "systems/render/SSAOPass.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>

#include <algorithm>

namespace Mood {

SSAOPass::SSAOPass() {
    m_ssaoShader = std::make_unique<OpenGLShader>(
        "shaders/ssao.vert", "shaders/ssao.frag");
    m_blurShader = std::make_unique<OpenGLShader>(
        "shaders/ssao.vert", "shaders/ssao_blur.frag");
    m_compositeShader = std::make_unique<OpenGLShader>(
        "shaders/ssao.vert", "shaders/ssao_composite.frag");

    glGenVertexArrays(1, &m_vao);

    Log::render()->info("SSAOPass inicializado");
}

SSAOPass::~SSAOPass() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

void SSAOPass::resize(u32 width, u32 height) {
    if (width == m_currentWidth && height == m_currentHeight) return;
    if (width == 0 || height == 0) return;

    m_currentWidth = width;
    m_currentHeight = height;

    // FBs half-res (mitad de ancho/alto) en LDR (RGBA8 wraping un canal).
    // Half-res es la convencion industria para SSAO -- compensa el costo
    // de los 16 samples por pixel sin perdida visual notable.
    u32 halfW = std::max(width  / 2u, 1u);
    u32 halfH = std::max(height / 2u, 1u);
    m_aoRawFb = std::make_unique<OpenGLFramebuffer>(
        halfW, halfH, OpenGLFramebuffer::Format::LDR);
    m_aoBlurFb = std::make_unique<OpenGLFramebuffer>(
        halfW, halfH, OpenGLFramebuffer::Format::LDR);

    Log::render()->info("SSAOPass resize a {}x{} (AO FBs a {}x{} half-res)",
                        width, height, halfW, halfH);
}

void SSAOPass::ssaoPass(GLuint depthTexId, const glm::mat4& proj,
                         const glm::mat4& invProj, f32 radius, f32 intensity) {
    m_aoRawFb->bind();

    m_ssaoShader->bind();
    m_ssaoShader->setInt("uDepthTex", 0);
    m_ssaoShader->setMat4("uProj", proj);
    m_ssaoShader->setMat4("uInvProj", invProj);
    m_ssaoShader->setVec2("uScreenSize",
        glm::vec2(static_cast<f32>(m_aoRawFb->width()),
                  static_cast<f32>(m_aoRawFb->height())));
    m_ssaoShader->setFloat("uRadius", radius);
    m_ssaoShader->setFloat("uIntensity", intensity);
    m_ssaoShader->setFloat("uBias", 0.025f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexId);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void SSAOPass::blurPass() {
    m_aoBlurFb->bind();

    m_blurShader->bind();
    m_blurShader->setInt("uAoTex", 0);
    m_blurShader->setVec2("uTexelSize",
        glm::vec2(1.0f / static_cast<f32>(m_aoRawFb->width()),
                  1.0f / static_cast<f32>(m_aoRawFb->height())));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_aoRawFb->glColorTextureId());

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void SSAOPass::compositePass(GLuint srcHdrId, OpenGLFramebuffer& dst) {
    dst.bind();

    m_compositeShader->bind();
    m_compositeShader->setInt("uHdrTex", 0);
    m_compositeShader->setInt("uAoTex", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcHdrId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_aoBlurFb->glColorTextureId());

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}

bool SSAOPass::apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
                     const glm::mat4& proj, const glm::mat4& invProj,
                     f32 radius, f32 intensity) {
    resize(src.width(), src.height());
    if (m_aoRawFb == nullptr || m_aoBlurFb == nullptr) return false;

    // Solo modo HDR del src tiene depth texture sampleable.
    const GLuint depthTexId = src.glDepthTextureId();
    if (depthTexId == 0) {
        Log::render()->warn("SSAOPass::apply: src no es HDR (sin depth texture) -- skip");
        return false;
    }

    // Save GL state.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (blendEnabled) glDisable(GL_BLEND);

    ssaoPass(depthTexId, proj, invProj, radius, intensity);
    blurPass();
    compositePass(src.glColorTextureId(), dst);

    if (blendEnabled) glEnable(GL_BLEND);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
    return true;
}

} // namespace Mood
