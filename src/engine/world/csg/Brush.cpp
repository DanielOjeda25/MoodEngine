#include "engine/world/csg/Brush.h"

#include <glm/gtc/matrix_inverse.hpp>  // glm::inverseTranspose
#include <glm/geometric.hpp>           // glm::normalize, cross, dot

#include <algorithm>                   // std::sort en collectFaceWorldPolygon
#include <cmath>                       // std::fabs
#include <limits>                      // std::numeric_limits para pickFace

namespace Mood::Csg {

void defaultTangentBasis(const glm::vec3& normal,
                          glm::vec3& outUAxis,
                          glm::vec3& outVAxis) {
    // Elegir un helper axis (Y o X) segun donde apunta la normal.
    // Si la normal es casi paralela a Y, usar X como helper para
    // evitar cross product con magnitud cercana a cero.
    const glm::vec3 helper =
        (std::fabs(normal.y) > 0.9f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);
    outUAxis = glm::normalize(glm::cross(helper, normal));
    outVAxis = glm::normalize(glm::cross(normal, outUAxis));
}

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
        // F2H15: tangent basis canonico desde la normal world.
        defaultTangentBasis(face.plane.normal, face.uAxis, face.vAxis);
        b.faces.push_back(face);
    }

    b.localAabb = computeBrushAabb(b);
    return b;
}

namespace {

/// @brief Recolecta los vertices del poligono de UNA cara
///        (intersect tripletes filtrando puntos del brush + dedup).
///        Devuelve los vertices en LOCAL space (antes de aplicar
///        worldMatrix). Mismo algoritmo que collectFaceVertices de
///        BrushMesh.cpp; duplicado aqui para que pickFace sea
///        independiente del path de mesh build.
std::vector<glm::vec3> collectFaceLocalVertices(const Brush& brush,
                                                  usize faceIndex) {
    std::vector<glm::vec3> verts;
    const usize n = brush.faces.size();
    if (faceIndex >= n) return verts;
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
            bool inside = true;
            for (usize m = 0; m < n; ++m) {
                if (m == faceIndex || m == j || m == k) continue;
                if (signedDistance(brush.faces[m].plane, p) > kPlaneEpsilon) {
                    inside = false;
                    break;
                }
            }
            if (!inside) continue;
            bool dup = false;
            for (const auto& existing : verts) {
                if (std::fabs(existing.x - p.x) < kPlaneEpsilon &&
                    std::fabs(existing.y - p.y) < kPlaneEpsilon &&
                    std::fabs(existing.z - p.z) < kPlaneEpsilon) {
                    dup = true;
                    break;
                }
            }
            if (!dup) verts.push_back(p);
        }
    }
    return verts;
}

/// @brief Ray-triangle intersection (Moller-Trumbore). Devuelve
///        `t` positivo si hit, -1 si no.
f32 rayTriangle(const glm::vec3& origin, const glm::vec3& dir,
                  const glm::vec3& v0, const glm::vec3& v1,
                  const glm::vec3& v2) {
    constexpr f32 kEps = 1e-7f;
    const glm::vec3 edge1 = v1 - v0;
    const glm::vec3 edge2 = v2 - v0;
    const glm::vec3 h = glm::cross(dir, edge2);
    const f32 a = glm::dot(edge1, h);
    if (a > -kEps && a < kEps) return -1.0f;  // ray paralelo
    const f32 f = 1.0f / a;
    const glm::vec3 s = origin - v0;
    const f32 u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return -1.0f;
    const glm::vec3 q = glm::cross(s, edge1);
    const f32 v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    const f32 t = f * glm::dot(edge2, q);
    if (t < kEps) return -1.0f;  // detras del origen
    return t;
}

} // namespace

std::vector<glm::vec3> collectFaceWorldPolygon(const Brush& brush,
                                                  u32 faceIndex,
                                                  const glm::mat4& worldMatrix) {
    std::vector<glm::vec3> localVerts =
        collectFaceLocalVertices(brush, faceIndex);
    if (localVerts.size() < 3) return {};

    // Transformar a world.
    std::vector<glm::vec3> worldVerts;
    worldVerts.reserve(localVerts.size());
    for (const auto& p : localVerts) {
        worldVerts.push_back(
            glm::vec3(worldMatrix * glm::vec4(p, 1.0f)));
    }

    // Ordenar CCW alrededor del centroide para que el render
    // de lineas (i, i+1) cierre el poligono. Reusa el algoritmo
    // de BrushMesh.cpp::sortFaceVerticesCCW pero aplicado a la
    // normal world-transformed.
    glm::vec3 centroid(0.0f);
    for (const auto& v : worldVerts) centroid += v;
    centroid /= static_cast<f32>(worldVerts.size());

    if (faceIndex >= brush.faces.size()) return {};
    // Normal en world space (transformada por la inversa-transpuesta).
    const glm::mat3 normalMatrix =
        glm::inverseTranspose(glm::mat3(worldMatrix));
    const glm::vec3 worldNormal =
        glm::normalize(normalMatrix * brush.faces[faceIndex].plane.normal);

    // Tangent basis para sortear por angulo.
    const glm::vec3 helper =
        (std::fabs(worldNormal.x) > 0.9f)
            ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    const glm::vec3 uAxis = glm::normalize(glm::cross(helper, worldNormal));
    const glm::vec3 vAxis = glm::normalize(glm::cross(worldNormal, uAxis));

    std::sort(worldVerts.begin(), worldVerts.end(),
              [&](const glm::vec3& a, const glm::vec3& b) {
        const glm::vec3 da = a - centroid;
        const glm::vec3 db = b - centroid;
        const f32 ang_a = std::atan2(glm::dot(da, vAxis), glm::dot(da, uAxis));
        const f32 ang_b = std::atan2(glm::dot(db, vAxis), glm::dot(db, uAxis));
        return ang_a < ang_b;
    });

    return worldVerts;
}

