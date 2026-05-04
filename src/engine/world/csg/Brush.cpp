#include "engine/world/csg/Brush.h"

#include <glm/gtc/matrix_inverse.hpp>  // glm::inverseTranspose
#include <glm/geometric.hpp>           // glm::normalize

namespace Mood::Csg {

namespace {

/// @brief Transforma un plano por una matriz `worldFromLocal`.
///        - La normal pasa por la inversa-transpuesta (estandard
///          para vectores normales bajo escalas no uniformes), y se
///          renormaliza.
///        - La distancia se recomputa: tomamos un punto de la cara
///          local (`-distance * normal` cae sobre el plano), lo
///          transformamos a world, y reconstruimos la distancia con
///          la convencion `dot(n,p) + d = 0` => d = -dot(n,p).
Plane transformPlane(const Plane& localPlane,
                     const glm::mat4& worldFromLocal,
                     const glm::mat3& normalMatrix) {
    const glm::vec3 worldNormal =
        glm::normalize(normalMatrix * localPlane.normal);
    // pointOnLocalPlane satisface dot(n, p) + d = 0  =>  p = -d * n.
    const glm::vec3 pointOnLocalPlane =
        -localPlane.distance * localPlane.normal;
    const glm::vec3 pointOnWorldPlane =
        glm::vec3(worldFromLocal * glm::vec4(pointOnLocalPlane, 1.0f));
    return planeFromPointAndNormal(pointOnWorldPlane, worldNormal);
}

} // namespace

Brush makeBoxBrush(const glm::mat4& worldFromLocal, u32 materialIndex) {
    // 6 caras canonicas de la box unitaria centrada en origen
    // [-0.5, +0.5]^3, normales hacia afuera.
    constexpr struct {
        glm::vec3 normal;
        f32 distance;  // dot(n, p_on_face) + d = 0  con p en la cara
    } kLocalFaces[6] = {
        {{ 1.0f, 0.0f, 0.0f}, -0.5f},  // +X: x = 0.5
        {{-1.0f, 0.0f, 0.0f}, -0.5f},  // -X: x = -0.5
        {{ 0.0f, 1.0f, 0.0f}, -0.5f},  // +Y
        {{ 0.0f,-1.0f, 0.0f}, -0.5f},  // -Y
        {{ 0.0f, 0.0f, 1.0f}, -0.5f},  // +Z
        {{ 0.0f, 0.0f,-1.0f}, -0.5f},  // -Z
    };

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(worldFromLocal));

    Brush b;
    b.faces.reserve(6);
    for (const auto& f : kLocalFaces) {
        const Plane localPlane{f.normal, f.distance};
        BrushFace face;
        face.plane = transformPlane(localPlane, worldFromLocal, normalMatrix);
        face.materialIndex = materialIndex;
        b.faces.push_back(face);
    }

    b.localAabb = computeBrushAabb(b);
    return b;
}

AABB computeBrushAabb(const Brush& brush) {
    AABB result{glm::vec3(0.0f), glm::vec3(0.0f)};
    if (brush.faces.size() < 4) {
        return result;
    }

    const usize n = brush.faces.size();
    bool hasAny = false;

    // Iterar sobre cada triplete unico (i<j<k) y testear si el punto
    // de interseccion pertenece al brush (signedDistance <= kEps en
    // todas las demas caras).
    for (usize i = 0; i < n; ++i) {
        for (usize j = i + 1; j < n; ++j) {
            for (usize k = j + 1; k < n; ++k) {
                glm::vec3 p{};
                if (!intersectThreePlanes(brush.faces[i].plane,
                                          brush.faces[j].plane,
                                          brush.faces[k].plane, p)) {
                    continue;
                }
                bool inside = true;
                for (usize m = 0; m < n; ++m) {
                    if (m == i || m == j || m == k) continue;
                    if (signedDistance(brush.faces[m].plane, p) > kPlaneEpsilon) {
                        inside = false;
                        break;
                    }
                }
                if (!inside) continue;

                if (!hasAny) {
                    result = AABB{p, p};
                    hasAny = true;
                } else {
                    result.min = glm::min(result.min, p);
                    result.max = glm::max(result.max, p);
                }
            }
        }
    }

    return result;
}

} // namespace Mood::Csg
