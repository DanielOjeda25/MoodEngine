// Implementacion de SceneRenderer (Hito 21 Bloque 2). Extraido de
// `EditorApplication::renderSceneToViewport` (que vivia en
// `editor/EditorRenderPass.cpp`). Sin cambios de logica — solo se
// removieron los bits editor-specificos (tile picking, OBB de seleccion,
// drop highlights, debug F1 de tiles); esos viven ahora en
// `editor/EditorSceneOverlay.cpp` y se agregan via `debugRenderer()`
// entre `renderScene` y `endFrame`.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "engine/animation/Skeleton.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/pipeline/LightGrid.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/RendererTypes.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLParticleRenderer.h"
#include "engine/render/backend/opengl/OpenGLRenderer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/LightSystem.h"
#include "systems/PostProcessPass.h"
#include "systems/ShadowPass.h"
#include "systems/SkyboxRenderer.h"

#include <glad/gl.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace Mood {

SceneRenderer::SceneRenderer()
    : m_tonemap(TonemapMode{2}) // ACES por default
{
    // --- RHI + recursos del viewport offscreen ---
    m_renderer = std::make_unique<OpenGLRenderer>();
    m_renderer->init();

    // Hito 15 Bloque 3: dos framebuffers para el viewport.
    //   - `m_sceneFb`: HDR (RGBA16F). Donde se pinta sky + escena + lit + fog.
    //   - `m_viewportFb`: LDR (RGBA8). Resultado del post-process pass.
    m_sceneFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    m_viewportFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::LDR);

    // PBR (Cook-Torrance + metallic-roughness) — Hito 17.
    m_pbrShader = std::make_unique<OpenGLShader>(
        "shaders/pbr.vert", "shaders/pbr.frag");
    // Skinned vertex shader, mismo .frag — Hito 19.
    m_pbrSkinnedShader = std::make_unique<OpenGLShader>(
        "shaders/pbr_skinned.vert", "shaders/pbr.frag");

    m_debugRenderer = std::make_unique<OpenGLDebugRenderer>();

    // Skybox (Hito 15). Tolera fallo de carga.
    try {
        m_skyboxRenderer =
            std::make_unique<SkyboxRenderer>(
                SkyboxRenderer::Equirect{},
                "assets/skyboxes/sky_kloofendal.png");
    } catch (const std::exception& e) {
        Log::render()->warn("SkyboxRenderer no disponible: {}. Sky fallback al clear color.",
                             e.what());
        m_skyboxRenderer.reset();
    }

    // IBL (Hito 17 Bloque 3). Si alguno falta, el shader cae a uAmbient.
    // Hito 26 G: bakeado desde el equirect kloofendal (mismo cielo que
    // muestra el SkyboxRenderer en modo Equirect), asi las esferas PBR
    // reflejan lo que el dev ve. Antes apuntaba a `sky_day` cubemap, lo
    // cual produciá mismatch visual.
    try {
        const std::string base = "assets/ibl/sky_kloofendal";
        std::array<std::string, 6> irrPaths{
            base + "/irradiance/px.png", base + "/irradiance/nx.png",
            base + "/irradiance/py.png", base + "/irradiance/ny.png",
            base + "/irradiance/pz.png", base + "/irradiance/nz.png"};
        m_iblIrradiance = std::make_unique<OpenGLCubemapTexture>(irrPaths);

        std::vector<std::array<std::string, 6>> prefilterMips;
        for (int mip = 0; mip < 5; ++mip) {
            const std::string m = base + "/prefilter/mip_" + std::to_string(mip);
            prefilterMips.push_back({
                m + "/px.png", m + "/nx.png",
                m + "/py.png", m + "/ny.png",
                m + "/pz.png", m + "/nz.png"});
        }
        m_iblPrefilter = std::make_unique<OpenGLCubemapTexture>(prefilterMips);

        m_iblBrdfLut = std::make_unique<OpenGLTexture>("assets/ibl/brdf_lut.png");

        Log::render()->info(
            "IBL cargado: irradiance + prefilter (5 mips) + BRDF LUT.");
    } catch (const std::exception& e) {
        Log::render()->warn("IBL no disponible: {}. Cae al ambient escalar.",
                             e.what());
        m_iblIrradiance.reset();
        m_iblPrefilter.reset();
        m_iblBrdfLut.reset();
    }

    m_postProcess = std::make_unique<PostProcessPass>();

    // Shadow pass (Hito 16).
    try {
        m_shadowPass = std::make_unique<ShadowPass>(2048);
    } catch (const std::exception& e) {
        Log::render()->warn("ShadowPass no disponible: {}. Sombras off.",
                             e.what());
        m_shadowPass.reset();
    }

    // Forward+ light grid (Hito 18). 3 SSBOs vacios al arranque; se
    // llenan cada frame en `renderScene`.
    m_lightGrid        = std::make_unique<LightGrid>();
    m_pointLightsSsbo  = std::make_unique<OpenGLSSBO>();
    m_lightTilesSsbo   = std::make_unique<OpenGLSSBO>();
    m_lightIndicesSsbo = std::make_unique<OpenGLSSBO>();

    m_lightSystem = std::make_unique<LightSystem>();

    // Particle system renderer (Hito 29 Bloque 2). Tolera fallo de
    // shader como el resto: log + null, escena sigue rindiendose sin
    // particulas.
    try {
        m_particleRenderer = std::make_unique<OpenGLParticleRenderer>();
    } catch (const std::exception& e) {
        Log::render()->warn("ParticleRenderer no disponible: {}. Particulas desactivadas.",
                             e.what());
        m_particleRenderer.reset();
    }
}

