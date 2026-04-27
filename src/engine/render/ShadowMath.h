#pragma once

// Calculo puro (sin GL) de las matrices del shadow pass para una luz
// direccional cubriendo un bounding sphere de la escena. Vive separado de
// `ShadowPass` para que sea testeable y reutilizable (ej. visualizar el
// frustum de la luz en un debug renderer en hitos posteriores).

#include "core/Types.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cmath>

namespace Mood {

struct ShadowMatrices {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    /// @brief Producto `proj * view`. Lo que se sube al shader como uLightSpace.
    glm::mat4 lightSpace{1.0f};
};

/// @brief Up vector seguro para `lookAt`. Si `lightDir` es casi paralelo a
///        (0,1,0), GL `lookAt` se rompe; en ese caso usamos (0,0,1).
inline glm::vec3 chooseLightUp(const glm::vec3& lightDir) {
    return (std::abs(lightDir.y) > 0.99f)
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
}

/// @brief Computa view + proj para shadow mapping con bounding sphere.
///
/// @param lightDir       Direccion a la que apunta la luz (no necesariamente
///                       normalizada; la funcion la normaliza). Si es ~zero,
///                       cae a (0,-1,0).
/// @param sceneCenter    Centro del bounding sphere.
/// @param sceneRadius    Radio del bounding sphere. La proyeccion ortografica
///                       cubre un cuadrado de lado `2 * radius * 1.5`
///                       (padding 1.5x para no clippear sombras al borde).
inline ShadowMatrices computeShadowMatrices(const glm::vec3& lightDir,
                                             const glm::vec3& sceneCenter,
                                             f32 sceneRadius) {
    glm::vec3 dir = lightDir;
    const f32 len = glm::length(dir);
    if (len < 1e-4f) {
        dir = glm::vec3(0.0f, -1.0f, 0.0f);
    } else {
        dir /= len;
    }

    const glm::vec3 lightPos = sceneCenter - dir * sceneRadius;
    const glm::mat4 view = glm::lookAt(lightPos, sceneCenter,
                                        chooseLightUp(dir));

    const f32 r = sceneRadius * 1.5f;
    const glm::mat4 proj = glm::ortho(-r, r, -r, r,
                                       0.0f, 2.0f * sceneRadius);

    ShadowMatrices out;
    out.view = view;
    out.proj = proj;
    out.lightSpace = proj * view;
    return out;
}

} // namespace Mood
