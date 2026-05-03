#pragma once

// LightSystem (Hito 11): recolecta los `LightComponent` de la scene cada
// frame y devuelve una snapshot lista para subir como uniforms al shader
// `lit`. Sin estado persistente — la snapshot se reconstruye cada frame.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <vector>

namespace Mood {

class Scene;
class IShader;

/// @brief Limite hard de point lights por frame. Hito 18 (Forward+)
///        subio el cap de 8 a 256 al migrar el storage de uniform array
///        a SSBO. El shader loopea solo las luces del tile actual, asi
///        que el costo de tener mas luces "potenciales" es la asignacion
///        CPU del light grid + el tamaño del SSBO. 256 alcanza para
///        escenas densas; subir mas si aparece un caso real.
constexpr u32 k_MaxPointLights = 256;

struct DirectionalLightData {
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; // hacia donde apunta
    glm::vec3 color{1.0f};
    f32 intensity = 0.0f;                    // 0 = sin sun (default)
    bool enabled = false;
};

struct PointLightData {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 radius = 10.0f;
};

/// @brief Snapshot del estado de iluminacion para un frame.
struct LightFrameData {
    DirectionalLightData directional;
    std::vector<PointLightData> pointLights; // <= k_MaxPointLights
};

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
