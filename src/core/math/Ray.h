#pragma once

// Ray geometrico + reconstruccion desde NDC (AUDIT-3). Free function pura
// — caller pasa `invVP` (= glm::inverse(projection * view)) y las coords
// NDC del cursor. Reemplaza la repeticion inline en EditorApplication_
// RunInteractions, ViewportPick, ScenePick y siblings (5+ ocurrencias
// identificadas en AUDIT-2 Bloque D).
//
// Tests unitarios en tests/test_ray.cpp.

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>

namespace Mood {

struct Ray {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

/// @brief Devuelve los puntos near/far en world space para un punto NDC,
///        sin normalizar la diferencia. Devuelve `nullopt` si alguno de
///        los `w` clip queda en 0 (proyeccion degenerada).
///
/// Util cuando el caller necesita la direccion no normalizada (p.ej.
/// para parametrizar una interseccion con plano: el parametro `t` queda
/// en unidades de "fraccion del segmento near->far").
struct NearFar {
    glm::vec3 nearW;
    glm::vec3 farW;
};

inline std::optional<NearFar> unprojectNearFar(const glm::mat4& invVP,
                                               float ndcX, float ndcY) {
    const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY, +1.0f, 1.0f);
    if (nearH.w == 0.0f || farH.w == 0.0f) {
        return std::nullopt;
    }
    return NearFar{
        glm::vec3(nearH) / nearH.w,
        glm::vec3(farH)  / farH.w,
    };
}

/// @brief Construye un Ray (origen = near, direction normalizada) desde
///        un punto NDC. Devuelve `nullopt` si el unprojection es
///        degenerado (clip.w = 0) o si los puntos near/far coinciden
///        (direction zero — no se puede normalizar).
inline std::optional<Ray> pickRayFromNdc(const glm::mat4& invVP,
                                         float ndcX, float ndcY) {
    const auto nf = unprojectNearFar(invVP, ndcX, ndcY);
    if (!nf) return std::nullopt;
    const glm::vec3 d = nf->farW - nf->nearW;
    const float len = glm::length(d);
    if (len < 1e-6f) return std::nullopt;
    return Ray{nf->nearW, d / len};
}

} // namespace Mood
