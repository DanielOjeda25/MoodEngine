#pragma once

// F2H24 Bloque C: helpers compartidos por EditorOverlay.cpp y
// EditorOverlay_Gizmo.cpp (proyeccion world->screen, raycasting screen-
// >world, calculo del parametro mas cercano entre dos lineas). Header
// privado del modulo — no incluir desde otro modulo.

#include "core/Types.h"

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace Mood::detail {

/// @brief Proyecta un punto world a coordenadas pixel del viewport
///        rectangular (x0,y0,w,h). Devuelve false si el punto cae
///        fuera del frustum (clip.w <= 0 o NDC.z fuera de [-1,1]).
inline bool projectWorldToScreen(const glm::mat4& vp,
                                   float x0, float y0, float w, float h,
                                   const glm::vec3& p,
                                   float& sx, float& sy) {
    const glm::vec4 clip = vp * glm::vec4(p, 1.0f);
    if (clip.w <= 1e-4f) return false;
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float ndcZ = clip.z / clip.w;
    if (ndcZ < -1.0f || ndcZ > 1.0f) return false;
    sx = x0 + (ndcX * 0.5f + 0.5f) * w;
    sy = y0 + (1.0f - (ndcY * 0.5f + 0.5f)) * h;
    return true;
}

/// @brief Genera el rayo de camara que pasa por el cursor (en pixels
///        del viewport rectangular). origin/dir devueltos en world.
inline bool cameraRayFromScreen(const glm::mat4& vp,
                                  float x0, float y0, float w, float h,
                                  ImVec2 mp,
                                  glm::vec3& rorigin, glm::vec3& rdir) {
    const float ndcX = ((mp.x - x0) / w) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((mp.y - y0) / h) * 2.0f;
    const glm::mat4 invVP = glm::inverse(vp);
    const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY, +1.0f, 1.0f);
    if (nearH.w == 0.0f || farH.w == 0.0f) return false;
    rorigin = glm::vec3(nearH) / nearH.w;
    const glm::vec3 farW = glm::vec3(farH) / farH.w;
    const glm::vec3 d = farW - rorigin;
    const float len = glm::length(d);
    if (len < 1e-6f) return false;
    rdir = d / len;
    return true;
}

/// @brief Parametro del punto de lineA (pA + dA*t) mas cercano al
///        rayo lineB (pB + dB*t). dA y dB deben ser unitarios.
///        Devuelve 0 si las lineas son paralelas.
inline f32 closestParamLineLine(const glm::vec3& pA, const glm::vec3& dA,
                                  const glm::vec3& pB, const glm::vec3& dB) {
    const glm::vec3 n = pA - pB;
    const f32 b  = glm::dot(dA, dB);
    const f32 d1 = glm::dot(dA, n);
    const f32 d2 = glm::dot(dB, n);
    const f32 denom = 1.0f - b * b;
    if (std::abs(denom) < 1e-5f) return 0.0f; // paralelas
    return (b * d2 - d1) / denom;
}

} // namespace Mood::detail
