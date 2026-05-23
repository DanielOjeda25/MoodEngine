#pragma once

// POD del frame data de iluminacion. Extraido de LightSystem.h en
// break-B1 para romper la inversion de capa `engine/render/pipeline/`
// -> `systems/light/`. Hoy lo incluyen `LightSystem.h` (productor) y
// `LightGrid.h` (consumidor en engine/). Sin codigo, sin includes
// de OpenGL: solo POD reutilizable por las dos capas.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <vector>

namespace Mood {

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

} // namespace Mood
