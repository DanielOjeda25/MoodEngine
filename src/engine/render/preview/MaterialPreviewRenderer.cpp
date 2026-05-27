#include "engine/render/preview/MaterialPreviewRenderer.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/preview/AssetThumbnailDiskCache.h"  // F3H15
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"

#include <glad/gl.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace Mood {

namespace {

// Layout PointLightStd430 igual que SceneRenderer. 48 bytes con padding
// std430 — el shader Forward+ no reorderea los structs, asi que tenemos
// que matchear byte por byte.
struct PointLightStd430 {
    f32 posX, posY, posZ; f32 _pad0;
    f32 colR, colG, colB; f32 intensity;
    f32 radius; f32 _pad1, _pad2, _pad3;
};
static_assert(sizeof(PointLightStd430) == 48,
              "PointLightStd430 layout mismatch con el shader");

// Light tile data: 2 u32 (count + offset). Igual que LightGrid.
struct LightTileStd430 {
    u32 count;
    u32 offset;
};

} // namespace

MaterialPreviewRenderer::MaterialPreviewRenderer(u32 width, u32 height)
    : m_width(width), m_height(height) {
    if (m_width == 0 || m_height == 0) {
        // FBO 0x0 es invalido — early return sin recursos. El caller
        // verifica outputTextureId() == 0 si necesita.
        return;
    }

    m_fb = std::make_unique<OpenGLFramebuffer>(
        m_width, m_height, OpenGLFramebuffer::Format::LDR);

    // Reusa los shaders del motor. El vertex shader pbr.vert espera
    // attribs pos(3) + color(3) + uv(2) + normal(3) — los mismos que
    // PrimitiveMeshes::createSphereMesh provee.
    m_pbrShader = std::make_unique<OpenGLShader>(
        "shaders/pbr.vert", "shaders/pbr.frag");

    // F3H15: shader del fondo gradient (mismo que F3H14 — un solo set
    // de archivos compartido entre los preview renderers).
    m_bgShader = std::make_unique<OpenGLShader>(
        "shaders/thumbnail_bg.vert", "shaders/thumbnail_bg.frag");

    // F3H15: VAO empty necesario para glDrawArrays en core profile.
    glGenVertexArrays(1, &m_dummyVao);

    // SSBOs vacios para Forward+: 1 dummy point light (count=0 igual,
    // pero el SSBO no puede estar vacio porque algunos drivers fallan
    // al bindear un buffer de tamaño 0).
    m_pointLightsSsbo = std::make_unique<OpenGLSSBO>();
    m_lightTilesSsbo  = std::make_unique<OpenGLSSBO>();
    m_lightIndicesSsbo = std::make_unique<OpenGLSSBO>();

    PointLightStd430 dummyLight{};
    m_pointLightsSsbo->upload(&dummyLight, sizeof(dummyLight));

    LightTileStd430 emptyTile{0u, 0u};
    m_lightTilesSsbo->upload(&emptyTile, sizeof(emptyTile));

    const u32 zero = 0u;
    m_lightIndicesSsbo->upload(&zero, sizeof(u32));
}

MaterialPreviewRenderer::~MaterialPreviewRenderer() {
    if (m_dummyVao != 0) {
        glDeleteVertexArrays(1, &m_dummyVao);
        m_dummyVao = 0;
    }
}

void MaterialPreviewRenderer::setDiskCacheRoot(std::filesystem::path cacheRoot) {
    m_diskCacheRoot = std::move(cacheRoot);
}

void MaterialPreviewRenderer::setIblTextures(OpenGLCubemapTexture* irradiance,
                                               OpenGLCubemapTexture* prefilter,
                                               ITexture* brdfLut) {
    m_iblIrradiance = irradiance;
    m_iblPrefilter  = prefilter;
    m_iblBrdfLut    = brdfLut;
}

GLuint MaterialPreviewRenderer::outputTextureId() const {
    return m_fb ? m_fb->glColorTextureId() : 0u;
}

