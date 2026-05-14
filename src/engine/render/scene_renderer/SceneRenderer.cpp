// F2H24 Bloque C: nucleo del SceneRenderer (Hito 21 Bloque 2). El
// frame loop completo (`renderScene`) vive en SceneRenderer_Render.cpp.
// Aca solo el lifecycle de recursos GL + helpers compartidos por el
// caller:
//   - ctor: crea OpenGLRenderer + FBs (HDR scene, LDR viewport) +
//     shaders PBR (static / instanced / skinned) + skybox + IBL
//     (irradiance / prefilter / BRDF LUT) + post-process + shadow +
//     light grid + SSBOs + light/particle systems.
//   - dtor: orden inverso al ctor para liberar recursos GL antes que
//     el contexto.
//   - frameStats: pasamanos al renderer interno.
//   - applyEnvironmentFromScene: copia params del primer
//     EnvironmentComponent al state del renderer (fog / exposure /
//     tonemap / IBL intensity). Llamado por el caller pre-renderScene
//     (ej. al cargar proyecto, antes del primer frame).
//   - endFrame: flushea el debug renderer + post-process pass. Llamado
//     post-renderScene tras opcionalmente agregar geometria al
//     debugRenderer.
//
// Sin cambios funcionales — solo split por LOC. La logica del frame
// (skybox / shadow / Forward+ / PBR static + instanced + skinned /
// brushes / particles) queda intacta en SceneRenderer_Render.cpp.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLInstanceBuffer.h"
#include "engine/render/backend/opengl/OpenGLParticleRenderer.h"
#include "engine/render/backend/opengl/OpenGLRenderer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/render/pipeline/LightGrid.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/light/LightSystem.h"
#include "systems/render/BloomPass.h"
#include "systems/render/PostProcessPass.h"
#include "systems/render/ShadowPass.h"
#include "systems/render/SkyboxRenderer.h"

#include <glad/gl.h>  // F2H28 Bloque E: glGenVertexArrays / glDeleteVertexArrays

#include <array>
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
    // F2H55: FB intermedio HDR (scene + bloom contribution) que alimenta
    // el post-process en lugar de leer directo de m_sceneFb.
    m_bloomFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    m_viewportFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::LDR);

    // PBR (Cook-Torrance + metallic-roughness) — Hito 17.
    m_pbrShader = std::make_unique<OpenGLShader>(
        "shaders/pbr.vert", "shaders/pbr.frag");
    // F2H4: variante instanced. Mismo .frag que el static — el .vert
    // lee `mat4 model` desde locations 5-8 con divisor=1.
    m_pbrInstancedShader = std::make_unique<OpenGLShader>(
        "shaders/pbr_instanced.vert", "shaders/pbr.frag");
    // Skinned vertex shader, mismo .frag — Hito 19.
    m_pbrSkinnedShader = std::make_unique<OpenGLShader>(
        "shaders/pbr_skinned.vert", "shaders/pbr.frag");

    // F2H28: shader wireframe orto para el workspace "Editor de mapas".
    // Tolera fallo de carga (silencioso): el render orto detecta nullptr
    // y no se ejecuta — el resto del editor sigue funcionando.
    try {
        m_wireframeOrthoShader = std::make_unique<OpenGLShader>(
            "shaders/wireframe_ortho.vert", "shaders/wireframe_ortho.frag");
    } catch (const std::exception& e) {
        Log::render()->warn("WireframeOrthoShader no disponible: {}. "
                             "Workspace 'Editor de mapas' renderizara sin wireframe.",
                             e.what());
        m_wireframeOrthoShader.reset();
    }

    // F2H28: 3 FBOs LDR para Top/Front/Side. Tamano inicial 1280x720;
    // se resize a demanda al primer renderOrthoView.
    for (auto& fb : m_orthoFbs) {
        fb = std::make_unique<OpenGLFramebuffer>(
            1280u, 720u, OpenGLFramebuffer::Format::LDR);
    }

    // F2H28 Bloque E: shader del grid 2D + VAO trivial para el
    // fullscreen triangle. Tolera fallo de carga: si el shader no
    // compila, el render orto skipea el grid pass pero sigue pintando
    // el wireframe sobre fondo negro.
    try {
        m_grid2dShader = std::make_unique<OpenGLShader>(
            "shaders/grid2d.vert", "shaders/grid2d.frag");
    } catch (const std::exception& e) {
        Log::render()->warn("Grid2dShader no disponible: {}. Vista orto sin grid.",
                             e.what());
        m_grid2dShader.reset();
    }
    glGenVertexArrays(1, &m_grid2dVao);

    // F2H4: VBO recyclable para subir las matrices model cada frame.
    m_instanceBuffer = std::make_unique<OpenGLInstanceBuffer>();

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

    // F2H55: bloom pass. Tolera fallo de carga de shaders (silencioso)
    // - si los .frag no estan en build/shaders/, el editor sigue
    // renderizando sin bloom.
    try {
        m_bloomPass = std::make_unique<BloomPass>();
    } catch (const std::exception& e) {
        Log::render()->warn("BloomPass no disponible: {}. Bloom desactivado.",
                             e.what());
        m_bloomPass.reset();
    }

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
    m_bloomPass.reset();  // F2H55
    m_debugRenderer.reset();
    // F2H28: tirar FBOs orto + shaders orto antes del contexto GL.
    if (m_grid2dVao != 0) glDeleteVertexArrays(1, &m_grid2dVao);
    m_grid2dShader.reset();
    for (auto& fb : m_orthoFbs) fb.reset();
    m_wireframeOrthoShader.reset();
    m_pbrSkinnedShader.reset();
    m_pbrInstancedShader.reset();
    m_pbrShader.reset();
    m_instanceBuffer.reset();
    m_viewportFb.reset();
    m_bloomFb.reset();  // F2H55
    m_sceneFb.reset();
    m_renderer.reset();
}