std::optional<u32> pickFace(const Brush& brush,
                              const glm::vec3& rayOrigin,
                              const glm::vec3& rayDir,
                              const glm::mat4& worldMatrix) {
    if (brush.faces.size() < 4) return std::nullopt;

    f32 bestT = std::numeric_limits<f32>::max();
    i32 bestFace = -1;

    // F2H17: para evitar pickear caras "del otro lado" del brush
    // (back-facing), transformamos cada normal a world y descartamos
    // las caras cuya normal apunte LEJOS de la camara (mismo sentido
    // que el rayo). Esto matchea el behavior de Blender / Hammer.
    const glm::mat3 normalMatrix =
        glm::inverseTranspose(glm::mat3(worldMatrix));

    for (usize fi = 0; fi < brush.faces.size(); ++fi) {
        // Skip back-faces.
        const glm::vec3 worldNormal =
            glm::normalize(normalMatrix * brush.faces[fi].plane.normal);
        if (glm::dot(worldNormal, rayDir) > 0.0f) {
            continue;  // back-face: la normal apunta hacia donde mira el rayo
        }
        std::vector<glm::vec3> localVerts =
            collectFaceLocalVertices(brush, fi);
        if (localVerts.size() < 3) continue;

        // Transformar vertices a world.
        std::vector<glm::vec3> worldVerts;
        worldVerts.reserve(localVerts.size());
        for (const auto& p : localVerts) {
            worldVerts.push_back(
                glm::vec3(worldMatrix * glm::vec4(p, 1.0f)));
        }

        // Centroide para fan-triangulation.
        glm::vec3 centroid(0.0f);
        for (const auto& v : worldVerts) centroid += v;
        centroid /= static_cast<f32>(worldVerts.size());

        // Triangular en abanico desde el centroide. Cada par
        // (i, i+1) consecutivo forma un triangulo con centroide.
        // Esto evita asumir que los vertices estan CCW-ordenados;
        // el algoritmo funciona con cualquier ordering convexo.
        const usize n = worldVerts.size();
        for (usize i = 0; i < n; ++i) {
            const glm::vec3& v0 = centroid;
            const glm::vec3& v1 = worldVerts[i];
            const glm::vec3& v2 = worldVerts[(i + 1) % n];
            const f32 t = rayTriangle(rayOrigin, rayDir, v0, v1, v2);
            if (t > 0.0f && t < bestT) {
                bestT = t;
                bestFace = static_cast<i32>(fi);
            }
        }
    }

    if (bestFace < 0) return std::nullopt;
    return static_cast<u32>(bestFace);
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

std::vector<BrushVertex> enumerateBrushVertices(const Brush& brush) {
    std::vector<BrushVertex> verts;
    const usize n = brush.faces.size();
    if (n < 4) return verts;

    auto findExisting = [&](const glm::vec3& p) -> int {
        for (usize idx = 0; idx < verts.size(); ++idx) {
            if (glm::distance(verts[idx].localPos, p) < kPlaneEpsilon) {
                return static_cast<int>(idx);
            }
        }
        return -1;
    };

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
                    if (signedDistance(brush.faces[m].plane, p)
                        > kPlaneEpsilon) {
                        inside = false;
                        break;
                    }
                }
                if (!inside) continue;

                const int existing = findExisting(p);
                if (existing >= 0) {
                    // Mismo vertex; agregar planos i/j/k que aun
                    // no esten en planeIndices.
                    auto& pi = verts[static_cast<usize>(existing)]
                                .planeIndices;
                    auto addUnique = [&](u32 idx) {
                        for (u32 e : pi) if (e == idx) return;
                        pi.push_back(idx);
                    };
                    addUnique(static_cast<u32>(i));
                    addUnique(static_cast<u32>(j));
                    addUnique(static_cast<u32>(k));
                } else {
                    BrushVertex uv;
                    uv.localPos = p;
                    uv.planeIndices = {static_cast<u32>(i),
                                        static_cast<u32>(j),
                                        static_cast<u32>(k)};
                    verts.push_back(std::move(uv));
                }
            }
        }
    }
    return verts;
}

