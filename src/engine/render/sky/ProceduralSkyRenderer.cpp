#include "engine/render/sky/ProceduralSkyRenderer.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/sky/HosekWilkie.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>

namespace Mood {

namespace {

// 6 view matrices del cubemap capture, una por face (positive X, neg X,
// pos Y, neg Y, pos Z, neg Z). Convencion estandar OpenGL cubemap.
// Up vectors elegidos para que las orientaciones queden consistentes con
// el sampling de samplerCube en los fragment shaders.
const std::array<glm::mat4, 6> k_faceViews = {
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),  // +X
    glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),  // -X
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),  // +Y
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),  // -Y
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),  // +Z
    glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),  // -Z
};

// Projection square 90° (cubemap face fov).
const glm::mat4 k_captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

} // namespace

ProceduralSkyRenderer::ProceduralSkyRenderer() {
    initCubemap();
    initFbo();

    try {
        m_shader = std::make_unique<OpenGLShader>(
            "shaders/procedural_sky.vert", "shaders/procedural_sky.frag");
    } catch (const std::exception& e) {
        Log::render()->error("ProceduralSkyRenderer: shader load failed: {}", e.what());
        m_shader.reset();
    }

    // VAO vacio para draws sin vertex buffer (gl_VertexID en el .vert).
    glGenVertexArrays(1, &m_emptyVao);

    Log::render()->info(
        "ProceduralSkyRenderer inicializado (cubemap {}x{}x6 RGBA16F)",
        k_cubemapSize, k_cubemapSize);
}

ProceduralSkyRenderer::~ProceduralSkyRenderer() {
    if (m_fbo != 0) glDeleteFramebuffers(1, &m_fbo);
    if (m_cubemap != 0) glDeleteTextures(1, &m_cubemap);
    if (m_emptyVao != 0) glDeleteVertexArrays(1, &m_emptyVao);
}

void ProceduralSkyRenderer::initCubemap() {
    glGenTextures(1, &m_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemap);

    for (int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F,
                     k_cubemapSize, k_cubemapSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void ProceduralSkyRenderer::initFbo() {
    glGenFramebuffers(1, &m_fbo);
    // El attachment se setea dinamicamente por face en render(), no aca.
}

bool ProceduralSkyRenderer::render(const glm::vec3& sunDirection,
                                    float turbidity,
                                    const glm::vec3& groundAlbedo) {
    if (m_shader == nullptr) {
        return false;
    }

    // Computar coeficientes Hosek-Wilkie CPU-side (1 vez para los 6 faces).
    const Sky::HosekCoefficients coefs = Sky::computeCoefficients(
        sunDirection, turbidity, groundAlbedo, 1.15f);

    // Guardar estado GL que vamos a modificar.
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, k_cubemapSize, k_cubemapSize);

    m_shader->bind();
    m_shader->setVec3("uA", coefs.A);
    m_shader->setVec3("uB", coefs.B);
    m_shader->setVec3("uC", coefs.C);
    m_shader->setVec3("uD", coefs.D);
    m_shader->setVec3("uE", coefs.E);
    m_shader->setVec3("uF", coefs.F);
    m_shader->setVec3("uG", coefs.G);
    m_shader->setVec3("uH", coefs.H);
    m_shader->setVec3("uI", coefs.I);
    m_shader->setVec3("uZ", coefs.Z);
    m_shader->setVec3("uSunDirection", sunDirection);

    glBindVertexArray(m_emptyVao);

    for (int face = 0; face < 6; ++face) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                m_cubemap, 0);

        const glm::mat4 viewProj = k_captureProj * k_faceViews[face];
        const glm::mat4 invViewProj = glm::inverse(viewProj);
        m_shader->setMat4("uInvViewProjection", invViewProj);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);

    return true;
}

} // namespace Mood
