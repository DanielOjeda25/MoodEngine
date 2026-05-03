#pragma once

// Fog uniforme distance-based (Hito 15 Bloque 2). Tres modos:
//
//   - Off (0):    factor = 0  (sin fog).
//   - Linear (1): factor = clamp((d - start) / (end - start), 0, 1).
//   - Exp (2):    factor = 1 - exp(-density * d).
//   - Exp2 (3):   factor = 1 - exp(-(density * d)^2).
//
// El shader `lit.frag` aplica `mix(litColor, fogColor, factor)`. Este
// header existe para poder testear los modos sin GL (la GLSL replica
// exactamente la misma matematica).

#include "core/Types.h"

#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/vec3.hpp>

#include <cmath>

namespace Mood {

enum class FogMode : u32 {
    Off    = 0,
    Linear = 1,
    Exp    = 2,
    Exp2   = 3,
};

/// @brief Parametros del fog. Todos en world units (metros, segun la
///        convencion del motor: 1 unidad = 1 m).
struct FogParams {
    FogMode mode = FogMode::Off;
    glm::vec3 color{0.55f, 0.65f, 0.75f}; // gris-azul neutro default
    f32 density = 0.02f;                  // exp / exp2
    f32 linearStart = 5.0f;               // lineal: distancia del start
    f32 linearEnd = 50.0f;                // lineal: distancia del 100%
};

/// @brief Calcula el factor de fog en [0, 1] segun la distancia al ojo.
inline f32 computeFogFactor(const FogParams& p, f32 distance) {
    switch (p.mode) {
        case FogMode::Off:
            return 0.0f;
        case FogMode::Linear: {
            const f32 denom = (p.linearEnd - p.linearStart);
            if (denom <= 0.0f) return 1.0f; // configuracion degenerada
            const f32 t = (distance - p.linearStart) / denom;
            return glm::clamp(t, 0.0f, 1.0f);
        }
        case FogMode::Exp: {
            return 1.0f - std::exp(-p.density * distance);
        }
        case FogMode::Exp2: {
            const f32 dd = p.density * distance;
            return 1.0f - std::exp(-(dd * dd));
        }
    }
    return 0.0f;
}

/// @brief Aplica fog a un color "lit" (post iluminacion, pre tonemap).
///        `litColor` es el resultado de Blinn-Phong; `distance` es la
///        distancia world-space del fragment al ojo.
inline glm::vec3 applyFog(const glm::vec3& litColor, const FogParams& p,
                          f32 distance) {
    const f32 t = computeFogFactor(p, distance);
    return glm::mix(litColor, p.color, t);
}

} // namespace Mood
