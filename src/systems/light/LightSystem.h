#pragma once

// LightSystem (Hito 11): recolecta los `LightComponent` de la scene cada
// frame y devuelve una snapshot lista para subir como uniforms al shader
// `lit`. Sin estado persistente — la snapshot se reconstruye cada frame.

#include "core/Types.h"
#include "engine/render/pipeline/LightData.h"  // break-B1: PODs reusables

#include <glm/vec3.hpp>

namespace Mood {

class Scene;
class IShader;

class LightSystem {
public:
    /// @brief Recolecta `LightComponent` + `TransformComponent` de la scene.
    ///        El primer Directional encontrado gana; el resto se ignora.
    ///        Hasta `k_MaxPointLights` Point lights se incluyen; las demas
    ///        se descartan con un warn (loggeado una sola vez).
    LightFrameData buildFrameData(Scene& scene);

    /// @brief Sube la `LightFrameData` + `cameraPos` al shader. Asume el
    ///        layout de uniforms del shader `lit.frag`. Llamar ANTES de los
    ///        draw calls de la scene.
    void bindUniforms(IShader& shader,
                      const LightFrameData& data,
                      const glm::vec3& cameraPos);

private:
    bool m_warnedOverflow = false;
};

} // namespace Mood