SceneRenderer::~SceneRenderer() {
    // Mismo orden inverso de destruccion que tenia EditorApplication antes
    // del refactor: recursos GL se tiran ANTES del contexto (que mata el
    // caller).
    m_particleRenderer.reset();
    m_lightSystem.reset();
    m_iblBrdfLut.reset();
    m_iblPrefilter.reset();
    m_iblIrradiance.reset();
    m_skyboxRenderer.reset();
    m_lightIndicesSsbo.reset();
    m_lightTilesSsbo.reset();
    m_pointLightsSsbo.reset();
    m_lightGrid.reset();
    m_shadowPass.reset();
    m_postProcess.reset();
    m_debugRenderer.reset();
    m_pbrSkinnedShader.reset();
    m_pbrShader.reset();
    m_viewportFb.reset();
    m_sceneFb.reset();
    m_renderer.reset();
}

void SceneRenderer::applyEnvironmentFromScene(Scene& scene) {
    // Reset a defaults primero. Sin este reset, abrir un proyecto sin
    // Environment hereda los valores del proyecto anterior.
    m_fog          = FogParams{};
    m_exposure     = 0.0f;
    m_tonemap      = TonemapMode{2}; // ACES
    m_iblIntensity = 1.0f;

    bool envFound = false;
    scene.forEach<EnvironmentComponent>(
        [&](Entity, EnvironmentComponent& env) {
            if (envFound) return; // primer Environment gana
            envFound = true;
            m_fog.mode        = static_cast<FogMode>(env.fogMode);
            m_fog.color       = env.fogColor;
            m_fog.density     = env.fogDensity;
            m_fog.linearStart = env.fogLinearStart;
            m_fog.linearEnd   = env.fogLinearEnd;
            m_exposure        = env.exposure;
            m_tonemap         = static_cast<TonemapMode>(env.tonemapMode);
            m_iblIntensity    = env.iblIntensity;
        });
}