void MaterialPreviewRenderer::renderPreview(const MaterialAsset& mat,
                                              AssetManager& assets) {
    if (!m_fb || !m_pbrShader) return;

    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    m_fb->bind();
    glViewport(0, 0,
               static_cast<GLsizei>(m_width),
               static_cast<GLsizei>(m_height));

    // F2H21 polish: rotacion lenta sobre Y (tiempo absoluto del clock
    // monotonico, ~22°/s) para que el preview "se vea 3D".
    const auto now = std::chrono::steady_clock::now();
    const f64 tSec = std::chrono::duration<f64>(now.time_since_epoch()).count();
    renderSphereToBoundFbo(mat, assets, static_cast<f32>(tSec) * 0.4f);

    m_fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

GLuint MaterialPreviewRenderer::thumbnail(u32 materialId, AssetManager& assets) {
    return loadOrRenderMatThumb(materialId, assets, m_width, m_thumbCache);
}

GLuint MaterialPreviewRenderer::thumbnailLarge(u32 materialId, AssetManager& assets) {
    return loadOrRenderMatThumb(materialId, assets, kLargePreviewSize, m_largeCache);
}

// F3H16: helper interno load-or-render. Reusable por thumbnail (size del
// constructor, cache m_thumbCache) y thumbnailLarge (kLargePreviewSize,
// cache m_largeCache). El filename del cache disco incluye el size —
// los PNGs no chocan entre tamanos en el mismo directorio.
GLuint MaterialPreviewRenderer::loadOrRenderMatThumb(
        u32 materialId, AssetManager& assets,
        u32 size,
        std::unordered_map<u32, std::unique_ptr<OpenGLFramebuffer>>& cache) {
    if (!m_pbrShader || size == 0) return 0u;
    if (auto it = cache.find(materialId); it != cache.end()) {
        return it->second ? it->second->glColorTextureId() : 0u;
    }
    MaterialAsset* mat = assets.getMaterial(materialId);
    if (mat == nullptr) return 0u;

    // F3H15: cache disco con prefix "mat".
    const std::string logicalPath = assets.materialPathOf(materialId);
    const bool diskOn = !m_diskCacheRoot.empty() && !logicalPath.empty();
    std::filesystem::path cachePath;
    if (diskOn) {
        cachePath = AssetThumbnailDiskCache::pathFor(
            m_diskCacheRoot, "mat", logicalPath, size);
        const auto matFsPath = assets.resolvePath(logicalPath);
        std::vector<u8> rgba;
        u32 cachedW = 0, cachedH = 0;
        if (AssetThumbnailDiskCache::tryLoad(
                cachePath, matFsPath, rgba, cachedW, cachedH) &&
            cachedW == size && cachedH == size) {
            // HIT: upload directo. tryLoad ya devuelve los bytes en GL
            // convention (bottom-up) — no hace falta flip manual aca.
            auto fb = std::make_unique<OpenGLFramebuffer>(
                size, size, OpenGLFramebuffer::Format::LDR);
            const GLuint tex = fb->glColorTextureId();
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                             static_cast<GLsizei>(size),
                             static_cast<GLsizei>(size),
                             GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glBindTexture(GL_TEXTURE_2D, 0);
            cache.emplace(materialId, std::move(fb));
            return tex;
        }
    }

    auto fb = std::make_unique<OpenGLFramebuffer>(
        size, size, OpenGLFramebuffer::Format::LDR);
    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    fb->bind();
    glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    renderSphereToBoundFbo(*mat, assets, 0.6f);  // ángulo fijo 3/4 (estático)

    // F3H15: readback + store al disco.
    if (diskOn) {
        std::vector<u8> rgba(static_cast<usize>(size) * size * 4);
        glReadPixels(0, 0,
                      static_cast<GLsizei>(size),
                      static_cast<GLsizei>(size),
                      GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        std::vector<u8> flipped(rgba.size());
        const usize rowBytes = static_cast<usize>(size) * 4;
        for (u32 y = 0; y < size; ++y) {
            std::copy_n(rgba.data() + (size - 1 - y) * rowBytes,
                         rowBytes,
                         flipped.data() + y * rowBytes);
        }
        AssetThumbnailDiskCache::store(cachePath, flipped.data(), size, size);
    }

    fb->unbind();
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    const GLuint tex = fb->glColorTextureId();
    cache.emplace(materialId, std::move(fb));
    return tex;
}

void MaterialPreviewRenderer::clearThumbnailCache() {
    m_thumbCache.clear();
    m_largeCache.clear();
}

void MaterialPreviewRenderer::renderSphereToBoundFbo(const MaterialAsset& mat,
                                                     AssetManager& assets,
                                                     f32 angleRad) {
    const MeshAssetId sphereId = assets.primitiveSphereId();
    if (sphereId == 0) return;
    MeshAsset* sphere = assets.getMesh(sphereId);
    if (sphere == nullptr || sphere->submeshes.empty()) return;
    IMesh* mesh = sphere->submeshes[0].mesh.get();
    if (mesh == nullptr) return;

    // Clear con un gris neutro como fallback si el shader bg fallo.
    glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // F3H15: fondo gradient (mismo patron que F3H14 en MeshThumbnailRenderer).
    // Depth test off para que el quad fullscreen no escriba depth.
    if (m_bgShader && m_dummyVao != 0) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        m_bgShader->bind();
        glBindVertexArray(m_dummyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    const glm::mat4 model =
        glm::rotate(glm::mat4(1.0f), angleRad, glm::vec3(0.0f, 1.0f, 0.0f));

    const f32 aspect = static_cast<f32>(m_width) / static_cast<f32>(m_height);
    const glm::vec3 camPos(0.0f, 0.0f, 2.5f);
    const glm::mat4 view = glm::lookAt(camPos,
                                        glm::vec3(0.0f),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection =
        glm::perspective(glm::radians(30.0f), aspect, 0.1f, 100.0f);

    // ---- Bind SSBOs vacios para el path Forward+ del shader ----
    m_pointLightsSsbo->bind(2);
    m_lightTilesSsbo->bind(3);
    m_lightIndicesSsbo->bind(4);

    // ---- Bind IBL si esta disponible ----
    const bool iblOk =
        m_iblIrradiance != nullptr &&
        m_iblPrefilter != nullptr &&
        m_iblBrdfLut != nullptr;
    f32 prefilterMaxLod = 0.0f;
    if (iblOk) {
        m_iblIrradiance->bind(4);
        m_iblPrefilter->bind(5);
        m_iblBrdfLut->bind(6);
        prefilterMaxLod = static_cast<f32>(m_iblPrefilter->mipLevels() - 1);
    }

    // ---- Setear uniforms del shader ----
    m_pbrShader->bind();
    m_pbrShader->setMat4("uView", view);
    m_pbrShader->setMat4("uProjection", projection);
    m_pbrShader->setMat4("uModel", model);
    m_pbrShader->setVec3("uCameraPos", camPos);
    // F2H21 polish: ambient mas alto (0.20) para que la mitad de la esfera
    // que NO recibe la directional no se vea totalmente negra. El IBL
    // (cuando esta cargado) cubre esto correctamente, pero con IBL off
    // el fallback escalar necesita un piso mas vivo.
    m_pbrShader->setVec3("uAmbient", glm::vec3(0.20f));

    // Texture units (constantes — los samplers del shader).
    m_pbrShader->setInt("uAlbedoMap",         0);
    m_pbrShader->setInt("uShadowMap",         1);
    m_pbrShader->setInt("uMetallicRoughness", 2);
    m_pbrShader->setInt("uAoMap",             3);
    m_pbrShader->setInt("uIrradianceMap",     4);
    m_pbrShader->setInt("uPrefilterMap",      5);
    m_pbrShader->setInt("uBrdfLut",           6);

    // 1 directional fija — desde arriba-derecha-frente (3/4 light).
    const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -0.6f, -0.7f));
    m_pbrShader->setVec3("uDirectional.direction", lightDir);
    m_pbrShader->setVec3("uDirectional.color",     glm::vec3(1.0f));
    m_pbrShader->setFloat("uDirectional.intensity", 3.0f);
    m_pbrShader->setInt  ("uDirectional.enabled",   1);

    // Forward+ deshabilitado funcionalmente: 1 tile que no apunta a
    // ninguna luz. Los SSBOs vacios cubren el binding requirement.
    m_pbrShader->setInt("uTileSize", 16);
    m_pbrShader->setInt("uTilesX",   1);
    m_pbrShader->setInt("uTilesY",   1);

    // IBL on/off + intensity.
    m_pbrShader->setInt  ("uIblEnabled",      iblOk ? 1 : 0);
    m_pbrShader->setFloat("uPrefilterMaxLod", prefilterMaxLod);
    m_pbrShader->setFloat("uIblIntensity",    1.0f);

    // Sin shadow.
    m_pbrShader->setInt  ("uShadowEnabled", 0);
    m_pbrShader->setFloat("uShadowBias",    0.005f);
    m_pbrShader->setMat4 ("uLightSpace",    glm::mat4(1.0f));

    // Sin fog.
    m_pbrShader->setInt  ("uFogMode",    0);
    m_pbrShader->setVec3 ("uFogColor",   glm::vec3(0.0f));
    m_pbrShader->setFloat("uFogDensity", 0.0f);
    m_pbrShader->setFloat("uFogStart",   0.0f);
    m_pbrShader->setFloat("uFogEnd",     0.0f);

    // ---- Bindear texturas del material ----
    ITexture* dummyTex = assets.getTexture(assets.missingTextureId());
    const bool hasAlbedo = mat.useAlbedoMap;
    glActiveTexture(GL_TEXTURE0);
    assets.getTexture(hasAlbedo ? mat.albedo : 0)->bind(0);
    m_pbrShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

    // Slot 1 (uShadowMap) requiere SOMETHING bindeado aunque uShadowEnabled=0;
    // bindeamos la dummy 2D texture. Algunos drivers compilan el shader con
    // `texture(uShadowMap, ...)` aunque el branch nunca se tome.
    glActiveTexture(GL_TEXTURE1);
    if (dummyTex != nullptr) {
        dummyTex->bind(1);
    }

    const bool hasMR = (mat.metallicRoughness != 0);
    glActiveTexture(GL_TEXTURE2);
    if (hasMR) {
        assets.getTexture(mat.metallicRoughness)->bind(2);
    } else if (dummyTex != nullptr) {
        dummyTex->bind(2);
    }
    m_pbrShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

    const bool hasAo = (mat.ao != 0);
    glActiveTexture(GL_TEXTURE3);
    if (hasAo) {
        assets.getTexture(mat.ao)->bind(3);
    } else if (dummyTex != nullptr) {
        dummyTex->bind(3);
    }
    m_pbrShader->setInt("uHasAoMap", hasAo ? 1 : 0);

    // Multiplicadores escalares.
    m_pbrShader->setVec3 ("uAlbedoTint",    mat.albedoTint);
    m_pbrShader->setFloat("uMetallicMult",  mat.metallicMult);
    m_pbrShader->setFloat("uRoughnessMult", mat.roughnessMult);
    m_pbrShader->setFloat("uAoMult",        mat.aoMult);

    // F2H63: el preview render siempre usa el path Opaque (la sphere
    // preview no tiene un backbuffer detras para refractar; sin sentido
    // mostrar transparencia en el thumbnail). El material translucido
    // se previsualiza como si fuera opaco — tradeoff aceptable para v1.
    m_pbrShader->setInt  ("uBlendMode",          0);
    m_pbrShader->setFloat("uOpacity",            1.0f);
    m_pbrShader->setFloat("uIor",                1.0f);
    m_pbrShader->setFloat("uRefractionStrength", 0.0f);
    m_pbrShader->setInt  ("uBackbufferCopy",     7);
    m_pbrShader->setVec2 ("uScreenSize",
                          glm::vec2(static_cast<f32>(m_width),
                                    static_cast<f32>(m_height)));

    // ---- Draw ----
    glActiveTexture(GL_TEXTURE0);  // dejar texture 0 activa para el draw
    mesh->bind();
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(mesh->vertexCount()));
}

} // namespace Mood
