#pragma once

// F2H24: helpers compartidos por los archivos parciales de
// DemoSpawners (DemoSpawners.cpp + DemoSpawners_*.cpp). Header
// privado del modulo — no incluir desde otro modulo.

#include "core/Types.h"

#include <glm/ext/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Mood::detail {

// Hito 23: rota un AABB por un Euler en orden YXZ (mismo que
// TransformComponent::worldMatrix) y devuelve el rango Y resultante en
// world-space. Util para autoscale + floor offset post-rotacion.
struct WorldYBounds { f32 minY = 0.0f; f32 maxY = 0.0f; };

inline WorldYBounds rotatedAabbWorldY(const glm::vec3& aabbMin,
                                       const glm::vec3& aabbMax,
                                       const glm::vec3& eulerDeg) {
    glm::mat4 R(1.0f);
    R = glm::rotate(R, glm::radians(eulerDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    R = glm::rotate(R, glm::radians(eulerDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    R = glm::rotate(R, glm::radians(eulerDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));

    f32 minY = std::numeric_limits<f32>::max();
    f32 maxY = std::numeric_limits<f32>::lowest();
    for (int xi = 0; xi <= 1; ++xi)
    for (int yi = 0; yi <= 1; ++yi)
    for (int zi = 0; zi <= 1; ++zi) {
        const glm::vec3 p(
            xi ? aabbMax.x : aabbMin.x,
            yi ? aabbMax.y : aabbMin.y,
            zi ? aabbMax.z : aabbMin.z);
        const glm::vec3 rp = glm::vec3(R * glm::vec4(p, 1.0f));
        minY = std::min(minY, rp.y);
        maxY = std::max(maxY, rp.y);
    }
    return {minY, maxY};
}

} // namespace Mood::detail
