#include "systems/render/ShadowPass.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/pipeline/ShadowMath.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLShadowMapArray.h"
#include "engine/render/resources/MaterialAsset.h"  // F2H64: BlendMode + castTranslucentShadow
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/CompiledMeshComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Mood {

ShadowPass::ShadowPass(u32 shadowMapSize, u32 cascadeCount) {
    if (cascadeCount == 0) cascadeCount = 1;
    if (cascadeCount > kMaxCsmCascades) cascadeCount = kMaxCsmCascades;
    m_cascadeCount = cascadeCount;
    m_shadowMapArray = std::make_unique<OpenGLShadowMapArray>(shadowMapSize,
                                                                cascadeCount);
    m_shader = std::make_unique<OpenGLShader>(
        "shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
    // F2H64: shader del sub-pase tinted (Translucent shadow casters).
    // Tolera fallo de carga: si no compila, el opaque pass sigue
    // funcionando y los Translucent simplemente no proyectan sombra.
    try {
        m_tintedShader = std::make_unique<OpenGLShader>(
            "shaders/shadow_tinted.vert", "shaders/shadow_tinted.frag");
    } catch (const std::exception& e) {
        Log::render()->warn("ShadowPass tinted shader no disponible: {}. "
                              "Sombras tintadas desactivadas (vidrios sin sombra).",
                              e.what());
        m_tintedShader.reset();
    }
    // Identidad por default para todos los slots (evita uniform garbage
    // si el lit shader samplea antes del primer recordCsm).
    m_lightSpaces.fill(glm::mat4(1.0f));
    m_cascadeSplits.fill(0.0f);
    Log::render()->info("ShadowPass inicializado (CSM, {} cascadas)",
                          cascadeCount);
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::recreate(u32 shadowMapSize, u32 cascadeCount) {
    if (cascadeCount == 0) cascadeCount = 1;
    if (cascadeCount > kMaxCsmCascades) cascadeCount = kMaxCsmCascades;
    if (m_shadowMapArray &&
        m_shadowMapArray->size() == shadowMapSize &&
        m_cascadeCount == cascadeCount) {
        return;  // no-op
    }
    m_shadowMapArray = std::make_unique<OpenGLShadowMapArray>(shadowMapSize,
                                                                cascadeCount);
    m_cascadeCount = cascadeCount;
    m_lightSpaces.fill(glm::mat4(1.0f));
    m_cascadeSplits.fill(0.0f);
    Log::render()->info("ShadowPass recreado ({} x {} x {} cascadas)",
                          shadowMapSize, shadowMapSize, cascadeCount);
}

u32 ShadowPass::shadowMapSize() const {
    return m_shadowMapArray ? m_shadowMapArray->size() : 0;
}

void ShadowPass::bindShadowMap(u32 textureUnit) const {
    if (!m_shadowMapArray) return;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY,
                   m_shadowMapArray->glDepthTextureId());
}

void ShadowPass::bindShadowColorMap(u32 textureUnit) const {
    if (!m_shadowMapArray) return;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY,
                   m_shadowMapArray->glColorTextureId());
}

const glm::mat4& ShadowPass::lightSpaceMatrix(u32 cascadeIdx) const {
    static const glm::mat4 kIdentity{1.0f};
    if (cascadeIdx >= kMaxCsmCascades) return kIdentity;
    return m_lightSpaces[cascadeIdx];
}

f32 ShadowPass::cascadeSplit(u32 splitIdx) const {
    if (splitIdx > kMaxCsmCascades) return 0.0f;
    return m_cascadeSplits[splitIdx];
}

void ShadowPass::recordCsm(Scene& scene,
                             AssetManager& assets,
                             IRenderer& renderer,
                             const glm::vec3& lightDir,
                             const glm::mat4& cameraView,
                             const glm::mat4& cameraProj,
                             u32 cascadeCount,
                             f32 lambda) {
    if (!m_shadowMapArray || !m_shader) return;
    if (cascadeCount == 0) cascadeCount = 1;
    if (cascadeCount > m_cascadeCount) cascadeCount = m_cascadeCount;

    // Splits del frustum. Pre-computar antes del loop porque los necesi-
    // tamos como uniforms del lit shader.
    //
    // F2H60 iter5 bugfix: las formulas estaban INVERTIDAS. Para proj
    // perspectivo right-handed GLM (glm::perspective):
    //   m22 = proj[2][2] = -(f+n)/(f-n)
    //   m32 = proj[3][2] = -2*f*n/(f-n)
    // Sistema: -(f+n) = m22*(f-n), -2fn = m32*(f-n). Dividiendo:
    //   n = m32 / (m22 - 1)
    //   f = m32 / (m22 + 1)
    // (Pre-fix tenia n y f swapeados -> el log mostraba
    // near=100, far=0.1, splits=[100..101] -> cascadas proyectaban
    // fuera de cuadro y mover lambda/cascadas no surtia efecto visible).
    const f32 m22 = cameraProj[2][2];
    const f32 m32 = cameraProj[3][2];
    f32 cameraNear = (std::abs(m22 - 1.0f) > 1e-4f)
                        ? (m32 / (m22 - 1.0f))
                        : 0.1f;
    f32 cameraFar  = (std::abs(m22 + 1.0f) > 1e-4f)
                        ? (m32 / (m22 + 1.0f))
                        : 100.0f;
    cameraNear = std::abs(cameraNear);
    cameraFar  = std::abs(cameraFar);

    const auto splits = computeCsmSplits(cameraNear, cameraFar,
                                           cascadeCount, lambda);
    m_cascadeSplits = splits;

    // F2H60 polish iter4: log diagnostico de los splits + params cuando
    // cambian. Permite verificar que el slider de cascadas/lambda
    // realmente llega al shadow pass (pre-fix iter4, applyEnvironment-
    // FromScene corria DESPUES y los cambios tenian 1 frame de lag).
    if (cascadeCount != m_lastLoggedCascadeCount ||
        std::fabs(lambda - m_lastLoggedLambda) > 1e-3f) {
        std::string splitStr;
        for (u32 i = 0; i <= cascadeCount; ++i) {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%.2f", splits[i]);
            splitStr += buf;
            if (i < cascadeCount) splitStr += " | ";
        }
        Log::render()->info(
            "ShadowPass params: cascadas={}, lambda={:.2f}, near={:.2f}, "
            "far={:.2f}, splits=[{}], smSize={}x{}",
            cascadeCount, lambda, cameraNear, cameraFar, splitStr,
            m_shadowMapArray->size(), m_shadowMapArray->size());
        m_lastLoggedCascadeCount = cascadeCount;
        m_lastLoggedLambda       = lambda;
    }

    // Capturar GL state previo para restaurar al final.
    GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    GLint prevCullFace = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFace);
    if (!wasCullEnabled) glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    m_shader->bind();

    const u32 smSize = m_shadowMapArray->size();

    // F2H60 polish iter3: contadores de casters por tipo. Pre-fix el
    // pase solo iteraba `MeshRendererComponent`, asi que los brushes
    // CSG (BrushComponent) y la mesh compilada (CompiledMeshComponent)
    // NO proyectaban sombras -- el dev veia "luz directional con
    // castShadows ON pero ningun shadow en el viewport". Ahora incluimos
    // los 3 paths y logueamos los conteos para diagnostico.
    u32 castersMR  = 0; // MeshRenderer
    u32 castersBR  = 0; // Brush (CSG)
    u32 castersCM  = 0; // CompiledMesh
    u32 brushesNoCache = 0; // brushes que aun no tienen meshCache (frame 1)

    // F2H64: helper inline para chequear el BlendMode del primer
    // material de un MeshRenderer. Usado para filtrar Translucent del
    // opaque shadow pass (vidrios no escriben depth) y para enumerarlos
    // en el sub-pase tinted.
    auto materialOfMesh = [&](MeshRendererComponent& mr) -> const MaterialAsset* {
        return assets.getMaterial(mr.materialOrMissing(0));
    };

    for (u32 i = 0; i < cascadeCount; ++i) {
        const ShadowMatrices sm = computeCascadeShadowMatrices(
            cameraView, cameraProj,
            splits[i], splits[i + 1],
            lightDir, smSize);
        m_lightSpaces[i] = sm.lightSpace;

        // F2H64: bind con withColor=true para que el clear del color
        // attachment quede en (1,1,1,1) por cascada -- "luz blanca pasa".
        // El opaque pass que viene a continuacion tiene glDrawBuffer=NONE
        // en el bind (withColor switchea ambos), pero el clear via
        // glClearBufferfv apunta directo al attachment y se ejecuta una
        // vez antes del opaque pass.
        m_shadowMapArray->bindLayerAsTarget(i, /*withColor=*/true);
        glClear(GL_DEPTH_BUFFER_BIT);
        // Clear del color attachment a blanco: "sin tinte" baseline.
        const GLfloat whiteTint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, whiteTint);

        // Switchear a opaque pass: drawBuffer NONE, depth-only.
        glDrawBuffer(GL_NONE);

        m_shader->bind();
        m_shader->setMat4("uLightSpace", sm.lightSpace);

        // Por simplicidad de la primera iteracion CSM, dibujamos TODAS
        // las meshes en cada cascada (sin frustum cull per-cascade). La
        // cascada lejos cubre la mayor parte del mundo de todos modos.
        // Optimizacion: per-cascade frustum cull con `Frustum.h` queda
        // como tuning si el N-pass impacta perf medible.

        // (1) MeshRenderer entities (modelos importados, primitivos
        //     procedurales sin Brush). F2H64: filtramos Translucent
        //     (luz pasa por vidrios; vidrios tinted dibujan en el
        //     sub-pase tinted abajo). Additive: skipeado completamente
        //     (no tiene sentido fisico como shadow caster).
        scene.forEach<TransformComponent, MeshRendererComponent>(
            [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
                MeshAsset* asset = assets.getMesh(mr.mesh);
                if (asset == nullptr) return;
                const MaterialAsset* mat = materialOfMesh(mr);
                if (mat != nullptr && mat->blendMode != BlendMode::Opaque) {
                    return;  // F2H64: Translucent/Additive NO escriben depth
                }
                m_shader->setMat4("uModel", t.worldMatrix());
                for (const auto& sub : asset->submeshes) {
                    if (sub.mesh == nullptr) continue;
                    renderer.drawMesh(*sub.mesh, *m_shader);
                    if (i == 0) ++castersMR; // contar 1 vez (cascada 0)
                }
            });

        // (2) BrushComponent (CSG -- box / sphere / cylinder / etc.). El
        //     meshCache se construye lazy en el PBR pass del SceneRenderer;
        //     si todavia no se renderearon (primer frame), skipeamos --
        //     en el frame N+1 ya estaran poblados.
        //
        //     IMPORTANTE (F2H60 iter3 bugfix): las posiciones de
        //     `BrushMeshVertex` viven en LOCAL space (ver
        //     BrushMesh.cpp:154 -- `v.position = p` sin multiplicar
        //     por worldMatrix; el worldMatrix solo se usa para UVs
        //     lockToWorld). Asi que uModel = t.worldMatrix(), mismo
        //     criterio que el PBR pass. Pre-fix usabamos identity ->
        //     todos los brushes se renderaban al origen en el shadow
        //     map -> la sombra del cubo se quedaba clavada en (0,0,0)
        //     y al bajar el cubo encima parecia "desaparecer".
        scene.forEach<TransformComponent, BrushComponent>(
            [&](Entity, TransformComponent& t, BrushComponent& bc) {
                if (bc.meshCache.empty()) {
                    if (i == 0) ++brushesNoCache;
                    return;
                }
                m_shader->setMat4("uModel", t.worldMatrix());
                for (auto& mesh : bc.meshCache) {
                    if (!mesh) continue;
                    renderer.drawMesh(*mesh, *m_shader);
                    if (i == 0) ++castersBR;
                }
            });

        // (3) CompiledMeshComponent (MoodPlayer: brushes compilados a
        //     mesh estatica unificada).
        scene.forEach<CompiledMeshComponent>(
            [&](Entity, CompiledMeshComponent& cm) {
                if (cm.meshes.empty()) return;
                m_shader->setMat4("uModel", glm::mat4(1.0f));
                for (auto& mesh : cm.meshes) {
                    if (!mesh) continue;
                    renderer.drawMesh(*mesh, *m_shader);
                    if (i == 0) ++castersCM;
                }
            });

        // F2H64: sub-pase tinted. Translucent shadow casters escriben
        // su tinte al color attachment. depth-write OFF, depth-test ON
        // (un vidrio detras de un opaco no contribuye al tinte).
        // Blend ZERO/SRC_COLOR: multiplicativo, acumula tintes superpuestos.
        if (m_tintedShader) {
            // Switch a color attachment + drawBuffer 0.
            const GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, drawBufs);
            // Cull face para tinted: igual que opaque (front face culling
            // ya esta activo desde el bloque al inicio del recordCsm).

            glDepthMask(GL_FALSE);    // NO escribir depth
            glEnable(GL_DEPTH_TEST);  // pero SI testear contra el depth opaco
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);  // dst *= src (multiplicativo)

            m_tintedShader->bind();
            m_tintedShader->setMat4("uLightSpace", sm.lightSpace);
            scene.forEach<TransformComponent, MeshRendererComponent>(
                [&](Entity, TransformComponent& t, MeshRendererComponent& mr) {
                    MeshAsset* asset = assets.getMesh(mr.mesh);
                    if (asset == nullptr) return;
                    const MaterialAsset* mat = materialOfMesh(mr);
                    if (mat == nullptr) return;
                    if (mat->blendMode != BlendMode::Translucent) return;
                    if (!mat->castTranslucentShadow) return;

                    m_tintedShader->setMat4("uModel", t.worldMatrix());
                    m_tintedShader->setVec3("uAlbedoTint", mat->albedoTint);
                    m_tintedShader->setFloat("uOpacity", mat->opacity);
                    for (const auto& sub : asset->submeshes) {
                        if (sub.mesh == nullptr) continue;
                        renderer.drawMesh(*sub.mesh, *m_tintedShader);
                    }
                });

            // Restaurar state: depth-write ON para la proxima cascada
            // (el opaque pass de cascada i+1 lo necesita), blend OFF,
            // y volver al shader opaque + drawBuffer NONE.
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // default
            glDrawBuffer(GL_NONE);
            m_shader->bind();  // re-bind opaque para la proxima iteracion
        }
    }

    // Restaurar GL state.
    glCullFace(static_cast<GLenum>(prevCullFace));
    if (!wasCullEnabled) glDisable(GL_CULL_FACE);

    m_shadowMapArray->unbind();

    // Diagnostico one-shot al cambiar el conteo de casters (para
    // verificar que los brushes / compiled meshes entran al shadow pass).
    const u32 totalCasters = castersMR + castersBR + castersCM;
    if (totalCasters != m_lastLoggedCasterTotal ||
        brushesNoCache != m_lastLoggedBrushesNoCache) {
        Log::render()->info(
            "ShadowPass casters: total={} (MeshRenderer={}, Brush={}, "
            "CompiledMesh={}), brushes_sin_cache={}",
            totalCasters, castersMR, castersBR, castersCM, brushesNoCache);
        m_lastLoggedCasterTotal     = totalCasters;
        m_lastLoggedBrushesNoCache  = brushesNoCache;
    }
}

} // namespace Mood
