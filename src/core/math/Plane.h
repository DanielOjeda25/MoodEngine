#pragma once

// Plano 3D y operaciones matematicas basicas. Header-only: las funciones
// son chicas y queremos que inlineen libremente en hot paths (frustum
// culling, CSG plane clipping).
//
// Convencion: dot(normal, p) + distance = 0  define el plano.
// signedDistance(plane, p) = dot(plane.normal, p) + plane.distance
//   > 0  -> p del lado de la normal
//   < 0  -> p del lado opuesto
//   = 0  -> p sobre el plano
//
// Origen del tipo: F2H3 (frustum culling) lo introdujo en
// engine/render/pipeline/Frustum.h. F2H11 (CSG) lo necesita para los
// brush implicitos, asi que se promueve a core/math/ para reuso.
//
// En CSG se usa con la normal apuntando hacia afuera del brush:
//   signedDistance > 0  -> punto fuera del brush por ese plano
//   signedDistance < 0  -> punto del lado interior del brush
// Un punto pertenece al brush <=> signedDistance <= kEpsilon en TODOS
// los planos del brush.

#include "core/Types.h"

#include <glm/geometric.hpp>  // glm::dot, glm::cross
#include <glm/vec3.hpp>

namespace Mood {

struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    f32 distance{0.0f};
};

/// @brief Tolerancia geometrica por defecto para comparaciones cerca de
///        cero (coincidencia de planos, vertex deduplication local). En
///        unidades de mundo del motor (1 unidad ~= 1 metro). 1e-4 da
///        resolucion submilimetrica sin caer en ruido de float.
inline constexpr f32 kPlaneEpsilon = 1e-4f;

/// @brief Distancia con signo del punto al plano. Positiva del lado de
///        la normal, negativa del lado opuesto. Asume `plane.normal`
///        ya normalizado (lo que la convencion del motor garantiza
///        para planos de frustum y de brush).
inline f32 signedDistance(const Plane& plane, const glm::vec3& point) {
    return glm::dot(plane.normal, point) + plane.distance;
}

/// @brief Construye un plano a partir de su normal y un punto que
///        pertenece a el. La normal se asume **ya normalizada**.
///        distance = -dot(normal, pointOnPlane) garantiza que
///        signedDistance(plane, pointOnPlane) == 0.
inline Plane planeFromPointAndNormal(const glm::vec3& pointOnPlane,
                                     const glm::vec3& unitNormal) {
    return Plane{unitNormal, -glm::dot(unitNormal, pointOnPlane)};
}

/// @brief Resuelve el punto de interseccion de tres planos via regla
///        de Cramer. Devuelve true si los tres planos intersectan en
///        un punto unico (sus tres normales son linealmente
///        independientes); false si dos o mas son paralelos o
///        coincidentes (el sistema 3x3 es singular).
///
///        Sistema lineal:
///          a.n . p = -a.d
///          b.n . p = -b.d
///          c.n . p = -c.d
///
///        det = dot(a.n, cross(b.n, c.n)). Si |det| < kPlaneEpsilon,
///        el sistema no tiene solucion unica.
///
///        Usado por F2H11 brush -> mesh: cada vertice candidato del
///        brush surge de la interseccion de tres caras (planos)
///        adyacentes. Se filtran luego los que NO pertenecen al
///        brush comparando contra los demas planos.
inline bool intersectThreePlanes(const Plane& a, const Plane& b,
                                 const Plane& c, glm::vec3& outPoint) {
    const glm::vec3 bxc = glm::cross(b.normal, c.normal);
    const f32 det = glm::dot(a.normal, bxc);
    if (det > -kPlaneEpsilon && det < kPlaneEpsilon) {
        return false;
    }
    const glm::vec3 cxa = glm::cross(c.normal, a.normal);
    const glm::vec3 axb = glm::cross(a.normal, b.normal);
    outPoint = (-a.distance * bxc - b.distance * cxa - c.distance * axb) / det;
    return true;
}

} // namespace Mood