void SceneRenderer::renderScene(Scene& scene,
                                 AssetManager& assets,
                                 const glm::mat4& view,
                                 const glm::mat4& projection,
                                 f32 aspect,
                                 const glm::vec3& cameraPos,
                                 u32 panelWidth,
                                 u32 panelHeight) {
    if (panelWidth > 0 && panelHeight > 0) {
        m_sceneFb->resize(panelWidth, panelHeight);
        m_viewportFb->resize(panelWidth, panelHeight);
    }

    // Hito 13: guardar matrices para que el overlay 2D del ViewportPanel
    // proyecte posiciones al frame siguiente.
    m_lastView = view;
    m_lastProjection = projection;
    m_lastAspect = aspect;

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
        // Bounding sphere fijo amplio para cubrir el mapa + props. Cuando
        // llegue CSM se reemplaza por una serie de cascadas. El centro y
        // radio los fijamos aproximadamente al origen del mundo: el caller
        // no lo sabe (ya no tenemos el GridMap aca) y para escenas tipicas
        // del Hito 20 alcanza.
        const glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
        const f32 sceneRadius = 30.0f;
        m_shadowPass->record(scene, assets, *m_renderer,
                             shadowLightDir, sceneCenter, sceneRadius);
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

    // Skybox primero (depth=1 + LEQUAL).
    if (m_skyboxRenderer) {
        m_skyboxRenderer->draw(view, projection);
    }

    // Render scene-driven (Hito 10 Bloque 4). Lo hacemos sin chequeos
    // de scene null porque el caller ya garantiza scene valida (el
    // parametro es ref).
    applyEnvironmentFromScene(scene);

    LightFrameData lights = m_lightSystem->buildFrameData(scene);

    // Hito 18: Forward+ light grid.
    const u32 fbW = m_sceneFb->width();
    const u32 fbH = m_sceneFb->height();
    m_lightGrid->compute(lights, view, projection, fbW, fbH);

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
    if (shadowEnabled && m_shadowPass) {
        m_shadowPass->bindShadowMap(1);
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
        m_lightSystem->bindUniforms(sh, lights, cameraPos);

        sh.setInt("uTileSize", static_cast<int>(k_LightGridTileSize));
        sh.setInt("uTilesX",   static_cast<int>(m_lightGrid->tilesX()));
        sh.setInt("uTilesY",   static_cast<int>(m_lightGrid->tilesY()));

        sh.setInt  ("uIblEnabled",      iblOk ? 1 : 0);
        sh.setFloat("uPrefilterMaxLod", prefilterMaxLod);
        sh.setFloat("uIblIntensity",    m_iblIntensity);

        sh.setInt  ("uShadowEnabled", shadowEnabled ? 1 : 0);
        sh.setFloat("uShadowBias",    0.005f);
        sh.setMat4 ("uLightSpace",
            shadowEnabled && m_shadowPass
                ? m_shadowPass->lightSpaceMatrix()
                : glm::mat4(1.0f));

        sh.setInt  ("uFogMode",    static_cast<int>(m_fog.mode));
        sh.setVec3 ("uFogColor",   m_fog.color);
        sh.setFloat("uFogDensity", m_fog.density);
        sh.setFloat("uFogStart",   m_fog.linearStart);
        sh.setFloat("uFogEnd",     m_fog.linearEnd);
    };

    // Texturas dummy para los slots no usados.
    ITexture* dummyTex = assets.getTexture(assets.missingTextureId());

    auto drawMeshRenderer = [&](IShader& sh,
                                 MeshRendererComponent& mr) {
        MeshAsset* asset = assets.getMesh(mr.mesh);
        if (asset == nullptr) return;
        for (usize i = 0; i < asset->submeshes.size(); ++i) {
            const auto& sub = asset->submeshes[i];
            if (sub.mesh == nullptr) continue;

            const MaterialAssetId matId =
                mr.materialOrMissing(sub.materialIndex);
            const MaterialAsset* mat = assets.getMaterial(matId);

            // useAlbedoMap distingue "tint puro" (gold/plastic, false) de
            // "samplear textura" (true). El default material tiene
            // useAlbedoMap=true con albedo=0 => muestra missing.png como
            // warning visible.
            const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
            glActiveTexture(GL_TEXTURE0);
            assets.getTexture(hasAlbedo ? mat->albedo : 0)->bind(0);
            sh.setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

            const bool hasMR =
                (mat != nullptr && mat->metallicRoughness != 0);
            glActiveTexture(GL_TEXTURE2);
            if (hasMR) {
                assets.getTexture(mat->metallicRoughness)->bind(2);
            } else {
                dummyTex->bind(2);
            }
            sh.setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

            const bool hasAo = (mat != nullptr && mat->ao != 0);
            glActiveTexture(GL_TEXTURE3);
            if (hasAo) {
                assets.getTexture(mat->ao)->bind(3);
            } else {
                dummyTex->bind(3);
            }
            sh.setInt("uHasAoMap", hasAo ? 1 : 0);

            if (mat != nullptr) {
                sh.setVec3 ("uAlbedoTint",   mat->albedoTint);
                sh.setFloat("uMetallicMult", mat->metallicMult);
                sh.setFloat("uRoughnessMult",mat->roughnessMult);
                sh.setFloat("uAoMult",       mat->aoMult);
            } else {
                sh.setVec3 ("uAlbedoTint",   glm::vec3(1.0f));
                sh.setFloat("uMetallicMult", 0.0f);
                sh.setFloat("uRoughnessMult",0.5f);
                sh.setFloat("uAoMult",       1.0f);
            }

            glActiveTexture(GL_TEXTURE0);
            m_renderer->drawMesh(*sub.mesh, sh);
        }
    };

    // Pase A: estaticos.
    applyShaderUniforms(*m_pbrShader);
    scene.forEach<TransformComponent, MeshRendererComponent>(
        [&](Entity e, TransformComponent& t, MeshRendererComponent& mr) {
            if (e.hasComponent<SkeletonComponent>()) return;
            m_pbrShader->setMat4("uModel", t.worldMatrix());
            drawMeshRenderer(*m_pbrShader, mr);
        });

    // Pase B: skinneadas. Solo bindear el skinned shader si hay alguna.
    bool hasSkinned = false;
    scene.forEach<SkeletonComponent>(
        [&](Entity, SkeletonComponent&) { hasSkinned = true; });
    if (hasSkinned && m_pbrSkinnedShader) {
        applyShaderUniforms(*m_pbrSkinnedShader);
        scene.forEach<TransformComponent, MeshRendererComponent,
                       SkeletonComponent>(
            [&](Entity, TransformComponent& t, MeshRendererComponent& mr,
                SkeletonComponent& sk) {
                m_pbrSkinnedShader->setMat4("uModel", t.worldMatrix());
                const usize n = sk.skinningMatrices.size();
                if (n == 0) return;
                const usize cap = static_cast<usize>(k_maxBonesPerSkeleton);
                const usize count = (n < cap) ? n : cap;
                for (usize i = 0; i < count; ++i) {
                    std::string name = "uBoneMatrices[" +
                                       std::to_string(i) + "]";
                    m_pbrSkinnedShader->setMat4(name, sk.skinningMatrices[i]);
                }
                drawMeshRenderer(*m_pbrSkinnedShader, mr);
            });
    }

    // Hito 29 Bloque 2: pase de particulas. Va DESPUES de la geometria
    // opaca (PBR estatico + skinneado) para que las particulas semi-
    // transparentes se ocluyan por el mundo, pero ANTES del debug
    // renderer (que dibuja con depth test pero sin blend, asi queda
    // arriba de todo). Depth write OFF dentro del renderer para que las
    // particulas no se descarten entre si.
    if (m_particleRenderer) {
        m_particleRenderer->render(scene, assets, view, projection);
    }

    // Aqui retornamos con el scene FB todavia bindeado y el RHI dentro
    // del beginFrame/endFrame. El caller puede ahora agregar geometria
    // al m_debugRenderer; cuando llame a `endFrame()`, esta clase la
    // flushea, cierra el frame y aplica post-process.
}

void SceneRenderer::endFrame() {
    // Si el caller acumulo geometria de debug entre `renderScene` y
    // `endFrame`, la flusheamos aqui usando las matrices del frame.
    // Cuando no hay debug pendiente el flush es no-op (el renderer no
    // ejecuta vertex array vacio).
    m_debugRenderer->flush(m_lastView, m_lastProjection);

    m_renderer->endFrame();
    m_sceneFb->unbind();

    // Hito 15 Bloque 3: post-process pass. Lee `m_sceneFb` (HDR), escribe
    // a `m_viewportFb` (LDR).
    if (m_postProcess && m_sceneFb && m_viewportFb) {
        m_postProcess->apply(*m_sceneFb, *m_viewportFb,
                              m_exposure, m_tonemap);
        m_viewportFb->unbind();
    }
}

} // namespace Mood