namespace {

/// @brief Proyecta un punto local a NDC. Devuelve true si el punto
///        esta delante del near plane (clip.w > 0). Si esta detras,
///        ndc se setea a (NaN, NaN) y la funcion retorna false.
bool projectLocalToNdc(const glm::vec3& localPos,
                        const glm::mat4& worldMatrix,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        glm::vec2& outNdc) {
    const glm::vec4 worldH = worldMatrix * glm::vec4(localPos, 1.0f);
    const glm::vec4 clipH = projection * view * worldH;
    if (clipH.w <= 1e-5f) {
        outNdc = glm::vec2(std::numeric_limits<f32>::quiet_NaN());
        return false;
    }
    outNdc = glm::vec2(clipH.x / clipH.w, clipH.y / clipH.w);
    return true;
}

/// @brief Distancia punto-segmento en 2D.
f32 pointToSegmentDist2(const glm::vec2& p,
                          const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 ab = b - a;
    const f32 lenSq = glm::dot(ab, ab);
    if (lenSq < 1e-10f) {
        return glm::distance(p, a);
    }
    f32 t = glm::dot(p - a, ab) / lenSq;
    t = std::clamp(t, 0.0f, 1.0f);
    const glm::vec2 closest = a + t * ab;
    return glm::distance(p, closest);
}

} // anonymous

std::optional<VertexPick> pickVertex(const Brush& brush,
                                       const glm::mat4& worldMatrix,
                                       const glm::mat4& view,
                                       const glm::mat4& projection,
                                       f32 ndcX, f32 ndcY,
                                       f32 thresholdNdc) {
    const auto verts = enumerateBrushVertices(brush);
    if (verts.empty()) return std::nullopt;

    const glm::vec2 cursor(ndcX, ndcY);
    int bestIdx = -1;
    f32 bestDist = thresholdNdc;
    for (usize i = 0; i < verts.size(); ++i) {
        glm::vec2 ndc;
        if (!projectLocalToNdc(verts[i].localPos, worldMatrix,
                                 view, projection, ndc)) continue;
        const f32 d = glm::distance(cursor, ndc);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = static_cast<int>(i);
        }
    }
    if (bestIdx < 0) return std::nullopt;

    VertexPick vp;
    vp.localPos = verts[static_cast<usize>(bestIdx)].localPos;
    vp.worldPos = glm::vec3(
        worldMatrix * glm::vec4(vp.localPos, 1.0f));
    vp.planeIndices = verts[static_cast<usize>(bestIdx)].planeIndices;
    return vp;
}

std::optional<EdgePick> pickEdge(const Brush& brush,
                                   const glm::mat4& worldMatrix,
                                   const glm::mat4& view,
                                   const glm::mat4& projection,
                                   f32 ndcX, f32 ndcY,
                                   f32 thresholdNdc) {
    const auto verts = enumerateBrushVertices(brush);
    const usize n = brush.faces.size();
    if (verts.size() < 2 || n < 4) return std::nullopt;

    const glm::vec2 cursor(ndcX, ndcY);
    int bestI = -1, bestJ = -1;
    glm::vec3 bestLocalA(0.0f), bestLocalB(0.0f);
    f32 bestDist = thresholdNdc;

    auto vertexHasPlane = [&](usize vIdx, u32 plane) {
        for (u32 p : verts[vIdx].planeIndices) {
            if (p == plane) return true;
        }
        return false;
    };

    for (u32 i = 0; i < static_cast<u32>(n); ++i) {
        for (u32 j = i + 1; j < static_cast<u32>(n); ++j) {
            // Vertices que esten en AMBAS caras = endpoints del edge.
            int idxA = -1, idxB = -1;
            for (usize v = 0; v < verts.size(); ++v) {
                if (!vertexHasPlane(v, i) || !vertexHasPlane(v, j)) continue;
                if (idxA < 0) idxA = static_cast<int>(v);
                else if (idxB < 0) idxB = static_cast<int>(v);
                else { idxA = -1; break; } // > 2 -> degenerado
            }
            if (idxA < 0 || idxB < 0) continue;

            glm::vec2 ndcA, ndcB;
            if (!projectLocalToNdc(verts[static_cast<usize>(idxA)].localPos,
                                     worldMatrix, view, projection, ndcA)) continue;
            if (!projectLocalToNdc(verts[static_cast<usize>(idxB)].localPos,
                                     worldMatrix, view, projection, ndcB)) continue;
            const f32 d = pointToSegmentDist2(cursor, ndcA, ndcB);
            if (d < bestDist) {
                bestDist = d;
                bestI = static_cast<int>(i);
                bestJ = static_cast<int>(j);
                bestLocalA = verts[static_cast<usize>(idxA)].localPos;
                bestLocalB = verts[static_cast<usize>(idxB)].localPos;
            }
        }
    }
    if (bestI < 0) return std::nullopt;

    EdgePick ep;
    ep.localA = bestLocalA;
    ep.localB = bestLocalB;
    ep.worldA = glm::vec3(worldMatrix * glm::vec4(bestLocalA, 1.0f));
    ep.worldB = glm::vec3(worldMatrix * glm::vec4(bestLocalB, 1.0f));
    ep.planeA = static_cast<u32>(bestI);
    ep.planeB = static_cast<u32>(bestJ);
    return ep;
}

} // namespace Mood::Csg
