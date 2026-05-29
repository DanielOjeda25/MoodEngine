#include "engine/render/sky/IBLBaker.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLShader.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>

namespace Mood {

namespace {

// Mismas 6 view matrices que ProceduralSkyRenderer — captura del cubemap.
const std::array<glm::mat4, 6> k_faceViews = {
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
};

const glm::mat4 k_captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

} // namespace

IBLBaker::IBLBaker() {
    try {
        m_irradianceShader = std::make_unique<OpenGLShader>(
            "shaders/procedural_sky.vert", "shaders/ibl_irradiance.frag");
    } catch (const std::exception& e) {
        Log::render()->error("IBLBaker: irradiance shader failed: {}", e.what());
    }

    try {
        m_prefilterShader = std::make_unique<OpenGLShader>(
            "shaders/procedural_sky.vert", "shaders/ibl_prefilter.frag");
    } catch (const std::exception& e) {
        Log::render()->error("IBLBaker: prefilter shader failed: {}", e.what());
    }

    glGenFramebuffers(1, &m_fbo);
    initEmptyVao();

    Log::render()->info(
        "IBLBaker inicializado (irradiance {}x{}x6, prefilter {}x{}x6 mip{})",
        k_irradianceSize, k_irradianceSize,
        k_prefilterSize, k_prefilterSize, k_prefilterMips);
}

IBLBaker::~IBLBaker() {
    if (m_fbo != 0) glDeleteFramebuffers(1, &m_fbo);
    if (m_emptyVao != 0) glDeleteVertexArrays(1, &m_emptyVao);
}

void IBLBaker::initEmptyVao() {
    glGenVertexArrays(1, &m_emptyVao);
}

GLuint IBLBaker::createCubemap(int size, int mipLevels) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    for (int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                     mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    if (mipLevels > 1) {
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);  // alloca storage para todos los mips
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return tex;
}

IBLBaker::Result IBLBaker::bake(GLuint envCubemap) {
    if (m_irradianceShader == nullptr || m_prefilterShader == nullptr) {
        Log::render()->warn("IBLBaker::bake: shaders no disponibles, skip");
        return {};
    }

    // Guardar estado GL.
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);

    Result out;
    out.irradiance = createCubemap(k_irradianceSize, 1);
    out.prefilter  = createCubemap(k_prefilterSize, k_prefilterMips);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glBindVertexArray(m_emptyVao);

    // ---- Pass 1: irradiance (32x32x6, 1 mip) ----
    glViewport(0, 0, k_irradianceSize, k_irradianceSize);
    m_irradianceShader->bind();
    m_irradianceShader->setInt("uEnvCubemap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    for (int face = 0; face < 6; ++face) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                out.irradiance, 0);

        const glm::mat4 vp = k_captureProj * k_faceViews[face];
        m_irradianceShader->setMat4("uInvViewProjection", glm::inverse(vp));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // ---- Pass 2: prefilter (128x128x6, 5 mips, GGX importance sampling) ----
    m_prefilterShader->bind();
    m_prefilterShader->setInt("uEnvCubemap", 0);
    m_prefilterShader->setInt("uSampleCount", k_prefilterSamples);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    for (int mip = 0; mip < k_prefilterMips; ++mip) {
        const int mipSize = k_prefilterSize >> mip;  // 128, 64, 32, 16, 8
        const float roughness = float(mip) / float(k_prefilterMips - 1);
        glViewport(0, 0, mipSize, mipSize);
        m_prefilterShader->setFloat("uRoughness", roughness);

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                    out.prefilter, mip);

            const glm::mat4 vp = k_captureProj * k_faceViews[face];
            m_prefilterShader->setMat4("uInvViewProjection", glm::inverse(vp));

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);

    return out;
}

} // namespace Mood
