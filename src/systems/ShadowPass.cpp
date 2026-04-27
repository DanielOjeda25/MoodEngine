#include "systems/ShadowPass.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/IMesh.h"
#include "engine/render/IRenderer.h"
#include "engine/render/MeshAsset.h"
#include "engine/render/ShadowMath.h"
#include "engine/render/opengl/OpenGLShader.h"
#include "engine/render/opengl/OpenGLShadowMap.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glad/gl.h>

namespace Mood {

ShadowPass::ShadowPass(u32 shadowMapSize) {
    m_shadowMap = std::make_unique<OpenGLShadowMap>(shadowMapSize);
    m_shader = std::make_unique<OpenGLShader>(
        "shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
    Log::render()->info("ShadowPass inicializado (shader compilado)");
}

ShadowPass::~ShadowPass() = default;

u32 ShadowPass::shadowMapSize() const {
    return m_shadowMap ? m_shadowMap->size() : 0;
}

void ShadowPass::bindShadowMap(u32 textureUnit) const {
    if (!m_shadowMap) return;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_shadowMap->glDepthTextureId());
}

void ShadowPass::record(Scene& scene,
                        AssetManager& assets,
                        IRenderer& renderer,
                        const glm::vec3& lightDir,
                        const glm::vec3& sceneCenter,
                        f32 sceneRadius) {
    if (!m_shadowMap || !m_shader) return;

    // Matrices via helper puro (testeable sin GL).
    const ShadowMatrices sm = computeShadowMatrices(lightDir, sceneCenter,
                                                      sceneRadius);
    m_lightSpace = sm.lightSpace;

    // Render al shadow map.
    m_shadowMap->bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    // Front-face culling durante el shadow pass: las caras frontales (las
    // que dan a la luz) no escriben depth — solo las traseras. Esto
    // mitiga peter-panning + shadow acne sin necesidad de bias agresivo.
    GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    GLint prevCullFace = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFace);
    if (!wasCullEnabled) glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    m_shader->bind();
    m_shader->setMat4("uLightSpace", m_lightSpace);

    scene.forEach<TransformComponent, MeshRendererComponent>(
        [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
            MeshAsset* asset = assets.getMesh(mr.mesh);
            if (asset == nullptr) return;
            m_shader->setMat4("uModel", t.worldMatrix());
            for (const auto& sub : asset->submeshes) {
                if (sub.mesh == nullptr) continue;
                renderer.drawMesh(*sub.mesh, *m_shader);
            }
        });

    // Restaurar culling state.
    glCullFace(static_cast<GLenum>(prevCullFace));
    if (!wasCullEnabled) glDisable(GL_CULL_FACE);

    m_shadowMap->unbind();
}

} // namespace Mood
