#include "engine/render/preview/MaterialPreviewRenderer.h"

#include "core/Log.h"
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

#include <chrono>
#include <stdexcept>

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

MaterialPreviewRenderer::~MaterialPreviewRenderer() = default;

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

    const MeshAssetId sphereId = assets.primitiveSphereId();
    if (sphereId == 0) return;
    MeshAsset* sphere = assets.getMesh(sphereId);
    if (sphere == nullptr || sphere->submeshes.empty()) return;
    IMesh* mesh = sphere->submeshes[0].mesh.get();
    if (mesh == nullptr) return;

    // Guardar viewport actual para restaurar al final.
    GLint prevViewport[4]{};
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // ---- Bind FBO + clear ----
    m_fb->bind();
    glViewport(0, 0,
               static_cast<GLsizei>(m_width),
               static_cast<GLsizei>(m_height));

    // Clear con un gris neutro para que el material se distinga del fondo.
    glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // ---- Camara fija frontal + rotacion lenta del modelo ----
    // F2H21 polish: el dev pidio "que se vea 3D" — rotamos la esfera
    // lentamente sobre Y para que muestre todas las caras. Tiempo
    // absoluto del clock monotonico (sin dt acumulado en miembro);
    // velocidad ~22 grados/segundo (1 vuelta cada ~16s).
    const auto now = std::chrono::steady_clock::now();
    const f64 tSec = std::chrono::duration<f64>(
        now.time_since_epoch()).count();
    const f32 angle = static_cast<f32>(tSec) * 0.4f;
    const glm::mat4 model =
        glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));

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

    // ---- Draw ----
    glActiveTexture(GL_TEXTURE0);  // dejar texture 0 activa para el draw
    mesh->bind();
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(mesh->vertexCount()));

    // ---- Restaurar estado ----
    m_fb->unbind();
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);
}

} // namespace Mood
