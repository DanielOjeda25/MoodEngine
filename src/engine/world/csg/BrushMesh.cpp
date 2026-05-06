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

BrushMeshData buildBrushMesh(const Brush& brush,
                               const glm::mat4& worldMatrix) {
    BrushMeshData out;

    if (brush.faces.size() < 4) {
        return out;  // brush degenerado
    }

    for (usize f = 0; f < brush.faces.size(); ++f) {
        std::vector<glm::vec3> faceVerts = collectFaceVertices(brush, f);
        if (faceVerts.size() < 3) {
            continue;  // cara degenerada (no produce poligono)
        }

        const BrushFace& face = brush.faces[f];
        const glm::vec3 normal = face.plane.normal;
        const u32 materialIndex = face.materialIndex;

        sortFaceVerticesCCW(faceVerts, normal);

        // F2H15: UV computacion con params per-cara.
        // - Si lockToWorld: proyectar p_world sobre los ejes
        //   tangentes (textura "fija" al mundo).
        // - Si no: proyectar p_local (textura sigue al brush).
        // - Aplicar rotation, scale y offset.
        const f32 cosR = std::cos(face.uvRotation);
        const f32 sinR = std::sin(face.uvRotation);

        const u32 baseIndex = static_cast<u32>(out.vertices.size());
        for (const auto& p : faceVerts) {
            BrushMeshVertex v;
            v.position = p;
            v.normal = normal;

            // Punto a usar para la proyeccion UV.
            const glm::vec3 pForUV = face.lockToWorld
                ? glm::vec3(worldMatrix * glm::vec4(p, 1.0f))
                : p;

            const f32 uRaw = glm::dot(pForUV, face.uAxis);
            const f32 vRaw = glm::dot(pForUV, face.vAxis);
            // Rotacion 2D antes de scale + offset.
            const f32 uRot = cosR * uRaw - sinR * vRaw;
            const f32 vRot = sinR * uRaw + cosR * vRaw;
            v.uv = glm::vec2(uRot * face.uvScale.x + face.uvOffset.x,
                             vRot * face.uvScale.y + face.uvOffset.y);
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

std::vector<f32> brushMeshDataToInterleaved(const BrushMeshData& data) {
    // Layout PBR: pos(3) + color(3) + uv(2) + normal(3) = 11 floats.
    constexpr usize kFloatsPerVertex = 11;
    std::vector<f32> out;
    out.reserve(data.indices.size() * kFloatsPerVertex);

    for (u32 idx : data.indices) {
        if (idx >= data.vertices.size()) continue;  // brush corrupto
        const BrushMeshVertex& v = data.vertices[idx];
        out.push_back(v.position.x);
        out.push_back(v.position.y);
        out.push_back(v.position.z);
        out.push_back(1.0f);  // color blanco; albedoTint domina
        out.push_back(1.0f);
        out.push_back(1.0f);
        out.push_back(v.uv.x);
        out.push_back(v.uv.y);
        out.push_back(v.normal.x);
        out.push_back(v.normal.y);
        out.push_back(v.normal.z);
    }
    return out;
}

} // namespace Mood::Csg
