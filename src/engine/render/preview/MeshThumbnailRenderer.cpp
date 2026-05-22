#include "engine/render/preview/MeshThumbnailRenderer.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/rhi/RendererTypes.h"
#include "engine/world/csg/BrushMesh.h"     // F2H80: preview de primitivas
#include "engine/world/csg/Primitives.h"

#include <glad/gl.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <algorithm>

namespace Mood {

namespace {

// Layouts std430 igual que SceneRenderer / MaterialPreviewRenderer — el shader
// Forward+ no reorderea, hay que matchear byte por byte.
struct PointLightStd430 {
    f32 posX, posY, posZ; f32 _pad0;
    f32 colR, colG, colB; f32 intensity;
    f32 radius; f32 _pad1, _pad2, _pad3;
};
static_assert(sizeof(PointLightStd430) == 48,
              "PointLightStd430 layout mismatch con el shader");

struct LightTileStd430 { u32 count; u32 offset; };

// Layout interleaved de los brushes (pos, color, uv, normal) = stride 11.
const std::vector<VertexAttribute> kBrushAttrs = {
    {0, 3}, {1, 3}, {2, 2}, {3, 3}
};

// Construye el Csg::Brush de una primitiva con las mismas dimensiones que el
// spawn real (ver EditorProjectActions_Brush.cpp). Plano/Quad/Cápsula son
// presets escalados de Box/Sphere.
Csg::Brush buildPrimitiveBrush(MeshThumbnailRenderer::PrimitiveKind kind) {
    using PK = MeshThumbnailRenderer::PrimitiveKind;
    const glm::mat4 I(1.0f);
    switch (kind) {
        case PK::Plane:
            return Csg::makeBoxBrush(glm::scale(I, glm::vec3(10.0f, 0.05f, 10.0f)));
        case PK::Quad:
            return Csg::makeBoxBrush(glm::scale(I, glm::vec3(1.0f, 0.05f, 1.0f)));
        case PK::Box:      return Csg::makeBoxBrush(I);
        case PK::Cylinder: return Csg::makeCylinderBrush(I);
        case PK::Sphere:   return Csg::makeSphereBrush(I);
        case PK::Cone:     return Csg::makeConeBrush(I);
        case PK::Capsule:
            return Csg::makeSphereBrush(glm::scale(I, glm::vec3(1.0f, 2.0f, 1.0f)));
        case PK::Pyramid:  return Csg::makePyramidBrush(I);
        case PK::Wedge:    return Csg::makeWedgeBrush(I);
        case PK::PrismTri: return Csg::makePrismBrush(I, 3);
        case PK::PrismHex: return Csg::makePrismBrush(I, 6);
    }
    return Csg::makeBoxBrush(I);
}

} // namespace

MeshThumbnailRenderer::MeshThumbnailRenderer(u32 size) : m_size(size) {
    if (m_size == 0) return;

    // Mismo shader PBR del motor (mismos attribs que cualquier mesh importado).
    m_pbrShader = std::make_unique<OpenGLShader>(
        "shaders/pbr.vert", "shaders/pbr.frag");

    // SSBOs no-vacíos (count=0) para el path Forward+ del shader: algunos
    // drivers fallan al bindear un buffer de tamaño 0.
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

MeshThumbnailRenderer::~MeshThumbnailRenderer() = default;

void MeshThumbnailRenderer::setIblTextures(OpenGLCubemapTexture* irradiance,
                                           OpenGLCubemapTexture* prefilter,
                                           ITexture* brdfLut) {
    m_iblIrradiance = irradiance;
    m_iblPrefilter  = prefilter;
    m_iblBrdfLut    = brdfLut;
}

void MeshThumbnailRenderer::clear() { m_cache.clear(); m_primCache.clear(); }

void MeshThumbnailRenderer::invalidate(u32 meshId) { m_cache.erase(meshId); }

// ============================================================================
// Setup PBR común
// ============================================================================

void MeshThumbnailRenderer::clearAndSetGlState() {
    glClearColor(0.16f, 0.16f, 0.18f, 1.0f);  // gris neutro
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void MeshThumbnailRenderer::setCamera(const glm::vec3& aabbMin,
                                      const glm::vec3& aabbMax,
                                      const glm::mat4& model) {
    // Bounding sphere (radio rotation-invariant; el centro lo mueve el model).
    const glm::vec3 centerLocal = (aabbMin + aabbMax) * 0.5f;
    const glm::vec3 target = glm::vec3(model * glm::vec4(centerLocal, 1.0f));
    f32 radius = glm::length(aabbMax - aabbMin) * 0.5f;
    if (radius < 1e-3f) radius = 0.5f;

    constexpr f32 kFovDeg = 30.0f;
    const f32 dist = (radius / std::sin(glm::radians(kFovDeg * 0.5f))) * 1.15f;
    const glm::vec3 dir = glm::normalize(glm::vec3(0.55f, 0.42f, 1.0f));  // 3/4 view
    const glm::vec3 camPos = target + dir * dist;

    const glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(
        glm::radians(kFovDeg), 1.0f, std::max(0.01f, dist - radius * 2.0f),
        dist + radius * 2.0f + 1.0f);

    m_pbrShader->setMat4("uView", view);
    m_pbrShader->setMat4("uProjection", projection);
    m_pbrShader->setMat4("uModel", model);
    m_pbrShader->setVec3("uCameraPos", camPos);
    m_pbrShader->setVec2("uScreenSize",
                         glm::vec2(static_cast<f32>(m_size), static_cast<f32>(m_size)));
}

void MeshThumbnailRenderer::bindInvariantState() {
    // SSBOs vacíos (Forward+).
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

    m_pbrShader->setVec3("uAmbient", glm::vec3(0.20f));
    m_pbrShader->setInt("uAlbedoMap",         0);
    m_pbrShader->setInt("uShadowMap",         1);
    m_pbrShader->setInt("uMetallicRoughness", 2);
    m_pbrShader->setInt("uAoMap",             3);
    m_pbrShader->setInt("uIrradianceMap",     4);
    m_pbrShader->setInt("uPrefilterMap",      5);
    m_pbrShader->setInt("uBrdfLut",           6);

    const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -0.6f, -0.7f));
    m_pbrShader->setVec3("uDirectional.direction", lightDir);
    m_pbrShader->setVec3("uDirectional.color",     glm::vec3(1.0f));
    m_pbrShader->setFloat("uDirectional.intensity", 3.0f);
    m_pbrShader->setInt  ("uDirectional.enabled",   1);

    m_pbrShader->setInt("uTileSize", 16);
    m_pbrShader->setInt("uTilesX",   1);
    m_pbrShader->setInt("uTilesY",   1);

    m_pbrShader->setInt  ("uIblEnabled",      iblOk ? 1 : 0);
    m_pbrShader->setFloat("uPrefilterMaxLod", prefilterMaxLod);
    m_pbrShader->setFloat("uIblIntensity",    1.0f);

    m_pbrShader->setInt  ("uShadowEnabled", 0);
    m_pbrShader->setFloat("uShadowBias",    0.005f);
    m_pbrShader->setMat4 ("uLightSpace",    glm::mat4(1.0f));

    m_pbrShader->setInt  ("uFogMode",    0);
    m_pbrShader->setVec3 ("uFogColor",   glm::vec3(0.0f));
    m_pbrShader->setFloat("uFogDensity", 0.0f);
    m_pbrShader->setFloat("uFogStart",   0.0f);
    m_pbrShader->setFloat("uFogEnd",     0.0f);

    // Sin transparencia en el thumbnail (igual criterio que el material preview).
    m_pbrShader->setInt  ("uBlendMode",          0);
    m_pbrShader->setFloat("uOpacity",            1.0f);
    m_pbrShader->setFloat("uIor",                1.0f);
    m_pbrShader->setFloat("uRefractionStrength", 0.0f);
    m_pbrShader->setInt  ("uBackbufferCopy",     7);
}

void MeshThumbnailRenderer::bindMaterial(const MaterialAsset* mat,
                                         AssetManager& assets) {
    ITexture* dummyTex = assets.getTexture(assets.missingTextureId());

    const bool hasAlbedo = mat && mat->useAlbedoMap;
    glActiveTexture(GL_TEXTURE0);
    assets.getTexture((hasAlbedo && mat) ? mat->albedo : 0u)->bind(0);
    m_pbrShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

    glActiveTexture(GL_TEXTURE1);
    if (dummyTex) dummyTex->bind(1);  // slot shadow (uShadowEnabled=0 igual)

    const bool hasMR = mat && (mat->metallicRoughness != 0);
    glActiveTexture(GL_TEXTURE2);
    if (hasMR) assets.getTexture(mat->metallicRoughness)->bind(2);
    else if (dummyTex) dummyTex->bind(2);
    m_pbrShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

    const bool hasAo = mat && (mat->ao != 0);
    glActiveTexture(GL_TEXTURE3);
    if (hasAo) assets.getTexture(mat->ao)->bind(3);
    else if (dummyTex) dummyTex->bind(3);
    m_pbrShader->setInt("uHasAoMap", hasAo ? 1 : 0);

    m_pbrShader->setVec3 ("uAlbedoTint",    mat ? mat->albedoTint    : glm::vec3(0.8f));
    m_pbrShader->setFloat("uMetallicMult",  mat ? mat->metallicMult  : 0.0f);
    m_pbrShader->setFloat("uRoughnessMult", mat ? mat->roughnessMult : 0.6f);
    m_pbrShader->setFloat("uAoMult",        mat ? mat->aoMult        : 1.0f);

    glActiveTexture(GL_TEXTURE0);  // dejar unit 0 activa para el draw
}

// ============================================================================
// Path: mesh del proyecto
// ============================================================================

GLuint MeshThumbnailRenderer::thumbnailFor(u32 meshId, AssetManager& assets) {
    if (!m_pbrShader || m_size == 0) return 0u;

    if (auto it = m_cache.find(meshId); it != m_cache.end()) {
        return it->second ? it->second->glColorTextureId() : 0u;
    }

    MeshAsset* asset = assets.getMesh(meshId);
    if (asset == nullptr || asset->submeshes.empty()) return 0u;

    auto fb = std::make_unique<OpenGLFramebuffer>(
        m_size, m_size, OpenGLFramebuffer::Format::LDR);

    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    fb->bind();
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
    renderMeshToBoundFbo(meshId, assets);
    fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    const GLuint tex = fb->glColorTextureId();
    m_cache.emplace(meshId, std::move(fb));
    return tex;
}

void MeshThumbnailRenderer::renderMeshToBoundFbo(u32 meshId, AssetManager& assets) {
    MeshAsset* asset = assets.getMesh(meshId);
    if (asset == nullptr || asset->submeshes.empty()) return;

    clearAndSetGlState();

    // Modelo: rotación de import (para que salga derecho). Orden YXZ igual que
    // TransformComponent.
    const glm::vec3 e = glm::radians(asset->importRotationEuler);
    glm::mat4 model(1.0f);
    model = glm::rotate(model, e.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, e.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, e.z, glm::vec3(0.0f, 0.0f, 1.0f));

    m_pbrShader->bind();
    setCamera(asset->aabbMin, asset->aabbMax, model);
    bindInvariantState();

    const std::vector<MaterialAssetId> matIds = assets.createMaterialsForMesh(meshId);

    for (const SubMesh& sub : asset->submeshes) {
        IMesh* mesh = sub.mesh.get();
        if (mesh == nullptr) continue;

        const MaterialAsset* mat = nullptr;
        if (!matIds.empty()) {
            const usize idx = std::min<usize>(sub.materialIndex, matIds.size() - 1);
            mat = assets.getMaterial(matIds[idx]);
        }
        bindMaterial(mat, assets);

        mesh->bind();
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh->vertexCount()));
    }
}

