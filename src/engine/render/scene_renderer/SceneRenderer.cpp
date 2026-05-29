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
#include "engine/render/backend/opengl/OpenGLOitFramebuffer.h"  // F2H64
#include "engine/render/backend/opengl/OpenGLInstanceBuffer.h"
#include "engine/render/backend/opengl/OpenGLParticleRenderer.h"
#include "engine/render/backend/opengl/OpenGLClothRenderer.h"  // F2H75
#include "engine/render/backend/opengl/OpenGLRenderer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/render/pipeline/LightGrid.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/shader_graph/ShaderGraphCache.h"  // F2H62 Bloque E
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/light/LightSystem.h"
#include "engine/render/passes/BloomPass.h"
#include "engine/render/passes/ColorGradingPass.h"
#include "engine/render/passes/PostProcessPass.h"
#include "engine/render/passes/SSAOPass.h"
// F3H31: sky procedural + GPU IBL bake.
#include "engine/render/sky/HosekWilkie.h"
#include "engine/render/sky/IBLBaker.h"
#include "engine/render/sky/ProceduralSkyRenderer.h"
#include "engine/render/passes/SkyboxRenderer.h"
#include "engine/render/passes/SSRPass.h"  // F2H61
#include "engine/render/passes/ShadowPass.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>

#include <glad/gl.h>  // F2H28 Bloque E: glGenVertexArrays / glDeleteVertexArrays

