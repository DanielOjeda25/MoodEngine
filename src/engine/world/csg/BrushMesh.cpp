#include "engine/world/csg/BrushMesh.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Mood::Csg {

namespace {

/// @brief True si dos puntos estan a menos de kPlaneEpsilon en cada eje.
bool pointsEqual(const glm::vec3& a, const glm::vec3& b) {
    return std::fabs(a.x - b.x) < kPlaneEpsilon
        && std::fabs(a.y - b.y) < kPlaneEpsilon
        && std::fabs(a.z - b.z) < kPlaneEpsilon;
}

/// @brief Genera dos ejes ortogonales (uAxis, vAxis) tangentes a un
///        plano con normal `n`. Estables: depende solo de `n`.
///        Para UVs planas (F2H11), proyectamos puntos de la cara
///        sobre estos ejes.
void buildTangentBasis(const glm::vec3& n,
                        glm::vec3& uAxis, glm::vec3& vAxis) {
    // Elegir un eje ayudante que NO sea casi paralelo a n.
    const glm::vec3 helper =
        (std::fabs(n.x) > 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                : glm::vec3(1.0f, 0.0f, 0.0f);
    uAxis = glm::normalize(glm::cross(helper, n));
    vAxis = glm::normalize(glm::cross(n, uAxis));
}

/// @brief Para una cara, calcula el conjunto de vertices del poligono
///        de la cara: para cada par (j, k) de OTROS planos, intersect
///        con el plano de la cara y conserva el punto si pertenece al
///        brush.
std::vector<glm::vec3>
collectFaceVertices(const Brush& brush, usize faceIndex) {
    std::vector<glm::vec3> verts;
    const usize n = brush.faces.size();
    const Plane& facePlane = brush.faces[faceIndex].plane;

    for (usize j = 0; j < n; ++j) {
        if (j == faceIndex) continue;
        for (usize k = j + 1; k < n; ++k) {
            if (k == faceIndex) continue;

            glm::vec3 p{};
            if (!intersectThreePlanes(facePlane, brush.faces[j].plane,
                                      brush.faces[k].plane, p)) {
                continue;
            }

            // Filtrar: el punto debe pertenecer al brush. Es decir,
            // ningun otro plano lo deja afuera.
            bool inside = true;
            for (usize m = 0; m < n; ++m) {
                if (m == faceIndex || m == j || m == k) continue;
                if (signedDistance(brush.faces[m].plane, p) > kPlaneEpsilon) {
                    inside = false;
                    break;
                }
            }
            if (!inside) continue;

            // Dedup: si ya tenemos un punto coincidente, saltar.
            bool duplicate = false;
            for (const auto& existing : verts) {
                if (pointsEqual(existing, p)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                verts.push_back(p);
            }
        }
    }

    return verts;
}

/// @brief Ordena `verts` CCW alrededor del centroide visto desde
///        +normal de la cara. Usa atan2 en el plano tangente.
void sortFaceVerticesCCW(std::vector<glm::vec3>& verts,
                          const glm::vec3& normal) {
    if (verts.size() < 3) return;

    glm::vec3 centroid(0.0f);
    for (const auto& v : verts) centroid += v;
    centroid /= static_cast<f32>(verts.size());

    glm::vec3 uAxis, vAxis;
    buildTangentBasis(normal, uAxis, vAxis);

    // Ordenar por angulo atan2(v_proj, u_proj) alrededor del centroide.
    std::sort(verts.begin(), verts.end(),
              [&](const glm::vec3& a, const glm::vec3& b) {
        const glm::vec3 da = a - centroid;
        const glm::vec3 db = b - centroid;
        const f32 ang_a = std::atan2(glm::dot(da, vAxis), glm::dot(da, uAxis));
        const f32 ang_b = std::atan2(glm::dot(db, vAxis), glm::dot(db, uAxis));
        return ang_a < ang_b;
    });
}

} // namespace

BrushMeshData buildBrushMesh(const Brush& brush) {
    BrushMeshData out;

    if (brush.faces.size() < 4) {
        return out;  // brush degenerado
    }

    for (usize f = 0; f < brush.faces.size(); ++f) {
        std::vector<glm::vec3> faceVerts = collectFaceVertices(brush, f);
        if (faceVerts.size() < 3) {
            continue;  // cara degenerada (no produce poligono)
        }

        const glm::vec3 normal = brush.faces[f].plane.normal;
        const u32 materialIndex = brush.faces[f].materialIndex;

        sortFaceVerticesCCW(faceVerts, normal);

        glm::vec3 uAxis, vAxis;
        buildTangentBasis(normal, uAxis, vAxis);

        // Agregar vertices a la mesh y memorizar sus indices base.
        const u32 baseIndex = static_cast<u32>(out.vertices.size());
        for (const auto& p : faceVerts) {
            BrushMeshVertex v;
            v.position = p;
            v.normal = normal;
            v.uv = glm::vec2(glm::dot(p, uAxis), glm::dot(p, vAxis));
            v.materialIndex = materialIndex;
            out.vertices.push_back(v);
        }

        // Fan-triangulate desde baseIndex: triangulos
        // (baseIndex, baseIndex + i, baseIndex + i + 1) con i en [1, n-2].
        const u32 n = static_cast<u32>(faceVerts.size());
        for (u32 i = 1; i + 1 < n; ++i) {
            out.indices.push_back(baseIndex);
            out.indices.push_back(baseIndex + i);
            out.indices.push_back(baseIndex + i + 1);
        }
    }

    return out;
}

} // namespace Mood::Csg
