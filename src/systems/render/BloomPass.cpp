#include "systems/render/BloomPass.h"

#include "core/Log.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"

#include <glm/vec2.hpp>

#include <algorithm>

namespace Mood {

BloomPass::BloomPass() {
    m_downsampleShader = std::make_unique<OpenGLShader>(
        "shaders/bloom.vert", "shaders/bloom_downsample.frag");
    m_upsampleShader = std::make_unique<OpenGLShader>(
        "shaders/bloom.vert", "shaders/bloom_upsample.frag");
    m_compositeShader = std::make_unique<OpenGLShader>(
        "shaders/bloom.vert", "shaders/bloom_composite.frag");

    // VAO trivial: el vertex shader genera posiciones desde gl_VertexID.
    // OpenGL Core Profile exige un VAO bound para glDrawArrays.
    glGenVertexArrays(1, &m_vao);

    Log::render()->info("BloomPass inicializado ({} mips)", k_mipCount);
}

BloomPass::~BloomPass() {
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

void BloomPass::resize(u32 width, u32 height) {
    if (width == m_currentWidth && height == m_currentHeight) {
        return; // sin cambios
    }
    if (width == 0 || height == 0) {
        return; // viewport invalido (ventana minimizada o panel cerrado)
    }

    m_currentWidth = width;
    m_currentHeight = height;

    u32 w = width;
    u32 h = height;
    for (u32 i = 0; i < k_mipCount; ++i) {
        w = std::max(w / 2u, 1u);
        h = std::max(h / 2u, 1u);
        m_mips[i] = std::make_unique<OpenGLFramebuffer>(
            w, h, OpenGLFramebuffer::Format::HDR);
    }

    Log::render()->info("BloomPass resize a {}x{} (mip 0 = {}x{}, mip {} = {}x{})",
                        width, height,
                        m_mips[0]->width(), m_mips[0]->height(),
                        k_mipCount - 1,
                        m_mips[k_mipCount - 1]->width(),
                        m_mips[k_mipCount - 1]->height());
}

void BloomPass::downsamplePass(GLuint srcTextureId, u32 srcWidth, u32 srcHeight,
                                OpenGLFramebuffer& dst, bool isFirstPass,
                                f32 threshold) {
    dst.bind(); // setea viewport al tamano de dst

    m_downsampleShader->bind();
    m_downsampleShader->setInt("uSrcTex", 0);
    m_downsampleShader->setVec2("uSrcTexelSize",
        glm::vec2(1.0f / static_cast<f32>(srcWidth),
                  1.0f / static_cast<f32>(srcHeight)));
    m_downsampleShader->setInt("uIsFirstPass", isFirstPass ? 1 : 0);
    m_downsampleShader->setFloat("uThreshold", threshold);
    m_downsampleShader->setFloat("uSoftKnee", 0.5f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTextureId);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void BloomPass::upsamplePass(GLuint srcTextureId, u32 srcWidth, u32 srcHeight,
                              OpenGLFramebuffer& dst, f32 radius) {
    dst.bind();

    m_upsampleShader->bind();
    m_upsampleShader->setInt("uSrcTex", 0);
    m_upsampleShader->setVec2("uSrcTexelSize",
        glm::vec2(1.0f / static_cast<f32>(srcWidth),
                  1.0f / static_cast<f32>(srcHeight)));
    m_upsampleShader->setFloat("uRadius", radius);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTextureId);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void BloomPass::compositePass(GLuint srcHdrId, GLuint bloomTexId,
                               OpenGLFramebuffer& dst, f32 intensity) {
    dst.bind();

    m_compositeShader->bind();
    m_compositeShader->setInt("uHdrTex", 0);
    m_compositeShader->setInt("uBloomTex", 1);
    m_compositeShader->setFloat("uBloomIntensity", intensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcHdrId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexId);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Reset active texture al slot 0 para no contaminar passes siguientes.
    glActiveTexture(GL_TEXTURE0);
}

bool BloomPass::apply(OpenGLFramebuffer& src, OpenGLFramebuffer& dst,
                      f32 threshold, f32 intensity, f32 radius) {
    // Resize idempotente: crea el mip chain en el primer apply() y
    // recrea solo si el src cambio de size. Si el src tiene size 0
    // (panel cerrado o minimizado) resize no construye nada.
    resize(src.width(), src.height());

    if (m_mips[0] == nullptr) {
        // Size invalido o creacion fallida. El caller debe usar el src
        // directo para el post-process en lugar del dst (que no se
        // sobreescribio).
        return false;
    }

    // Guardamos state que vamos a tocar.
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled)  glDisable(GL_CULL_FACE);
    if (blendEnabled) glDisable(GL_BLEND);

    // -------- Downsample chain --------
    // src -> mip[0] (con threshold + Karis)
    downsamplePass(src.glColorTextureId(), src.width(), src.height(),
                   *m_mips[0], /*isFirstPass=*/true, threshold);
    // mip[0] -> mip[1] -> ... -> mip[k_mipCount-1] (sin threshold)
    for (u32 i = 1; i < k_mipCount; ++i) {
        downsamplePass(m_mips[i - 1]->glColorTextureId(),
                       m_mips[i - 1]->width(), m_mips[i - 1]->height(),
                       *m_mips[i], /*isFirstPass=*/false, threshold);
    }

    // -------- Upsample chain con blending aditivo --------
    // mip[5] tent -> sumar a mip[4]. mip[4] tent -> sumar a mip[3]. Etc.
    // Hasta sumar a mip[0]. El blend ADD apila las contribuciones encima
    // del downsample value que ya tiene cada mip.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    for (i32 i = static_cast<i32>(k_mipCount) - 1; i > 0; --i) {
        upsamplePass(m_mips[i]->glColorTextureId(),
                     m_mips[i]->width(), m_mips[i]->height(),
                     *m_mips[i - 1], radius);
    }

    glDisable(GL_BLEND);

    // -------- Composite final: src + bloom(mip[0]) -> dst --------
    compositePass(src.glColorTextureId(), m_mips[0]->glColorTextureId(),
                  dst, intensity);

    // Restaurar state.
    if (blendEnabled) glEnable(GL_BLEND);
    if (cullEnabled)  glEnable(GL_CULL_FACE);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
    return true;
}

} // namespace Mood