FrameStats SceneRenderer::frameStats() const {
    return m_renderer ? m_renderer->frameStats() : FrameStats{};
}

void SceneRenderer::applyEnvironmentFromScene(Scene& scene) {
    // Reset a defaults primero. Sin este reset, abrir un proyecto sin
    // Environment hereda los valores del proyecto anterior.
    m_fog          = FogParams{};
    m_exposure     = 0.0f;
    m_tonemap      = TonemapMode{2}; // ACES
    m_iblIntensity = 1.0f;
    // F2H55: defaults bloom.
    m_bloomEnabled   = true;
    m_bloomThreshold = 1.0f;
    m_bloomIntensity = 0.6f;
    m_bloomRadius    = 1.0f;

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
            // F2H55: bloom params del componente al cache del renderer.
            m_bloomEnabled    = env.bloomEnabled;
            m_bloomThreshold  = env.bloomThreshold;
            m_bloomIntensity  = env.bloomIntensity;
            m_bloomRadius     = env.bloomRadius;
        });
}

void SceneRenderer::endFrame() {
    MOOD_PROFILE_FUNCTION();
    // Si el caller acumulo geometria de debug entre `renderScene` y
    // `endFrame`, la flusheamos aqui usando las matrices del frame.
    // Cuando no hay debug pendiente el flush es no-op (el renderer no
    // ejecuta vertex array vacio).
    {
        MOOD_PROFILE_SCOPE("DebugRenderer::flush");
        m_debugRenderer->flush(m_lastView, m_lastProjection);
    }

    m_renderer->endFrame();
    m_sceneFb->unbind();

    // F2H55: bloom pass entre scene HDR y post-process. Si esta apagado
    // o intensity = 0, o el bloom no pudo correr (size invalido), se
    // skipea y el post-process lee directo del m_sceneFb. Cero regresion
    // respecto a pre-F2H55.
    const bool runBloom = m_bloomEnabled
                       && m_bloomIntensity > 0.0f
                       && m_bloomPass
                       && m_bloomFb
                       && m_sceneFb;
    OpenGLFramebuffer* postProcessSrc = m_sceneFb.get();
    if (runBloom) {
        MOOD_PROFILE_SCOPE("BloomPass::apply");
        const bool bloomApplied = m_bloomPass->apply(
            *m_sceneFb, *m_bloomFb,
            m_bloomThreshold, m_bloomIntensity, m_bloomRadius);
        if (bloomApplied) {
            postProcessSrc = m_bloomFb.get();
        }
    }

    // Hito 15 Bloque 3: post-process pass. Lee HDR (m_sceneFb o m_bloomFb
    // segun el flag de arriba), escribe a `m_viewportFb` (LDR).
    if (m_postProcess && postProcessSrc && m_viewportFb) {
        MOOD_PROFILE_SCOPE("PostProcess::apply");
        m_postProcess->apply(*postProcessSrc, *m_viewportFb,
                              m_exposure, m_tonemap);
        m_viewportFb->unbind();
    }
}

} // namespace Mood
