// F2H24 Bloque C: frame loop del SceneRenderer (Hito 21 Bloque 2).
// renderScene orquesta el render del frame completo: shadow pass,
// skybox, environment apply, light grid Forward+, PBR static (instanced
// + non-batchable), PBR skinned, brushes CSG, particulas. El caller
// puede acumular geometria de debug despues de esta llamada y antes
// de endFrame (en SceneRenderer.cpp).
//
// Notas: archivo grande (~580 LOC). El frame loop es una unidad
// cohesiva con muchas variables locales compartidas entre pases —
// partir mas fino requeriria extraer metodos privados con todas las
// dependencias como parametros, lo cual no aporta legibilidad. Se
// acepta como deuda chica que sigue bajo el hard cap 800 LOC.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/animation/skeleton/Skeleton.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLInstanceBuffer.h"
#include "engine/render/backend/opengl/OpenGLOitFramebuffer.h"  // F2H64
#include "engine/render/backend/opengl/OpenGLParticleRenderer.h"
#include "engine/render/backend/opengl/OpenGLRenderer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/pipeline/Frustum.h"
#include "engine/render/pipeline/ShadowMath.h"  // F2H60: kMaxCsmCascades
#include "engine/render/pipeline/LightGrid.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/scene_renderer/RenderBatching.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/RendererTypes.h"
#include "engine/scene/VisGroup.h"  // F2H33: hide gate
#include "engine/shader_graph/ShaderGraphCache.h"  // F2H62 Bloque E
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/CompiledMeshComponent.h"  // F2H26
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/BrushMesh.h"
#include "systems/light/LightSystem.h"
#include "systems/render/ShadowPass.h"
#include "systems/render/SkyboxRenderer.h"

#include <glad/gl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace Mood {

