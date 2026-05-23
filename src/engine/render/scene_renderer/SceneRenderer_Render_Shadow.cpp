// break-B2: shadow/CSM recording extraido de SceneRenderer_Render.cpp.
// Detecta directional con castShadows + recompila el array si cambio
// cascadeCount + graba el CSM. Mismo orden y semantica que la version
// pre-split (auditado: zero cambio de comportamiento).

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/passes/ShadowPass.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

namespace Mood {

void SceneRenderer::recordShadowPass(Scene& scene, AssetManager& assets,
                                       const glm::mat4& view,
                                       const glm::mat4& projection,
                                       bool& outEnabled,
                                       glm::vec3& outLightDir) {
    outEnabled = false;
    outLightDir = glm::vec3(0.0f, -1.0f, 0.0f);
    if (!m_shadowPass) return;

    scene.forEach<LightComponent>(
        [&](Entity, LightComponent& lc) {
            if (outEnabled) return;
            if (!lc.enabled) return;
            if (lc.type != LightComponent::Type::Directional) return;
            if (!lc.castShadows) return;
            outEnabled = true;
            outLightDir = lc.direction;
        });

    if (outEnabled) {
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
                                  outLightDir, view, projection,
                                  m_csmCascadeCount, m_csmSplitLambda);
    }
    if (outEnabled != m_shadowEnabledLastFrame) {
        if (outEnabled) {
            Log::render()->info(
                "ShadowPass ACTIVO (directional con castShadows detectada, "
                "dir=({:.2f},{:.2f},{:.2f}))",
                outLightDir.x, outLightDir.y, outLightDir.z);
        } else {
            Log::render()->info("ShadowPass inactivo (sin directional con castShadows)");
        }
        m_shadowEnabledLastFrame = outEnabled;
    }
}

} // namespace Mood
