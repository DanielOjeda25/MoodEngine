#include "engine/render/preview/AnimationPreviewRenderer.h"

#include "engine/animation/clips/AnimationClip.h"
#include "engine/animation/skeleton/Skeleton.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"

#include <glad/gl.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <string>

namespace Mood {

namespace {

struct PointLightStd430 {
    f32 posX, posY, posZ; f32 _pad0;
    f32 colR, colG, colB; f32 intensity;
    f32 radius; f32 _pad1, _pad2, _pad3;
};
static_assert(sizeof(PointLightStd430) == 48, "layout mismatch con el shader");

struct LightTileStd430 { u32 count; u32 offset; };

} // namespace

AnimationPreviewRenderer::AnimationPreviewRenderer(u32 size) : m_size(size) {
    if (m_size == 0) return;

    m_fb = std::make_unique<OpenGLFramebuffer>(
        m_size, m_size, OpenGLFramebuffer::Format::LDR);

    // Shader skinneado del motor (mismo par que SceneRenderer para skinned).
    m_skinnedShader = std::make_unique<OpenGLShader>(
        "shaders/pbr_skinned.vert", "shaders/pbr.frag");

    m_pointLightsSsbo  = std::make_unique<OpenGLSSBO>();
    m_lightTilesSsbo   = std::make_unique<OpenGLSSBO>();
    m_lightIndicesSsbo = std::make_unique<OpenGLSSBO>();

    PointLightStd430 dummyLight{};
    m_pointLightsSsbo->upload(&dummyLight, sizeof(dummyLight));
    LightTileStd430 emptyTile{0u, 0u};
    m_lightTilesSsbo->upload(&emptyTile, sizeof(emptyTile));
    const u32 zero = 0u;
    m_lightIndicesSsbo->upload(&zero, sizeof(u32));
}

AnimationPreviewRenderer::~AnimationPreviewRenderer() = default;

void AnimationPreviewRenderer::setIblTextures(OpenGLCubemapTexture* irradiance,
                                              OpenGLCubemapTexture* prefilter,
                                              ITexture* brdfLut) {
    m_iblIrradiance = irradiance;
    m_iblPrefilter  = prefilter;
    m_iblBrdfLut    = brdfLut;
}

GLuint AnimationPreviewRenderer::outputTextureId() const {
    return m_fb ? m_fb->glColorTextureId() : 0u;
}

void AnimationPreviewRenderer::bindInvariantState() {
    m_pointLightsSsbo->bind(2);
    m_lightTilesSsbo->bind(3);
    m_lightIndicesSsbo->bind(4);

    const bool iblOk = m_iblIrradiance && m_iblPrefilter && m_iblBrdfLut;
    f32 prefilterMaxLod = 0.0f;
    if (iblOk) {
        m_iblIrradiance->bind(4);
        m_iblPrefilter->bind(5);
        m_iblBrdfLut->bind(6);
        prefilterMaxLod = static_cast<f32>(m_iblPrefilter->mipLevels() - 1);
    }

    m_skinnedShader->setVec3("uAmbient", glm::vec3(0.20f));
    m_skinnedShader->setInt("uAlbedoMap",         0);
    m_skinnedShader->setInt("uShadowMap",         1);
    m_skinnedShader->setInt("uMetallicRoughness", 2);
    m_skinnedShader->setInt("uAoMap",             3);
    m_skinnedShader->setInt("uIrradianceMap",     4);
    m_skinnedShader->setInt("uPrefilterMap",      5);
    m_skinnedShader->setInt("uBrdfLut",           6);

    const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -0.6f, -0.7f));
    m_skinnedShader->setVec3("uDirectional.direction", lightDir);
    m_skinnedShader->setVec3("uDirectional.color",     glm::vec3(1.0f));
    m_skinnedShader->setFloat("uDirectional.intensity", 3.0f);
    m_skinnedShader->setInt  ("uDirectional.enabled",   1);

    m_skinnedShader->setInt("uTileSize", 16);
    m_skinnedShader->setInt("uTilesX",   1);
    m_skinnedShader->setInt("uTilesY",   1);

    m_skinnedShader->setInt  ("uIblEnabled",      iblOk ? 1 : 0);
    m_skinnedShader->setFloat("uPrefilterMaxLod", prefilterMaxLod);
    m_skinnedShader->setFloat("uIblIntensity",    1.0f);

    m_skinnedShader->setInt  ("uShadowEnabled", 0);
    m_skinnedShader->setFloat("uShadowBias",    0.005f);
    m_skinnedShader->setMat4 ("uLightSpace",    glm::mat4(1.0f));

    m_skinnedShader->setInt  ("uFogMode",    0);
    m_skinnedShader->setVec3 ("uFogColor",   glm::vec3(0.0f));
    m_skinnedShader->setFloat("uFogDensity", 0.0f);
    m_skinnedShader->setFloat("uFogStart",   0.0f);
    m_skinnedShader->setFloat("uFogEnd",     0.0f);

    m_skinnedShader->setInt  ("uBlendMode",          0);
    m_skinnedShader->setFloat("uOpacity",            1.0f);
    m_skinnedShader->setFloat("uIor",                1.0f);
    m_skinnedShader->setFloat("uRefractionStrength", 0.0f);
    m_skinnedShader->setInt  ("uBackbufferCopy",     7);
    m_skinnedShader->setVec2 ("uScreenSize",
                              glm::vec2(static_cast<f32>(m_size), static_cast<f32>(m_size)));
}