#include <array>
#include <filesystem>  // F2H86: detectar equirect vs cubemap dir.
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
    // F2H61: el scene FB ahora exporta un G-buffer parcial (color + normal
    // view-space en location 1) para el SSRPass. Sin SSR el normal RT existe
    // pero no se samplea; cost extra ~8MB a 1080p.
    m_sceneFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR, /*withNormalRT=*/true);
    // F2H56: FB intermedio HDR donde SSAOPass escribe (scene HDR
    // multiplicado por AO factor). Alimenta a BloomPass.
    m_ssaoOutFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    // F2H55: FB intermedio HDR (scene + bloom contribution) que alimenta
    // el post-process en lugar de leer directo de m_sceneFb.
    m_bloomFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    // F2H58: FB intermedio HDR (post bloom + color grading) entre el
    // bloom y el post-process. Reuso del mismo size que m_bloomFb.
    m_colorGradingFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    // F2H61: FB intermedio HDR donde SSR escribe (color base + reflejo
    // aditivo). Sin normal RT propio -- el SSR pass lee el normal RT del
    // m_sceneFb original.
    m_ssrFb = std::make_unique<OpenGLFramebuffer>(
        1280u, 720u, OpenGLFramebuffer::Format::HDR);
    // F2H63: snapshot del color del FB pre-translucent. Justo antes del
    // pase translucent hacemos blit m_sceneFb -> m_backbufferCopyFb; los
    // shaders translucent samplean su color attachment via uBackbufferCopy
    // para hacer refraccion screen-space (offset por normal * (IOR-1)).
    // HDR para preservar rango (no perdemos highlights del bloom inminente).
    m_backbufferCopyFb = std::make_unique<OpenGLFramebuffer>(
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

    // F2H62 Bloque E: cache de shader graphs compilados.
    // El vertex base es pbr.vert (meshes estaticos no-instanced). v1
    // NO soporta shader graph para instanced ni skinned -- caen al
    // PBR estandar transparentemente.
    m_shaderGraphCache = std::make_unique<ShaderGraphCache>("shaders/pbr.vert");

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

    // F2H64: composite OIT. Fullscreen tri (post_process.vert genera la
    // posicion desde gl_VertexID); fragment lee accum + revealage del FB
    // OIT y compone sobre el scene FB via blend hardware.
    try {
        m_oitCompositeShader = std::make_unique<OpenGLShader>(
            "shaders/post_process.vert", "shaders/oit_composite.frag");
    } catch (const std::exception& e) {
        Log::render()->warn("OitCompositeShader no disponible: {}. Translucents OIT "
                             "no funcionaran -- pase translucent skipeado por frame.",
                             e.what());
        m_oitCompositeShader.reset();
    }
    glGenVertexArrays(1, &m_oitCompositeVao);

    // F2H4: VBO recyclable para subir las matrices model cada frame.
    m_instanceBuffer = std::make_unique<OpenGLInstanceBuffer>();

    m_debugRenderer = std::make_unique<OpenGLDebugRenderer>();

    // F2H86: BRDF LUT global (no depende del skybox, mismo lookup tabular
    // para todos los environments). Carga separada del swap por-skybox.
    try {
        m_iblBrdfLut = std::make_unique<OpenGLTexture>("assets/ibl/brdf_lut.png");
    } catch (const std::exception& e) {
        Log::render()->warn("BRDF LUT no disponible: {}. Cae al ambient escalar.",
                             e.what());
        m_iblBrdfLut.reset();
    }

    // F2H86: skybox + IBL (irradiance + prefilter). Default kloofendal
    // (mismo cielo que mostraba el SkyboxRenderer en modo Equirect
    // pre-F2H86). El loader detecta automaticamente equirect vs cubemap
    // dir y cae silencioso si los assets no existen.
    loadSkyboxAndIblFromBase("skyboxes/sky_kloofendal");

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

    // F2H56: SSAO pass. Mismo patron tolerante a falla de shaders.
    try {
        m_ssaoPass = std::make_unique<SSAOPass>();
    } catch (const std::exception& e) {
        Log::render()->warn("SSAOPass no disponible: {}. SSAO desactivado.",
                             e.what());
        m_ssaoPass.reset();
    }

    // F2H58: Color Grading pass. Mismo patron tolerante.
    try {
        m_colorGradingPass = std::make_unique<ColorGradingPass>();
    } catch (const std::exception& e) {
        Log::render()->warn("ColorGradingPass no disponible: {}. Color grading desactivado.",
                             e.what());
        m_colorGradingPass.reset();
    }

    // F2H61: SSR pass. Mismo patron tolerante.
    try {
        m_ssrPass = std::make_unique<SSRPass>();
    } catch (const std::exception& e) {
        Log::render()->warn("SSRPass no disponible: {}. SSR desactivado.",
                             e.what());
        m_ssrPass.reset();
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

    // F2H75: cloth renderer. Mismo manejo tolerante a fallo de shader.
    try {
        m_clothRenderer = std::make_unique<OpenGLClothRenderer>();
    } catch (const std::exception& e) {
        Log::render()->warn("ClothRenderer no disponible: {}. Telas desactivadas.",
                             e.what());
        m_clothRenderer.reset();
    }
}

SceneRenderer::~SceneRenderer() {
    // Mismo orden inverso de destruccion que tenia EditorApplication antes
    // del refactor: recursos GL se tiran ANTES del contexto (que mata el
    // caller).
    m_particleRenderer.reset();
    m_clothRenderer.reset();
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
    m_ssaoPass.reset();   // F2H56
    m_colorGradingPass.reset(); // F2H58
    m_ssrPass.reset();    // F2H61
    // F2H58: liberar identity LUT sintetizada (si existe).
    if (m_identityLutId != 0) {
        GLuint tex = static_cast<GLuint>(m_identityLutId);
        glDeleteTextures(1, &tex);
        m_identityLutId = 0;
    }
    m_debugRenderer.reset();
    // F2H28: tirar FBOs orto + shaders orto antes del contexto GL.
    if (m_oitCompositeVao != 0) glDeleteVertexArrays(1, &m_oitCompositeVao); // F2H64
    m_oitCompositeShader.reset(); // F2H64
    if (m_grid2dVao != 0) glDeleteVertexArrays(1, &m_grid2dVao);
    m_grid2dShader.reset();
    for (auto& fb : m_orthoFbs) fb.reset();
    m_wireframeOrthoShader.reset();
    m_pbrSkinnedShader.reset();
    m_pbrInstancedShader.reset();
    m_pbrShader.reset();
    m_instanceBuffer.reset();
    m_viewportFb.reset();
    m_oitAccumFb.reset();      // F2H64 (antes que sceneFb: depende de su depth tex)
    m_backbufferCopyFb.reset();// F2H63
    m_bloomFb.reset();   // F2H55
    m_colorGradingFb.reset();  // F2H58
    m_ssrFb.reset();     // F2H61
    m_ssaoOutFb.reset(); // F2H56
    m_sceneFb.reset();
    m_renderer.reset();
}

FrameStats SceneRenderer::frameStats() const {
    return m_renderer ? m_renderer->frameStats() : FrameStats{};
}

// F3H23: VRAM query via NVX_gpu_memory_info (NVIDIA). Si el driver
// no expone la extensión (AMD/Intel/Mesa), GLAD_GL_NVX_gpu_memory_info
// es 0 y devolvemos 0 sin loguear (el caller muestra "—").
u64 SceneRenderer::vramUsedBytes() const {
    if (!GLAD_GL_NVX_gpu_memory_info) return 0;
    GLint totalKb = 0;
    GLint availKb = 0;
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalKb);
    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &availKb);
    if (totalKb <= 0 || availKb < 0 || availKb > totalKb) return 0;
    const u64 usedKb = static_cast<u64>(totalKb - availKb);
    return usedKb * 1024ull;
}