void SceneRenderer::renderScene(Scene& scene,
                                 AssetManager& assets,
                                 const glm::mat4& view,
                                 const glm::mat4& projection,
                                 f32 aspect,
                                 const glm::vec3& cameraPos,
                                 u32 panelWidth,
                                 u32 panelHeight) {
    MOOD_PROFILE_FUNCTION();
    if (panelWidth > 0 && panelHeight > 0) {
        m_sceneFb->resize(panelWidth, panelHeight);
        if (m_ssaoOutFb) m_ssaoOutFb->resize(panelWidth, panelHeight); // F2H56
        if (m_bloomFb) m_bloomFb->resize(panelWidth, panelHeight);     // F2H55
        if (m_ssrFb) m_ssrFb->resize(panelWidth, panelHeight);         // F2H61
        if (m_backbufferCopyFb) m_backbufferCopyFb->resize(panelWidth, panelHeight); // F2H63
        ensureOitFb(panelWidth, panelHeight);                           // F2H64
        m_viewportFb->resize(panelWidth, panelHeight);
    }

    // Hito 13: guardar matrices para que el overlay 2D del ViewportPanel
    // proyecte posiciones al frame siguiente.
    m_lastView = view;
    m_lastProjection = projection;
    m_lastAspect = aspect;

    // F2H62 Bloque E: actualizar uTime acumulado para shader graphs
    // animados. Usamos un steady_clock para no depender de un dt
    // externo (renderScene no recibe dt). Resolucion: segundos desde
    // el primer frame.
    {
        static const auto k_appStart = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::duration<f32>>(now - k_appStart);
        m_currentTime = elapsed.count();
    }

    // F2H60 polish iter4: aplicar Environment ANTES del shadow pass para
    // que los cambios de cascadas/lambda surtan efecto en el mismo frame.
    // Pre-fix se llamaba en linea 136 (despues del shadow pass) -> los
    // sliders tenian 1 frame de lag y parecian no responder en tiempo
    // real. No tiene side-effects de GL state, solo poblar miembros.
    applyEnvironmentFromScene(scene);

    // Hito 16: Shadow pass ANTES de bindear el scene FB.
    bool shadowEnabled = false;
    glm::vec3 shadowLightDir(0.0f, -1.0f, 0.0f);
    if (m_shadowPass) {
        scene.forEach<LightComponent>(
            [&](Entity, LightComponent& lc) {
                if (shadowEnabled) return;
                if (!lc.enabled) return;
                if (lc.type != LightComponent::Type::Directional) return;
                if (!lc.castShadows) return;
                shadowEnabled = true;
                shadowLightDir = lc.direction;
            });
    }
    if (shadowEnabled) {
        // F2H60: Cascade Shadow Maps. Cache F2H42 eliminado: CSM depende
        // de la matriz de camara que cambia casi siempre. El gate global
        // m_csmEnabled fue eliminado en F2H60 polish iter2 -- las sombras
        // se controlan solo via LightComponent::castShadows.
        // Recreate del array si cambio cascadeCount via Inspector.
        if (m_shadowPass->cascadeCount() != m_csmCascadeCount) {
            m_shadowPass->recreate(m_shadowPass->shadowMapSize(),
                                     m_csmCascadeCount);
        }
        MOOD_PROFILE_SCOPE("ShadowPass::recordCsm");
        m_shadowPass->recordCsm(scene, assets, *m_renderer,
                                  shadowLightDir, view, projection,
                                  m_csmCascadeCount, m_csmSplitLambda);
    }
    if (shadowEnabled != m_shadowEnabledLastFrame) {
        if (shadowEnabled) {
            Log::render()->info(
                "ShadowPass ACTIVO (directional con castShadows detectada, "
                "dir=({:.2f},{:.2f},{:.2f}))",
                shadowLightDir.x, shadowLightDir.y, shadowLightDir.z);
        } else {
            Log::render()->info("ShadowPass inactivo (sin directional con castShadows)");
        }
        m_shadowEnabledLastFrame = shadowEnabled;
    }

    // El render de la escena (sky + lit + fog + debug) escribe al HDR.
    m_sceneFb->bind();

    ClearValues clear{};
    clear.color = {0.07f, 0.12f, 0.22f, 1.0f};
    m_renderer->beginFrame(clear);

    // F2H61: el clear global pisa AMBOS color attachments con el sky color.
    // Para el normal RT (location 1) necesitamos (0,0,0,0) para que el SSR
    // distinga pixels "no-PBR" via alpha < 0.5. Si el FB se construyo sin
    // normal RT, glClearBufferfv en draw buffer 1 es no-op silencioso.
    if (m_sceneFb->glNormalTextureId() != 0) {
        const GLfloat zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearBufferfv(GL_COLOR, 1, zero);
    }

    // Skybox primero (depth=1 + LEQUAL).
    if (m_skyboxRenderer) {
        MOOD_PROFILE_SCOPE("Skybox::draw");
        m_skyboxRenderer->draw(view, projection);
    }

    // F2H60 polish iter4: applyEnvironmentFromScene se llama arriba,
    // ANTES del shadow pass, asi los cambios en cascadas/lambda surten
    // efecto en el mismo frame. Removido de aca.

    // F2H58 C: resolver la LUT del frame. Si color grading esta off, no
    // resolvemos (m_currentLutTextureId queda 0 -> pass skipea). Si esta
    // ON con path vacio, lazy-create identity LUT. Si hay path, carga
    // via AssetManager (que ya tiene cache propio path->id).
    m_currentLutTextureId = 0;
    if (m_colorGradingEnabled) {
        if (m_colorGradingLutPath.empty()) {
            synthesizeIdentityLut();
            m_currentLutTextureId = m_identityLutId;
        } else {
            const TextureAssetId tid = assets.loadTexture(m_colorGradingLutPath);
            if (tid != assets.missingTextureId()) {
                ITexture* tex = assets.getTexture(tid);
                if (tex != nullptr) {
                    m_currentLutTextureId = static_cast<u32>(
                        reinterpret_cast<uintptr_t>(tex->handle()));
                }
            }
            // Fallback: si el path no resuelve, usar identity para no
            // pintar negro.
            if (m_currentLutTextureId == 0) {
                synthesizeIdentityLut();
                m_currentLutTextureId = m_identityLutId;
            }
        }
    }

    LightFrameData lights;
    {
        MOOD_PROFILE_SCOPE("LightSystem::buildFrameData");
        lights = m_lightSystem->buildFrameData(scene);
    }

    // Hito 18: Forward+ light grid.
    const u32 fbW = m_sceneFb->width();
    const u32 fbH = m_sceneFb->height();
    {
        MOOD_PROFILE_SCOPE("LightGrid::compute");
        m_lightGrid->compute(lights, view, projection, fbW, fbH);
    }

    // Log diagnostico one-shot al cambiar la cantidad de luces.
    const u32 pcount = static_cast<u32>(lights.pointLights.size());
    if (pcount != m_lastLoggedPointLightCount) {
        const u32 totalAssign = m_lightGrid->totalAssignments();
        u32 nonEmpty = 0;
        for (const auto& t : m_lightGrid->tileData()) if (t.count > 0) ++nonEmpty;
        const f32 avgPerNonEmpty = nonEmpty > 0
            ? static_cast<f32>(totalAssign) / static_cast<f32>(nonEmpty)
            : 0.0f;
        Log::render()->info(
            "LightGrid: {} point lights -> {} tiles ({}x{}), {} no-vacios, "
            "{} asignaciones (avg {:.2f}/tile)",
            pcount, m_lightGrid->tileCount(),
            m_lightGrid->tilesX(), m_lightGrid->tilesY(),
            nonEmpty, totalAssign, avgPerNonEmpty);
        m_lastLoggedPointLightCount = pcount;
    }

    // SSBO 2: point light data std430.
    struct PointLightStd430 {
        f32 posX, posY, posZ; f32 _pad0;
        f32 colR, colG, colB; f32 intensity;
        f32 radius; f32 _pad1, _pad2, _pad3;
    };
    static_assert(sizeof(PointLightStd430) == 48,
                  "PointLight std430 layout mismatch con el shader");
    std::vector<PointLightStd430> packed;
    packed.reserve(lights.pointLights.size());
    for (const auto& p : lights.pointLights) {
        PointLightStd430 e{};
        e.posX = p.position.x; e.posY = p.position.y; e.posZ = p.position.z;
        e.colR = p.color.x;    e.colG = p.color.y;    e.colB = p.color.z;
        e.intensity = p.intensity;
        e.radius    = p.radius;
        packed.push_back(e);
    }
    if (packed.empty()) packed.push_back(PointLightStd430{});
    m_pointLightsSsbo->upload(packed.data(),
        static_cast<GLsizeiptr>(packed.size() * sizeof(PointLightStd430)));
    m_pointLightsSsbo->bind(2);

    // SSBO 3: tile data.
    const auto& tileData = m_lightGrid->tileData();
    if (tileData.empty()) {
        LightTileData zero{0u, 0u};
        m_lightTilesSsbo->upload(&zero, sizeof(LightTileData));
    } else {
        m_lightTilesSsbo->upload(tileData.data(),
            static_cast<GLsizeiptr>(tileData.size() * sizeof(LightTileData)));
    }
    m_lightTilesSsbo->bind(3);

    // SSBO 4: light indices.
    const auto& indices = m_lightGrid->lightIndices();
    if (indices.empty()) {
        const u32 zero = 0u;
        m_lightIndicesSsbo->upload(&zero, sizeof(u32));
    } else {
        m_lightIndicesSsbo->upload(indices.data(),
            static_cast<GLsizeiptr>(indices.size() * sizeof(u32)));
    }
    m_lightIndicesSsbo->bind(4);

    // IBL + shadow map: bindeos UNA VEZ (global GL state).
    const bool iblOk = (m_iblIrradiance && m_iblPrefilter && m_iblBrdfLut);
    const f32 prefilterMaxLod = iblOk
        ? static_cast<f32>(m_iblPrefilter->mipLevels() - 1)
        : 0.0f;
    if (iblOk) {
        m_iblIrradiance->bind(4);
        m_iblPrefilter->bind(5);
        m_iblBrdfLut->bind(6);
    }
    // F2H60 polish: bindeamos el shadow map array SIEMPRE (no solo si
    // shadowEnabled). Razon: el shader declara `uniform sampler2DArray
    // Shadow uShadowMap` y GL valida los samplers de la draw call aunque
    // el shader NO los samplee en el branch dinamico (uShadowEnabled==0
    // fast path en sampleShadow). Sin bindeo, la texture unit 1 queda
    // con texture id 0 -> warning GL "texture object 0 ... non-depth
    // format ... sampled with shadow sampler".
    if (m_shadowPass) {
        m_shadowPass->bindShadowMap(1);
        m_shadowPass->bindShadowColorMap(8);  // F2H64: sombras tintadas
    }

    // Lambda con TODOS los uniforms del shader de escena.
    auto applyShaderUniforms = [&](IShader& sh) {
        sh.bind();
        sh.setMat4("uView", view);
        sh.setMat4("uProjection", projection);
        sh.setInt("uAlbedoMap",         0);
        sh.setInt("uShadowMap",         1);
        sh.setInt("uMetallicRoughness", 2);
        sh.setInt("uAoMap",             3);
        sh.setInt("uIrradianceMap",     4);
        sh.setInt("uPrefilterMap",      5);
        sh.setInt("uBrdfLut",           6);
        // F2H63: el translucent pass bindea el backbuffer copy a unit 7
        // y setea uScreenSize. Para el pase opaco normal uBlendMode=0 y
        // estos uniforms no se leen — pero igual seteamos defaults para
        // que la sampler unit no quede sin asignar (UB del driver).
        sh.setInt  ("uBackbufferCopy",  7);
        sh.setInt  ("uShadowColorMap",  8);  // F2H64: sombras tintadas
        sh.setVec2 ("uScreenSize",
                    glm::vec2(static_cast<f32>(fbW), static_cast<f32>(fbH)));
        m_lightSystem->bindUniforms(sh, lights, cameraPos);

        sh.setInt("uTileSize", static_cast<int>(k_LightGridTileSize));
        sh.setInt("uTilesX",   static_cast<int>(m_lightGrid->tilesX()));
        sh.setInt("uTilesY",   static_cast<int>(m_lightGrid->tilesY()));

        sh.setInt  ("uIblEnabled",      iblOk ? 1 : 0);
        sh.setFloat("uPrefilterMaxLod", prefilterMaxLod);
        sh.setFloat("uIblIntensity",    m_iblIntensity);

        // F2H60: CSM. uShadowMap pasa de sampler2DShadow a sampler2DArray
        // Shadow. El array de matrices `uLightSpaces[kMaxCsmCascades]` +
        // splits view-space `uCascadeSplits` permiten al shader elegir
        // cascada por fragment.
        sh.setInt  ("uShadowEnabled",  shadowEnabled ? 1 : 0);
        // F2H60 polish iter4: bias bajado de 0.005 a 0.0015. El bias
        // antiguo era ~0.25m de "lift" en NDC depth -- visible peter-
        // panning al apoyar un brush sobre el piso (la sombra
        // "desaparecia" porque el bias hacia que el receiver pareciera
        // estar mas cerca de la luz que el caster). 0.0015 + el
        // biasScale por cascada del shader (x1, x2, x3, x4) es
        // suficiente para evitar acne sin despegar sombras cercanas.
        sh.setFloat("uShadowBias",     0.0015f);
        const u32 csmCount = (shadowEnabled && m_shadowPass)
            ? m_shadowPass->cascadeCount() : 1;
        sh.setInt("uCascadeCount", static_cast<int>(csmCount));
        for (u32 i = 0; i < kMaxCsmCascades; ++i) {
            const std::string lsKey = "uLightSpaces[" + std::to_string(i) + "]";
            sh.setMat4(lsKey.c_str(),
                shadowEnabled && m_shadowPass
                    ? m_shadowPass->lightSpaceMatrix(i)
                    : glm::mat4(1.0f));
        }
        for (u32 i = 0; i <= kMaxCsmCascades; ++i) {
            const std::string spKey = "uCascadeSplits[" + std::to_string(i) + "]";
            sh.setFloat(spKey.c_str(),
                shadowEnabled && m_shadowPass
                    ? m_shadowPass->cascadeSplit(i)
                    : 0.0f);
        }

        sh.setInt  ("uFogMode",    static_cast<int>(m_fog.mode));
        sh.setVec3 ("uFogColor",   m_fog.color);
        sh.setFloat("uFogDensity", m_fog.density);
        sh.setFloat("uFogStart",   m_fog.linearStart);
        sh.setFloat("uFogEnd",     m_fog.linearEnd);
    };

    // Texturas dummy para los slots no usados.
    ITexture* dummyTex = assets.getTexture(assets.missingTextureId());

    auto drawMeshRenderer = [&](IShader& defaultSh,
                                 MeshRendererComponent& mr,
                                 const glm::mat4& model) {
        MeshAsset* asset = assets.getMesh(mr.mesh);
        if (asset == nullptr) return;
        for (usize i = 0; i < asset->submeshes.size(); ++i) {
            const auto& sub = asset->submeshes[i];
            if (sub.mesh == nullptr) continue;
            // F2H67: sub-mesh selector. Si la entity pidio uno especifico,
            // skipear el resto (case-sensitive exact match).
            if (!mr.subMeshName.empty() && sub.name != mr.subMeshName) {
                continue;
            }

            const MaterialAssetId matId =
                mr.materialOrMissing(sub.materialIndex);
            const MaterialAsset* mat = assets.getMaterial(matId);

            // F2H62 Bloque E: si el material tiene shaderGraphPath, pedimos
            // al cache un IShader compilado. Si lo da, lo usamos en lugar
            // del defaultSh para este submesh; sino, fallback transparente.
            // El cache reusa el program compilado entre frames; solo
            // recompila si el hash del GLSL cambia (dev edito el grafo).
            IShader* sh = &defaultSh;
            bool usingGraph = false;
            if (mat != nullptr && !mat->shaderGraphPath.empty() &&
                m_shaderGraphCache != nullptr) {
                IShader* gSh = m_shaderGraphCache->getOrCompile(
                    mat->shaderGraphPath, assets);
                if (gSh != nullptr) {
                    sh = gSh;
                    usingGraph = true;
                    // Rebind program + uniforms (estabamos en defaultSh).
                    applyShaderUniforms(*sh);
                    sh->setMat4("uModel", model);
                    sh->setFloat("uTime", m_currentTime);
                }
            }

            // useAlbedoMap distingue "tint puro" (gold/plastic, false) de
            // "samplear textura" (true). El default material tiene
            // useAlbedoMap=true con albedo=0 => muestra missing.png como
            // warning visible.
            const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
            glActiveTexture(GL_TEXTURE0);
            assets.getTexture(hasAlbedo ? mat->albedo : 0)->bind(0);
            sh->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

            const bool hasMR =
                (mat != nullptr && mat->metallicRoughness != 0);
            glActiveTexture(GL_TEXTURE2);
            if (hasMR) {
                assets.getTexture(mat->metallicRoughness)->bind(2);
            } else {
                dummyTex->bind(2);
            }
            sh->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

            const bool hasAo = (mat != nullptr && mat->ao != 0);
            glActiveTexture(GL_TEXTURE3);
            if (hasAo) {
                assets.getTexture(mat->ao)->bind(3);
            } else {
                dummyTex->bind(3);
            }
            sh->setInt("uHasAoMap", hasAo ? 1 : 0);

            if (mat != nullptr) {
                sh->setVec3 ("uAlbedoTint",   mat->albedoTint);
                sh->setFloat("uMetallicMult", mat->metallicMult);
                sh->setFloat("uRoughnessMult",mat->roughnessMult);
                sh->setFloat("uAoMult",       mat->aoMult);
                // F2H63: blending per-material. uBlendMode=0 (Opaque) hace
                // que el shader use el path tradicional — los otros tres
                // uniforms quedan sin efecto.
                sh->setInt  ("uBlendMode",          static_cast<int>(mat->blendMode));
                sh->setFloat("uOpacity",            mat->opacity);
                sh->setFloat("uIor",                mat->ior);
                sh->setFloat("uRefractionStrength", mat->refractionStrength);
            } else {
                sh->setVec3 ("uAlbedoTint",   glm::vec3(1.0f));
                sh->setFloat("uMetallicMult", 0.0f);
                sh->setFloat("uRoughnessMult",0.5f);
                sh->setFloat("uAoMult",       1.0f);
                sh->setInt  ("uBlendMode",          0);   // Opaque default
                sh->setFloat("uOpacity",            1.0f);
                sh->setFloat("uIor",                1.0f);
                sh->setFloat("uRefractionStrength", 0.0f);
            }

            glActiveTexture(GL_TEXTURE0);
            m_renderer->drawMesh(*sub.mesh, *sh);

            // Si swappeamos al graph shader, volvemos a bindear el
            // defaultSh para no afectar el siguiente submesh o entity
            // del loop externo (que asume defaultSh activo).
            if (usingGraph) {
                applyShaderUniforms(defaultSh);
            }
        }
    };

    // F2H3 + F2H4: agrupamos las entidades estaticas por (mesh, material)
    // y dibujamos los batches con `glDrawArraysInstanced`. Las que no
    // pasan el cull se descartan; las que no son batcheables (multi-submesh
    // o materiales mixtos) caen al path no-instanced del fallback.
    const Frustum frustum = frustumFromViewProj(projection * view);
    BatchingResult batching = groupByBatch(scene, assets, frustum, cameraPos);
    u32 emittedBatches = 0;
    u32 emittedInstances = 0;
    u32 instancesLod0 = 0, instancesLod1 = 0, instancesLod2 = 0;

    // Pase A.1: batches instanced.
    if (!batching.batches.empty()) {
        MOOD_PROFILE_SCOPE("PBR::instancedPass");
        applyShaderUniforms(*m_pbrInstancedShader);
        for (auto& [key, batch] : batching.batches) {
            MeshAsset* asset = assets.getMesh(key.mesh);
            if (asset == nullptr) continue;
            // F2H6: usa el LOD pedido por la BatchKey con fallback automatico
            // a LOD 0 si el nivel no fue generado (mesh muy chico, skinned,
            // o cache invalido).
            const auto& lodSubmeshes = asset->submeshesForLod(key.lod);
            if (lodSubmeshes.empty()) continue;
            const auto& sub = lodSubmeshes.front();
            if (sub.mesh == nullptr) continue;

            // Bindeo de texturas + uniforms del material (mismo flow que
            // drawMeshRenderer, pero usando la material id de la BatchKey
            // en lugar de la del MeshRenderer — son equivalentes para
            // entidades batcheables porque tienen 1 submesh).
            const MaterialAsset* mat = assets.getMaterial(key.material);
            const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
            glActiveTexture(GL_TEXTURE0);
            assets.getTexture(hasAlbedo ? mat->albedo : 0)->bind(0);
            m_pbrInstancedShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

            const bool hasMR = (mat != nullptr && mat->metallicRoughness != 0);
            glActiveTexture(GL_TEXTURE2);
            if (hasMR) assets.getTexture(mat->metallicRoughness)->bind(2);
            else       dummyTex->bind(2);
            m_pbrInstancedShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

            const bool hasAo = (mat != nullptr && mat->ao != 0);
            glActiveTexture(GL_TEXTURE3);
            if (hasAo) assets.getTexture(mat->ao)->bind(3);
            else       dummyTex->bind(3);
            m_pbrInstancedShader->setInt("uHasAoMap", hasAo ? 1 : 0);

            if (mat != nullptr) {
                m_pbrInstancedShader->setVec3 ("uAlbedoTint",    mat->albedoTint);
                m_pbrInstancedShader->setFloat("uMetallicMult",  mat->metallicMult);
                m_pbrInstancedShader->setFloat("uRoughnessMult", mat->roughnessMult);
                m_pbrInstancedShader->setFloat("uAoMult",        mat->aoMult);
            } else {
                m_pbrInstancedShader->setVec3 ("uAlbedoTint",    glm::vec3(1.0f));
                m_pbrInstancedShader->setFloat("uMetallicMult",  0.0f);
                m_pbrInstancedShader->setFloat("uRoughnessMult", 0.5f);
                m_pbrInstancedShader->setFloat("uAoMult",        1.0f);
            }

            // Subir matrices model + bindear atributos de instancia. El
            // bind del mesh-VAO ocurre dentro de drawMeshInstanced (via
            // `mesh.bind()`), pero el VAO necesita el VBO de instancias
            // bindeado AHORA para que `glVertexAttribPointer` funcione
            // sobre el VBO correcto.
            sub.mesh->bind();
            const u32 sizeBytes = static_cast<u32>(
                batch.models.size() * sizeof(glm::mat4));
            m_instanceBuffer->upload(batch.models.data(), sizeBytes);
            m_instanceBuffer->bindAsAttributeMat4(/*startLocation=*/5);

            glActiveTexture(GL_TEXTURE0);
            m_renderer->drawMeshInstanced(*sub.mesh, *m_pbrInstancedShader,
                                            static_cast<u32>(batch.models.size()));
            ++emittedBatches;
            const u32 batchSize = static_cast<u32>(batch.models.size());
            emittedInstances += batchSize;
            // F2H6: distribucion de instancias por LOD (debug Tracy).
            if (key.lod == 0)      instancesLod0 += batchSize;
            else if (key.lod == 1) instancesLod1 += batchSize;
            else                   instancesLod2 += batchSize;
        }
    }

    // Pase A.2: fallback no-instanced para entidades non-batchable
    // (multi-submesh o materiales mixtos). Mismo path que el F2H3 puro
    // pero solo sobre la lista que ya pre-cullada el helper.
    if (!batching.nonBatchable.empty()) {
        MOOD_PROFILE_SCOPE("PBR::staticPass");
        applyShaderUniforms(*m_pbrShader);
        for (Entity e : batching.nonBatchable) {
            if (!e.hasComponent<TransformComponent>() ||
                !e.hasComponent<MeshRendererComponent>()) continue;
            auto& t = e.getComponent<TransformComponent>();
            auto& mr = e.getComponent<MeshRendererComponent>();
            const glm::mat4 model = t.worldMatrix();
            m_pbrShader->setMat4("uModel", model);
            drawMeshRenderer(*m_pbrShader, mr, model);
        }
    }

    MOOD_PROFILE_PLOT("PBR::CulledStatic", static_cast<i64>(batching.culledCount));
    MOOD_PROFILE_PLOT("PBR::BatchedDrawCalls", static_cast<i64>(emittedBatches));
    MOOD_PROFILE_PLOT("PBR::Instances", static_cast<i64>(emittedInstances));
    MOOD_PROFILE_PLOT("PBR::InstancesLod0", static_cast<i64>(instancesLod0));
    MOOD_PROFILE_PLOT("PBR::InstancesLod1", static_cast<i64>(instancesLod1));
    MOOD_PROFILE_PLOT("PBR::InstancesLod2", static_cast<i64>(instancesLod2));

    // Pase B: skinneadas. Solo bindear el skinned shader si hay alguna.
    bool hasSkinned = false;
    scene.forEach<SkeletonComponent>(
        [&](Entity, SkeletonComponent&) { hasSkinned = true; });
    if (hasSkinned && m_pbrSkinnedShader) {
        MOOD_PROFILE_SCOPE("PBR::skinnedPass");
        applyShaderUniforms(*m_pbrSkinnedShader);
        scene.forEach<TransformComponent, MeshRendererComponent,
                       SkeletonComponent>(
            [&](Entity e, TransformComponent& t, MeshRendererComponent& mr,
                SkeletonComponent& sk) {
                if (isEntityHiddenByVisGroup(scene, e)) return;  // F2H33
                const glm::mat4 model = t.worldMatrix();
                m_pbrSkinnedShader->setMat4("uModel", model);
                const usize n = sk.skinningMatrices.size();
                if (n == 0) return;
                const usize cap = static_cast<usize>(k_maxBonesPerSkeleton);
                const usize count = (n < cap) ? n : cap;
                for (usize i = 0; i < count; ++i) {
                    std::string name = "uBoneMatrices[" +
                                       std::to_string(i) + "]";
                    m_pbrSkinnedShader->setMat4(name, sk.skinningMatrices[i]);
                }
                // F2H62: drawMeshRenderer va a saltar al graph shader si
                // el material lo pide, pero el path skinned NO usa
                // pbr_skinned.vert para el graph (la v1 del cache solo
                // soporta pbr.vert estatico). Resultado: skeletal +
                // shaderGraphPath cae al fallback PBR — esperado.
                drawMeshRenderer(*m_pbrSkinnedShader, mr, model);
            });
    }

    // F2H11: pase de brushes CSG. Cada entidad con BrushComponent +
    // TransformComponent se dibuja con su mesh propia (sin instancing
    // — los brushes son pocos y editables; la mesh unificada con
    // weld + cull caras internas viene en F2H16). Si el brush esta
    // dirty, regeneramos la mesh aqui mismo. Reusa el shader PBR
    // del pase no-instanced y el material del componente.
    {
        bool hasAnyBrush = false;
        scene.forEach<BrushComponent>([&](Entity, BrushComponent&) {
            hasAnyBrush = true;
        });
        if (hasAnyBrush) {
            MOOD_PROFILE_SCOPE("PBR::brushPass");
            applyShaderUniforms(*m_pbrShader);
            const std::vector<VertexAttribute> kBrushAttrs = {
                {0, 3}, {1, 3}, {2, 2}, {3, 3}  // pos, color, uv, normal
            };
            scene.forEach<TransformComponent, BrushComponent>(
                [&](Entity e, TransformComponent& t, BrushComponent& bc) {
                    if (isEntityHiddenByVisGroup(scene, e)) return;  // F2H33
                    const glm::mat4 worldMatrix = t.worldMatrix();
                    // F2H15: si alguna cara tiene lockToWorld=true,
                    // el rebuild de mesh debe hacerse cada vez que
                    // el transform cambia (las UVs world dependen
                    // de la posicion world). Detectar via cache.
                    bool transformChangedForLock = false;
                    if (bc.anyFaceLockToWorld) {
                        const glm::mat4& last = bc.lastWorldMatrix;
                        for (int col = 0; col < 4 && !transformChangedForLock; ++col) {
                            for (int row = 0; row < 4 && !transformChangedForLock; ++row) {
                                if (std::fabs(worldMatrix[col][row] - last[col][row])
                                    > Mood::kPlaneEpsilon) {
                                    transformChangedForLock = true;
                                }
                            }
                        }
                    }

                    if (bc.dirty || transformChangedForLock ||
                        bc.meshCache.empty()) {
                        const Csg::BrushMeshData data =
                            Csg::buildBrushMesh(bc.brush, worldMatrix);
                        if (data.submeshes.empty()) {
                            bc.dirty = false;
                            bc.meshCache.clear();
                            return;  // brush degenerado: nada que dibujar
                        }
                        // Recompute cache de lock-to-world tras la rebuild.
                        bc.anyFaceLockToWorld = false;
                        for (const auto& f : bc.brush.faces) {
                            if (f.lockToWorld) {
                                bc.anyFaceLockToWorld = true;
                                break;
                            }
                        }
                        bc.lastWorldMatrix = worldMatrix;
                        // F2H17: 1 IMesh por submesh (1 por slot
                        // distinto). Limpiar cache previo y crear N.
                        bc.meshCache.clear();
                        bc.meshCacheSlots.clear();
                        bc.meshCache.reserve(data.submeshes.size());
                        bc.meshCacheSlots.reserve(data.submeshes.size());
                        for (const auto& sub : data.submeshes) {
                            const std::vector<f32> verts =
                                Csg::brushSubmeshToInterleaved(sub);
                            bc.meshCache.push_back(
                                assets.createDynamicMesh(verts, kBrushAttrs));
                            bc.meshCacheSlots.push_back(sub.materialSlot);
                        }
                        bc.dirty = false;
                    }
                    if (bc.meshCache.empty()) return;

                    m_pbrShader->setMat4("uModel", worldMatrix);

                    // F2H17: 1 draw call por submesh. meshCacheSlots[i]
                    // da el slot de bc.materials para bc.meshCache[i].
                    for (usize si = 0; si < bc.meshCache.size() &&
                                        si < bc.meshCacheSlots.size(); ++si) {
                        const u32 slot = bc.meshCacheSlots[si];
                        const MaterialAssetId matId =
                            (slot < bc.materials.size())
                                ? bc.materials[slot] : 0u;
                        const bool useBlankLook = (matId == 0);
                        const MaterialAsset* mat = useBlankLook
                            ? nullptr : assets.getMaterial(matId);

                        const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
                        glActiveTexture(GL_TEXTURE0);
                        if (hasAlbedo) assets.getTexture(mat->albedo)->bind(0);
                        else           dummyTex->bind(0);
                        m_pbrShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

                        const bool hasMR = (mat != nullptr && mat->metallicRoughness != 0);
                        glActiveTexture(GL_TEXTURE2);
                        if (hasMR) assets.getTexture(mat->metallicRoughness)->bind(2);
                        else       dummyTex->bind(2);
                        m_pbrShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

                        const bool hasAo = (mat != nullptr && mat->ao != 0);
                        glActiveTexture(GL_TEXTURE3);
                        if (hasAo) assets.getTexture(mat->ao)->bind(3);
                        else       dummyTex->bind(3);
                        m_pbrShader->setInt("uHasAoMap", hasAo ? 1 : 0);

                        if (mat != nullptr) {
                            m_pbrShader->setVec3 ("uAlbedoTint",   mat->albedoTint);
                            m_pbrShader->setFloat("uMetallicMult", mat->metallicMult);
                            m_pbrShader->setFloat("uRoughnessMult",mat->roughnessMult);
                            m_pbrShader->setFloat("uAoMult",       mat->aoMult);
                        } else {
                            m_pbrShader->setVec3 ("uAlbedoTint",   glm::vec3(0.7f));
                            m_pbrShader->setFloat("uMetallicMult", 0.0f);
                            m_pbrShader->setFloat("uRoughnessMult",0.85f);
                            m_pbrShader->setFloat("uAoMult",       1.0f);
                        }

                        glActiveTexture(GL_TEXTURE0);
                        if (bc.meshCache[si]) {
                            m_renderer->drawMesh(*bc.meshCache[si], *m_pbrShader);
                        }
                    }
                });
        }
    }

    // F2H26: pase de mesh compilada del mapa. Solo se dibuja en
    // MoodPlayer (el editor sigue usando BrushComponent). 1 IMesh por
    // submesh; 1 draw call por submesh con su material correspondiente.
    // La mesh esta en world space — uModel = identity. No hay rebuild
    // dynamic ni dirty flag (la mesh es estatica desde el load).
    {
        bool hasAnyCompiled = false;
        scene.forEach<CompiledMeshComponent>([&](Entity, CompiledMeshComponent&) {
            hasAnyCompiled = true;
        });
        if (hasAnyCompiled) {
            MOOD_PROFILE_SCOPE("PBR::compiledMeshPass");
            applyShaderUniforms(*m_pbrShader);
            const glm::mat4 identity(1.0f);
            scene.forEach<CompiledMeshComponent>(
                [&](Entity, CompiledMeshComponent& cm) {
                    if (cm.meshes.empty()) return;
                    m_pbrShader->setMat4("uModel", identity);
                    for (usize si = 0; si < cm.meshes.size(); ++si) {
                        const MaterialAssetId matId =
                            (si < cm.materials.size()) ? cm.materials[si] : 0u;
                        const bool useBlankLook = (matId == 0);
                        const MaterialAsset* mat = useBlankLook
                            ? nullptr : assets.getMaterial(matId);

                        const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
                        glActiveTexture(GL_TEXTURE0);
                        if (hasAlbedo) assets.getTexture(mat->albedo)->bind(0);
                        else           dummyTex->bind(0);
                        m_pbrShader->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

                        const bool hasMR = (mat != nullptr && mat->metallicRoughness != 0);
                        glActiveTexture(GL_TEXTURE2);
                        if (hasMR) assets.getTexture(mat->metallicRoughness)->bind(2);
                        else       dummyTex->bind(2);
                        m_pbrShader->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

                        const bool hasAo = (mat != nullptr && mat->ao != 0);
                        glActiveTexture(GL_TEXTURE3);
                        if (hasAo) assets.getTexture(mat->ao)->bind(3);
                        else       dummyTex->bind(3);
                        m_pbrShader->setInt("uHasAoMap", hasAo ? 1 : 0);

                        if (mat != nullptr) {
                            m_pbrShader->setVec3 ("uAlbedoTint",   mat->albedoTint);
                            m_pbrShader->setFloat("uMetallicMult", mat->metallicMult);
                            m_pbrShader->setFloat("uRoughnessMult",mat->roughnessMult);
                            m_pbrShader->setFloat("uAoMult",       mat->aoMult);
                        } else {
                            m_pbrShader->setVec3 ("uAlbedoTint",   glm::vec3(0.7f));
                            m_pbrShader->setFloat("uMetallicMult", 0.0f);
                            m_pbrShader->setFloat("uRoughnessMult",0.85f);
                            m_pbrShader->setFloat("uAoMult",       1.0f);
                        }

                        glActiveTexture(GL_TEXTURE0);
                        if (cm.meshes[si]) {
                            m_renderer->drawMesh(*cm.meshes[si], *m_pbrShader);
                        }
                    }
                });
        }
    }

    // F2H64: pase OIT (Order-Independent Transparency Weighted Blended,
    // McGuire & Bavoil 2013). Reemplaza el sort back-to-front de F2H63
    // por un algoritmo de acumulacion ponderada que no depende del orden
    // de iteracion. Va DESPUES del opaco + skybox + brushes + compiledMesh,
    // ANTES de particulas y debug.
    //
    // Pipeline:
    //   1. Blit del color del scene FB al backbuffer copy (F2H63 reusa esto
    //      para refraccion screen-space dentro del frag shader).
    //   2. Bind m_oitAccumFb (2 color attachments + depth compartido con
    //      m_sceneFb). Clear: accum=(0,0,0,0), revealage=(1,1,1,1).
    //   3. Blend separado per-attachment:
    //        loc 0 (accum):     GL_ONE / GL_ONE       (suma pura)
    //        loc 1 (revealage): GL_ZERO / GL_ONE_MINUS_SRC_COLOR (mult)
    //   4. Iterar translucents SIN sort. El shader detecta uOitPass=1 y
    //      escribe la formula McGuire al accum/revealage. Tanto Translucent
    //      como Additive comparten este path.
    //   5. Composite: bind m_sceneFb (solo loc 0 via drawBuffers), bind
    //      oit_composite shader + accum/revealage, fullscreen tri con
    //      blend GL_ONE_MINUS_SRC_ALPHA / GL_SRC_ALPHA. Restaura ambos
    //      attachments del scene FB al salir.
    if (!batching.translucents.empty() && m_oitAccumFb && m_oitCompositeShader) {
        MOOD_PROFILE_SCOPE("PBR::oitPass");

        // 1) Snapshot del color FB pre-translucent al backbuffer copy.
        //    Los shaders translucent lo samplean via uBackbufferCopy
        //    (unit 7) para refraccion screen-space.
        blitSceneToBackbufferCopy();
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_backbufferCopyFb->glColorTextureId());

        // 2) Bind del FB OIT + clear separado por attachment.
        m_oitAccumFb->bind();
        const GLfloat clearAccum[4]    = {0.0f, 0.0f, 0.0f, 0.0f};
        const GLfloat clearRevealage[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearAccum);
        glClearBufferfv(GL_COLOR, 1, clearRevealage);
        // NO clearear el depth: el FB OIT lo comparte con el scene FB y
        // queremos preservar la profundidad de los opacos para depth-test
        // contra ellos.

        // 3) GL state. blend separado por attachment (GL 4.0+).
        glEnable(GL_BLEND);
        glBlendFunci(0, GL_ONE, GL_ONE);                               // accum
        glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);              // revealage
        glDepthMask(GL_FALSE);  // OIT no escribe depth (los opacos ya lo hicieron)
        glEnable(GL_DEPTH_TEST);

        // 4) Iterar translucents SIN sort (algoritmo order-independent).
        applyShaderUniforms(*m_pbrShader);
        m_pbrShader->setInt("uOitPass", 1);
        for (const TranslucentDraw& td : batching.translucents) {
            Entity e = td.entity;
            if (!e.hasComponent<TransformComponent>() ||
                !e.hasComponent<MeshRendererComponent>()) continue;
            auto& t  = e.getComponent<TransformComponent>();
            auto& mr = e.getComponent<MeshRendererComponent>();
            const glm::mat4 model = t.worldMatrix();
            m_pbrShader->setMat4("uModel", model);
            drawMeshRenderer(*m_pbrShader, mr, model);
        }
        // Reset uOitPass para no contaminar el resto del frame (opaque
        // path del frame siguiente).
        m_pbrShader->setInt("uOitPass", 0);

        // Restaurar GL state base antes del composite.
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // default
        glDepthMask(GL_TRUE);

        // 5) Composite pass. Volver al scene FB, ESCRIBIR SOLO LOC 0
        //    (no pisamos las normales de los opacos en loc 1).
        m_sceneFb->bind();
        const GLenum compositeDrawBufs[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, compositeDrawBufs);

        // Blend del composite: dst = dst * revealage + src * (1 - revealage).
        // src.a viene como (1 - revealage_acumulado) del shader, asi que el
        // blend ONE_MINUS_SRC_ALPHA / SRC_ALPHA hace exactamente eso.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        m_oitCompositeShader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_oitAccumFb->glAccumTextureId());
        m_oitCompositeShader->setInt("uAccum", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_oitAccumFb->glRevealageTextureId());
        m_oitCompositeShader->setInt("uRevealage", 1);

        glBindVertexArray(m_oitCompositeVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Restaurar drawBuffers del scene FB para que los pases siguientes
        // (particulas, debug, post-process) escriban a ambos attachments
        // si el shader lo hace.
        const GLenum sceneDrawBufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, sceneDrawBufs);

        // Restaurar GL state estandar.
        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
    }

    // Hito 29 Bloque 2: pase de particulas. Va DESPUES de la geometria
    // opaca (PBR estatico + skinneado) para que las particulas semi-
    // transparentes se ocluyan por el mundo, pero ANTES del debug
    // renderer (que dibuja con depth test pero sin blend, asi queda
    // arriba de todo). Depth write OFF dentro del renderer para que las
    // particulas no se descarten entre si.
    if (m_particleRenderer) {
        MOOD_PROFILE_SCOPE("ParticleRenderer::render");
        m_particleRenderer->render(scene, assets, view, projection);
    }

    // Aqui retornamos con el scene FB todavia bindeado y el RHI dentro
    // del beginFrame/endFrame. El caller puede ahora agregar geometria
    // al m_debugRenderer; cuando llame a `endFrame()`, esta clase la
    // flushea, cierra el frame y aplica post-process.
}

} // namespace Mood
