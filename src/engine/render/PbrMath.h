#pragma once

// Helpers PBR puros (sin GL) para testear y reusar la matematica del
// Cook-Torrance del `pbr.frag` desde C++. Vive separado para que el
// render shader y los tests compartan la misma definicion (single source
// of truth: si bumpeas la formula aca, el shader debe seguir).

#include "core/Types.h"

#include <glm/common.hpp>     // glm::clamp
#include <glm/geometric.hpp>  // glm::dot
#include <glm/vec3.hpp>

#include <algorithm>          // std::max
#include <cmath>

namespace Mood {

/// @brief F0 (reflectividad a incidencia normal) segun el workflow
///        metallic-roughness. Para dieléctricos F0 ~= 0.04 (4%, plásticos
///        y demás no-metales). Para metales F0 toma el color del albedo
///        — los metales no tienen diffuse y su especular es coloreado.
inline glm::vec3 fresnelF0(const glm::vec3& albedo, f32 metallic) {
    const glm::vec3 dielectric(0.04f);
    const f32 m = glm::clamp(metallic, 0.0f, 1.0f);
    return dielectric * (1.0f - m) + albedo * m;
}

/// @brief Fresnel-Schlick: aproximacion polinomial de quinto orden del
///        coeficiente de reflexion en funcion del angulo half-view
///        (`cos(theta_h_v)` en [0, 1]). Es la misma formula del shader
///        `pbr.frag::fresnelSchlick`.
///
///        F(cosTheta) = F0 + (1 - F0) * (1 - cosTheta)^5
inline glm::vec3 fresnelSchlick(f32 cosTheta, const glm::vec3& F0) {
    const f32 c = glm::clamp(1.0f - cosTheta, 0.0f, 1.0f);
    const f32 c5 = c * c * c * c * c;
    return F0 + (glm::vec3(1.0f) - F0) * c5;
}

/// @brief GGX (Trowbridge-Reitz) — distribucion normalizada de
///        microfacetas. `roughness` en [0, 1]; en el shader se eleva al
///        cuadrado para `alpha = r^2` (convencion Disney/glTF).
///
///        D(N,H) = a^2 / (PI * (NdotH^2 * (a^2 - 1) + 1)^2)
inline f32 distributionGGX(const glm::vec3& N, const glm::vec3& H, f32 roughness) {
    constexpr f32 PI = 3.14159265358979f;
    const f32 a  = roughness * roughness;
    const f32 a2 = a * a;
    const f32 nDotH  = std::max(glm::dot(N, H), 0.0f);
    const f32 nDotH2 = nDotH * nDotH;
    const f32 denom  = nDotH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

/// @brief Smith con Schlick-GGX — termino geometrico (self-shadowing +
///        masking). `k` para direct lighting es (r+1)^2 / 8.
inline f32 geometrySchlickGGX(f32 nDotX, f32 k) {
    return nDotX / (nDotX * (1.0f - k) + k);
}

inline f32 geometrySmith(const glm::vec3& N, const glm::vec3& V,
                          const glm::vec3& L, f32 roughness) {
    const f32 r = roughness + 1.0f;
    const f32 k = (r * r) / 8.0f;
    const f32 nDotV = std::max(glm::dot(N, V), 0.0f);
    const f32 nDotL = std::max(glm::dot(N, L), 0.0f);
    return geometrySchlickGGX(nDotV, k) * geometrySchlickGGX(nDotL, k);
}

} // namespace Mood