void SceneRenderer::synthesizeIdentityLut() {
    if (m_identityLutId != 0) return;  // idempotente

    // 256x16 RGBA8: 16 slices de 16x16. Para cada (x, y):
    //   sliceIdx  = x / 16          (0..15) -> componente Blue
    //   localR    = x % 16          (0..15) -> componente Red
    //   G         = y               (0..15)
    // El sample da color = (localR/15, G/15, sliceIdx/15) = el color de
    // entrada cuando se usa el algoritmo de lookup del color_grading.frag.
    // Por eso esta LUT es "identidad" (lookup(c) == c).
    constexpr u32 W = 256;
    constexpr u32 H = 16;
    std::vector<u8> pixels(W * H * 4);
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const u32 sliceIdx = x / 16;
            const u32 localR   = x % 16;
            const u32 idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<u8>((localR   * 255 + 7) / 15);
            pixels[idx + 1] = static_cast<u8>((y        * 255 + 7) / 15);
            pixels[idx + 2] = static_cast<u8>((sliceIdx * 255 + 7) / 15);
            pixels[idx + 3] = 255;
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // RGBA8 LINEAL (no sRGB) -- la LUT es una tabla de mapeo, no una
    // imagen para mirar. CLAMP_TO_EDGE en U para que el wrap entre
    // slices Blue no genere bleeding.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                  static_cast<GLsizei>(W), static_cast<GLsizei>(H), 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_identityLutId = static_cast<u32>(tex);
    Log::render()->info("ColorGrading: identity LUT sintetizada (id={}, 256x16 RGBA8 lineal)",
                         m_identityLutId);
}

void SceneRenderer::ensureOitFb(u32 width, u32 height) {
    if (!m_sceneFb) return;
    if (width == 0 || height == 0) return;
    const GLuint depthTex = m_sceneFb->glDepthTextureId();
    if (depthTex == 0) return;  // scene FB en modo LDR no expone depth tex
    if (!m_oitAccumFb) {
        m_oitAccumFb = std::make_unique<OpenGLOitFramebuffer>(width, height, depthTex);
        return;
    }
    m_oitAccumFb->resize(width, height, depthTex);
}

bool SceneRenderer::blitSceneToBackbufferCopy() {
    if (!m_sceneFb || !m_backbufferCopyFb) return false;
    const GLint w = static_cast<GLint>(m_sceneFb->width());
    const GLint h = static_cast<GLint>(m_sceneFb->height());
    if (w <= 0 || h <= 0) return false;

    // Capturar el FB activo (asumido = m_sceneFb por el caller) y el
    // read previo para restaurar al final. Inferimos los GL handles raw
    // (OpenGLFramebuffer no expone su `m_fbo`) consultando los bindings
    // antes y despues de un bind() del destino.
    GLint prevDrawFb = 0;
    GLint prevReadFb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFb);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFb);

    m_backbufferCopyFb->bind();
    GLint copyFbHandle = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &copyFbHandle);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevDrawFb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copyFbHandle);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBufs);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFb);
    return true;
}