void AnimationPreviewRenderer::bindMaterial(const MaterialAsset* mat,
                                            AssetManager& assets) {
    ITexture* dummyTex = assets.getTexture(assets.missingTextureId());

    const bool hasAlbedo = mat && mat->useAlbedoMap;
    glActiveTexture(GL_TEXTURE0);
    assets.getTexture((hasAlbedo && mat) ? mat->albedo : 0u)->bind(0);
    m_skinnedShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

    glActiveTexture(GL_TEXTURE1);
    if (dummyTex) dummyTex->bind(1);

    const bool hasMR = mat && (mat->metallicRoughness != 0);
    glActiveTexture(GL_TEXTURE2);
    if (hasMR) assets.getTexture(mat->metallicRoughness)->bind(2);
    else if (dummyTex) dummyTex->bind(2);
    m_skinnedShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

    const bool hasAo = mat && (mat->ao != 0);
    glActiveTexture(GL_TEXTURE3);
    if (hasAo) assets.getTexture(mat->ao)->bind(3);
    else if (dummyTex) dummyTex->bind(3);
    m_skinnedShader->setInt("uHasAoMap", hasAo ? 1 : 0);

    m_skinnedShader->setVec3 ("uAlbedoTint",    mat ? mat->albedoTint    : glm::vec3(0.8f));
    m_skinnedShader->setFloat("uMetallicMult",  mat ? mat->metallicMult  : 0.0f);
    m_skinnedShader->setFloat("uRoughnessMult", mat ? mat->roughnessMult : 0.6f);
    m_skinnedShader->setFloat("uAoMult",        mat ? mat->aoMult        : 1.0f);

    glActiveTexture(GL_TEXTURE0);
}

GLuint AnimationPreviewRenderer::renderClip(u32 meshId, u32 clipId, f32 timeSec,
                                            AssetManager& assets) {
    if (!m_fb || !m_skinnedShader) return 0u;
    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    m_fb->bind();
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
    const bool ok = renderPoseToBoundFbo(meshId, clipId, timeSec, assets);
    m_fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    return ok ? m_fb->glColorTextureId() : 0u;
}

GLuint AnimationPreviewRenderer::staticThumbnail(u32 meshId, u32 clipId,
                                                 AssetManager& assets) {
    if (!m_skinnedShader || m_size == 0) return 0u;

    if (auto it = m_staticCache.find(clipId); it != m_staticCache.end()) {
        return it->second ? it->second->glColorTextureId() : 0u;
    }

    // Pose representativa: ~33% de la duración (evita el primer frame que suele
    // ser bind/T-pose).
    AnimationClip* clip = assets.getAnimationClip(clipId);
    const f32 t = (clip && clip->duration > 1e-4f) ? clip->duration * 0.33f : 0.0f;

    auto fb = std::make_unique<OpenGLFramebuffer>(
        m_size, m_size, OpenGLFramebuffer::Format::LDR);
    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    fb->bind();
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
    renderPoseToBoundFbo(meshId, clipId, t, assets);
    fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    const GLuint tex = fb->glColorTextureId();
    m_staticCache.emplace(clipId, std::move(fb));
    return tex;
}

