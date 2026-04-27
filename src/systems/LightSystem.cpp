#include "systems/LightSystem.h"

#include "core/Log.h"
#include "engine/render/IShader.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/geometric.hpp>

namespace Mood {

namespace {

// Ambient bajo a partir de Hito 16: con shadow mapping las sombras se
// notan recien si el ambient no inunda las caras opuestas a la luz. 0.08
// deja las caras "en sombra" perceptiblemente oscuras pero no totalmente
// negras (se mantiene legibilidad de la textura). Hito 17: solo sirve
// como FALLBACK del PBR cuando uIblEnabled = 0 (IBL no cargado);
// normalmente el shader usa el irradiance map en su lugar.
constexpr float k_defaultAmbient = 0.08f;

} // namespace

LightFrameData LightSystem::buildFrameData(Scene& scene) {
    LightFrameData out;
    bool sawDirectional = false;
    u32 droppedPoints = 0;

    scene.forEach<LightComponent, TransformComponent>(
        [&](Entity, LightComponent& lc, TransformComponent& t) {
            if (!lc.enabled) return;
            if (lc.type == LightComponent::Type::Directional) {
                if (!sawDirectional) {
                    out.directional.direction = (glm::length(lc.direction) > 1e-5f)
                        ? glm::normalize(lc.direction)
                        : glm::vec3(0.0f, -1.0f, 0.0f);
                    out.directional.color     = lc.color;
                    out.directional.intensity = lc.intensity;
                    out.directional.enabled   = true;
                    sawDirectional = true;
                }
            } else { // Point
                if (out.pointLights.size() < k_MaxPointLights) {
                    PointLightData p{};
                    p.position  = t.position;
                    p.color     = lc.color;
                    p.intensity = lc.intensity;
                    p.radius    = lc.radius;
                    out.pointLights.push_back(p);
                } else {
                    ++droppedPoints;
                }
            }
        });

    if (droppedPoints > 0 && !m_warnedOverflow) {
        Log::render()->warn("LightSystem: {} point lights ignoradas (limite {})",
                             droppedPoints, k_MaxPointLights);
        m_warnedOverflow = true;
    }
    return out;
}

void LightSystem::bindUniforms(IShader& shader,
                                const LightFrameData& data,
                                const glm::vec3& cameraPos) {
    shader.setVec3("uCameraPos", cameraPos);
    shader.setVec3("uAmbient", glm::vec3(k_defaultAmbient));

    // Directional. Cuando enabled=false el shader la salta por el flag.
    shader.setVec3("uDirectional.direction", data.directional.direction);
    shader.setVec3("uDirectional.color",     data.directional.color);
    shader.setFloat("uDirectional.intensity", data.directional.intensity);
    shader.setInt("uDirectional.enabled",   data.directional.enabled ? 1 : 0);

    // Hito 18: las point lights ya NO se suben aca como uniform array.
    // Viven en un SSBO `PointLightBuffer` (binding 2) y el shader las
    // resuelve por tile via el LightGrid (SSBOs binding 3 + 4). El
    // EditorRenderPass arma el LightGrid y bindea los tres SSBOs entre
    // `bindUniforms` y los draw calls.
}

} // namespace Mood