void SceneRenderer::endFrame() {
    MOOD_PROFILE_FUNCTION();
    // F3H21: restaurar GL_FILL ANTES de los debug overlays — gizmos /
    // AABBs / handles del editor deben verse llenos aunque el scene este
    // wireframe. Si no estuvieramos en wireframe, este call es no-op
    // (GL_FILL es el default).
    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // F3H31 fix bug: el flush del debugRenderer se hace DESPUES del
    // post-process (SSAO/SSR/Bloom/Tonemap), no aqui. Antes este flush
    // dibujaba outlines/AABBs/gizmos al m_sceneFb y los post-process
    // los reflejaban (SSR), blooming (Bloom) o oscurecian (SSAO) —
    // produciendo el "outline fantasma" que el dev reporto.
    //
    // Mover el flush al final, contra m_viewportFb con depth blittado
    // del m_sceneFb, hace que los overlays se dibujen sobre la imagen
    // final tonemapeada sin afectar el pipeline PBR.

    m_renderer->endFrame();
    m_sceneFb->unbind();

    // F2H56: SSAO pass. Lee depth + color del m_sceneFb, escribe a
    // m_ssaoOutFb (= color * AO factor). Si esta apagado o el src no
    // tiene depth texture (modo LDR), se skipea sin tocar el flow.
    OpenGLFramebuffer* afterSsao = m_sceneFb.get();
    // F3H22: SSAO respeta el flag del Environment. Rendered ya NO fuerza
    // post passes (el dev controla AO/Bloom/SSR desde el Environment como
    // en Blender). Material vs Rendered se diferencia solo por
    // `m_skipPostPasses` (Material salta; Rendered no).
    const bool runSsao = m_ssaoEnabled
                      && !m_skipPostPasses
                      && m_ssaoIntensity > 0.0f
                      && m_ssaoPass
                      && m_ssaoOutFb
                      && m_sceneFb;
    if (runSsao) {
        MOOD_PROFILE_SCOPE("SSAOPass::apply");
        const glm::mat4 invProj = glm::inverse(m_lastProjection);
        const bool ssaoApplied = m_ssaoPass->apply(
            *m_sceneFb, *m_ssaoOutFb,
            m_lastProjection, invProj,
            m_ssaoRadius, m_ssaoIntensity);
        if (ssaoApplied) {
            afterSsao = m_ssaoOutFb.get();
        }
    }

    // F2H61: SSR pass. Lee color del afterSsao (asi el reflejo tiene AO
    // de la superficie) + depth/normal del m_sceneFb (G-buffer original).
    // Escribe a m_ssrFb el color + reflejo aditivo. Si esta off, sin
    // gbuffer, o intensity 0, se skipea y bloom lee de afterSsao directo.
    OpenGLFramebuffer* afterSsr = afterSsao;
    // F3H22: SSR respeta los flags del Environment (sin forcing). El
    // pase requiere normal RT + tuning a la escala de la escena, así que
    // el dev lo activa explícitamente desde el Environment Component.
    const bool runSsr = m_ssrEnabled
                     && !m_skipPostPasses
                     && m_ssrIntensity > 0.0f
                     && m_ssrPass
                     && m_ssrFb
                     && afterSsao
                     && m_sceneFb
                     && m_sceneFb->glNormalTextureId() != 0;
    if (runSsr) {
        MOOD_PROFILE_SCOPE("SSRPass::apply");
        // El FB del SSR puede no coincidir con el viewport size si la
        // ventana se resizeo entre frames. Forzar resize.
        if (m_ssrFb->width()  != afterSsao->width() ||
            m_ssrFb->height() != afterSsao->height()) {
            m_ssrFb->resize(afterSsao->width(), afterSsao->height());
        }
        const glm::mat4 invProj = glm::inverse(m_lastProjection);
        const bool ssrApplied = m_ssrPass->apply(
            *afterSsao, *m_sceneFb, *m_ssrFb,
            m_lastProjection, invProj,
            m_ssrMaxSteps, m_ssrThickness, m_ssrStepSize, m_ssrIntensity);
        if (ssrApplied) {
            afterSsr = m_ssrFb.get();
        }
    }

    // F2H55: bloom pass entre el resultado del SSR (o SSAO/scene si SSR
    // off) y post-process. Si esta apagado o intensity = 0, o el bloom
    // no pudo correr, se skipea y el post-process lee directo del FB
    // anterior. Cero regresion respecto a pre-F2H55.
    // F3H22: bloom respeta los flags del Environment. Sin forcing — el dev
    // controla threshold/intensity/radius desde ahí (igual que Blender).
    const bool runBloom = m_bloomEnabled
                       && !m_skipPostPasses
                       && m_bloomIntensity > 0.0f
                       && m_bloomPass
                       && m_bloomFb
                       && afterSsr;
    OpenGLFramebuffer* postProcessSrc = afterSsr;
    if (runBloom) {
        MOOD_PROFILE_SCOPE("BloomPass::apply");
        const bool bloomApplied = m_bloomPass->apply(
            *afterSsr, *m_bloomFb,
            m_bloomThreshold, m_bloomIntensity, m_bloomRadius);
        if (bloomApplied) {
            postProcessSrc = m_bloomFb.get();
        }
    }

    // F2H58: color grading entre bloom y post-process. Si esta off o
    // intensity = 0 o LUT no resuelta, skip. El pass internamente
    // chequea size del dst FB (resize-on-bind por dst.bind()).
    const bool runColorGrading = m_colorGradingEnabled
                               && !m_skipPostPasses  // F3H21
                               && m_colorGradingIntensity > 0.0f
                               && m_colorGradingPass
                               && m_colorGradingFb
                               && m_currentLutTextureId != 0
                               && postProcessSrc;
    if (runColorGrading) {
        MOOD_PROFILE_SCOPE("ColorGradingPass::apply");
        // El FB del color grading puede no coincidir con el viewport
        // size si la ventana se resizeo entre frames. Forzar resize
        // del dst FB al size del src antes del pass.
        if (m_colorGradingFb->width()  != postProcessSrc->width() ||
            m_colorGradingFb->height() != postProcessSrc->height()) {
            m_colorGradingFb->resize(postProcessSrc->width(),
                                       postProcessSrc->height());
        }
        const bool gradingApplied = m_colorGradingPass->apply(
            *postProcessSrc, *m_colorGradingFb,
            static_cast<GLuint>(m_currentLutTextureId),
            m_colorGradingIntensity);
        if (gradingApplied) {
            postProcessSrc = m_colorGradingFb.get();
        }
    }

    // Hito 15 Bloque 3: post-process pass. Lee HDR (m_sceneFb o m_bloomFb
    // segun el flag de arriba), escribe a `m_viewportFb` (LDR).
    if (m_postProcess && postProcessSrc && m_viewportFb) {
        MOOD_PROFILE_SCOPE("PostProcess::apply");
        m_postProcess->apply(*postProcessSrc, *m_viewportFb,
                              m_exposure, m_tonemap);
        // No hacer unbind aqui — necesitamos m_viewportFb bound para el
        // flush del debugRenderer abajo.
    }

    // F3H31 fix: flush del debugRenderer al m_viewportFb DESPUES del
    // post-process — outlines/AABBs/gizmos se dibujan sobre la imagen
    // final tonemapeada, sin pasar por SSR/Bloom/AO. Antes del flush
    // copiamos el depth de m_sceneFb (donde vive el z de la geometry)
    // al depth renderbuffer de m_viewportFb, para que el z-test del
    // debug shader funcione correctamente vs la geometry.
    if (m_viewportFb && m_sceneFb) {
        MOOD_PROFILE_SCOPE("DebugRenderer::flush");

        // Blit depth scene -> viewport. glBlitFramebuffer requiere
        // bindings READ + DRAW separados.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_sceneFb->glHandle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_viewportFb->glHandle());
        const GLint w = static_cast<GLint>(m_viewportFb->width());
        const GLint h = static_cast<GLint>(m_viewportFb->height());
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        // Bind m_viewportFb como destino + flush.
        m_viewportFb->bind();
        m_debugRenderer->flush(m_lastView, m_lastProjection);
        m_viewportFb->unbind();
    } else if (m_viewportFb) {
        m_viewportFb->unbind();
    }
}

} // namespace Mood
