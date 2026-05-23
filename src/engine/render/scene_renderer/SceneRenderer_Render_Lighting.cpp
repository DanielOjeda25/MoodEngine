// break-B2: light grid + scene-shader uniforms extraidos de
// SceneRenderer_Render.cpp.
//
//  - uploadLightGridSsbos: Forward+ light grid (Hito 18). Recompute del
//    grid + upload + bind de los 3 SSBO (point lights bind 2, tiles 3,
//    indices 4). Loguea on-edge si cambia la cantidad de point lights.
//
//  - applySceneShaderUniforms: era la lambda `applyShaderUniforms` de
//    renderScene. Setea todos los uniforms estandar del shader de
//    escena (mats, samplers, lights, CSM, fog, IBL). Llamada por el
//    pase opaco, el instanced, los brushes, el compiled mesh, el
//    skinned y el OIT.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLSSBO.h"
#include "engine/render/passes/ShadowPass.h"
#include "engine/render/pipeline/LightGrid.h"
#include "engine/render/rhi/IShader.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/pipeline/ShadowMath.h"
#include "systems/light/LightSystem.h"

#include <glad/gl.h>

#include <string>
#include <vector>

namespace Mood {

void SceneRenderer::uploadLightGridSsbos(const LightFrameData& lights,
                                          const glm::mat4& view,
                                          const glm::mat4& projection,
                                          u32 fbW, u32 fbH) {
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
}

void SceneRenderer::applySceneShaderUniforms(IShader& sh,
                                              const FrameContext& frame) {
    sh.bind();
    sh.setMat4("uView",       frame.view);
    sh.setMat4("uProjection", frame.projection);
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
                glm::vec2(static_cast<f32>(frame.fbW), static_cast<f32>(frame.fbH)));
    m_lightSystem->bindUniforms(sh, *frame.lights, frame.cameraPos);

    sh.setInt("uTileSize", static_cast<int>(k_LightGridTileSize));
    sh.setInt("uTilesX",   static_cast<int>(m_lightGrid->tilesX()));
    sh.setInt("uTilesY",   static_cast<int>(m_lightGrid->tilesY()));

    sh.setInt  ("uIblEnabled",      frame.iblOk ? 1 : 0);
    sh.setFloat("uPrefilterMaxLod", frame.prefilterMaxLod);
    sh.setFloat("uIblIntensity",    m_iblIntensity);

    // F2H60: CSM. uShadowMap pasa de sampler2DShadow a sampler2DArray
    // Shadow. El array de matrices `uLightSpaces[kMaxCsmCascades]` +
    // splits view-space `uCascadeSplits` permiten al shader elegir
    // cascada por fragment.
    sh.setInt  ("uShadowEnabled",  frame.shadowEnabled ? 1 : 0);
    // F2H60 polish iter4: bias bajado de 0.005 a 0.0015. El bias
    // antiguo era ~0.25m de "lift" en NDC depth -- visible peter-
    // panning al apoyar un brush sobre el piso (la sombra
    // "desaparecia" porque el bias hacia que el receiver pareciera
    // estar mas cerca de la luz que el caster). 0.0015 + el
    // biasScale por cascada del shader (x1, x2, x3, x4) es
    // suficiente para evitar acne sin despegar sombras cercanas.
    sh.setFloat("uShadowBias",     0.0015f);
    const u32 csmCount = (frame.shadowEnabled && m_shadowPass)
        ? m_shadowPass->cascadeCount() : 1;
    sh.setInt("uCascadeCount", static_cast<int>(csmCount));
    for (u32 i = 0; i < kMaxCsmCascades; ++i) {
        const std::string lsKey = "uLightSpaces[" + std::to_string(i) + "]";
        sh.setMat4(lsKey.c_str(),
            frame.shadowEnabled && m_shadowPass
                ? m_shadowPass->lightSpaceMatrix(i)
                : glm::mat4(1.0f));
    }
    for (u32 i = 0; i <= kMaxCsmCascades; ++i) {
        const std::string spKey = "uCascadeSplits[" + std::to_string(i) + "]";
        sh.setFloat(spKey.c_str(),
            frame.shadowEnabled && m_shadowPass
                ? m_shadowPass->cascadeSplit(i)
                : 0.0f);
    }

    sh.setInt  ("uFogMode",    static_cast<int>(m_fog.mode));
    sh.setVec3 ("uFogColor",   m_fog.color);
    sh.setFloat("uFogDensity", m_fog.density);
    sh.setFloat("uFogStart",   m_fog.linearStart);
    sh.setFloat("uFogEnd",     m_fog.linearEnd);
}

} // namespace Mood