// ============================================================================
// Path: primitiva CSG
// ============================================================================

GLuint MeshThumbnailRenderer::thumbnailForPrimitive(PrimitiveKind kind,
                                                    AssetManager& assets) {
    if (!m_pbrShader || m_size == 0) return 0u;

    const int key = static_cast<int>(kind);
    if (auto it = m_primCache.find(key); it != m_primCache.end()) {
        return it->second ? it->second->glColorTextureId() : 0u;
    }

    // Construir el mesh del brush una sola vez.
    const Csg::Brush brush = buildPrimitiveBrush(kind);
    const Csg::BrushMeshData data = Csg::buildBrushMesh(brush);
    std::vector<f32> verts;
    for (const auto& sub : data.submeshes) {
        const std::vector<f32> part = Csg::brushSubmeshToInterleaved(sub);
        verts.insert(verts.end(), part.begin(), part.end());
    }
    if (verts.empty()) return 0u;

    // AABB desde las posiciones (stride 11, primeros 3 floats = pos).
    glm::vec3 lo(1e9f), hi(-1e9f);
    for (usize i = 0; i + 2 < verts.size(); i += 11) {
        const glm::vec3 p(verts[i], verts[i + 1], verts[i + 2]);
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }

    std::unique_ptr<IMesh> mesh = assets.createDynamicMesh(verts, kBrushAttrs);
    if (mesh == nullptr) return 0u;

    auto fb = std::make_unique<OpenGLFramebuffer>(
        m_size, m_size, OpenGLFramebuffer::Format::LDR);

    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    fb->bind();
    glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
    renderRawMeshToBoundFbo(mesh.get(), lo, hi, assets);
    fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    const GLuint tex = fb->glColorTextureId();
    m_primCache.emplace(key, std::move(fb));
    // `mesh` se destruye acá: la miniatura ya quedó como textura en el FBO.
    return tex;
}

void MeshThumbnailRenderer::renderRawMeshToBoundFbo(IMesh* mesh,
                                                    const glm::vec3& aabbMin,
                                                    const glm::vec3& aabbMax,
                                                    AssetManager& assets) {
    if (mesh == nullptr) return;
    clearAndSetGlState();
    m_pbrShader->bind();
    setCamera(aabbMin, aabbMax, glm::mat4(1.0f));
    bindInvariantState();
    bindMaterial(nullptr, assets);  // look default (sin material)
    mesh->bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh->vertexCount()));
}

} // namespace Mood