bool AnimationPreviewRenderer::renderPoseToBoundFbo(u32 meshId, u32 clipId,
                                                    f32 timeSec, AssetManager& assets) {
    // Limpiar siempre (si el mesh/clip no sirven, queda el fondo neutro).
    glClearColor(0.16f, 0.16f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    MeshAsset* mesh = assets.getMesh(meshId);
    if (mesh == nullptr || !mesh->hasSkeleton() || mesh->submeshes.empty()) return false;
    const Skeleton& skeleton = *mesh->skeleton;

    AnimationClip* clip = assets.getAnimationClip(clipId);
    if (clip == nullptr || clip->tracks.empty()) return false;

    // Remap clip→esqueleto cacheado por clipId.
    auto it = m_remapCache.find(clipId);
    if (it == m_remapCache.end()) {
        std::vector<int> remap;
        bindClipToSkeleton(*clip, skeleton, remap);
        it = m_remapCache.emplace(clipId, std::move(remap)).first;
    }

    // Pose → skinning matrices (loop a duración si corresponde).
    f32 t = timeSec;
    if (clip->duration > 1e-4f) t = std::fmod(timeSec, clip->duration);
    evaluateClipWithRemap(*clip, t, skeleton, it->second, m_localPose);
    computeSkinningMatrices(skeleton, m_localPose, m_skinning);

    // ---- Modelo (rotación de import para que el NPC salga derecho) ----
    const glm::vec3 e = glm::radians(mesh->importRotationEuler);
    glm::mat4 model(1.0f);
    model = glm::rotate(model, e.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, e.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, e.z, glm::vec3(0.0f, 0.0f, 1.0f));

    // ---- Cámara encuadrada al AABB del bind pose (margen para las extremidades
    // que la animación puede sacar fuera del bind) ----
    const glm::vec3 centerLocal = (mesh->aabbMin + mesh->aabbMax) * 0.5f;
    const glm::vec3 target = glm::vec3(model * glm::vec4(centerLocal, 1.0f));
    f32 radius = glm::length(mesh->aabbMax - mesh->aabbMin) * 0.5f;
    if (radius < 1e-3f) radius = 1.0f;
    radius *= 1.25f;  // margen para brazos/piernas en movimiento

    constexpr f32 kFovDeg = 32.0f;
    const f32 dist = (radius / std::sin(glm::radians(kFovDeg * 0.5f))) * 1.1f;
    // Vista frontal-3/4 ligeramente elevada (los personajes se leen de frente).
    const glm::vec3 dir = glm::normalize(glm::vec3(0.25f, 0.15f, 1.0f));
    const glm::vec3 camPos = target + dir * dist;
    const glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(
        glm::radians(kFovDeg), 1.0f, std::max(0.01f, dist - radius * 2.0f),
        dist + radius * 2.0f + 1.0f);

    // ---- Uniforms ----
    m_skinnedShader->bind();
    m_skinnedShader->setMat4("uView", view);
    m_skinnedShader->setMat4("uProjection", projection);
    m_skinnedShader->setMat4("uModel", model);
    m_skinnedShader->setVec3("uCameraPos", camPos);
    bindInvariantState();

    // Bone matrices (clamp a MAX_BONES del shader).
    const usize count = std::min<usize>(m_skinning.size(), k_maxBonesPerSkeleton);
    for (usize i = 0; i < count; ++i) {
        m_skinnedShader->setMat4("uBoneMatrices[" + std::to_string(i) + "]",
                                 m_skinning[i]);
    }

    // ---- Draw por submesh con su material ----
    const std::vector<MaterialAssetId> matIds = assets.createMaterialsForMesh(meshId);
    for (const SubMesh& sub : mesh->submeshes) {
        IMesh* m = sub.mesh.get();
        if (m == nullptr) continue;
        const MaterialAsset* mat = nullptr;
        if (!matIds.empty()) {
            const usize idx = std::min<usize>(sub.materialIndex, matIds.size() - 1);
            mat = assets.getMaterial(matIds[idx]);
        }
        bindMaterial(mat, assets);
        m->bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m->vertexCount()));
    }

    return true;
}

} // namespace Mood
