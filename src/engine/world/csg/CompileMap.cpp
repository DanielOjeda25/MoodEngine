#include "engine/world/csg/CompileMap.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Mood::Csg {

namespace {

constexpr f32 kAntiparallelDot = -0.9999f;  // dot < esto => antiparalelo

bool pointsEqualEps(const glm::vec3& a, const glm::vec3& b, f32 eps) {
    return std::fabs(a.x - b.x) < eps
        && std::fabs(a.y - b.y) < eps
        && std::fabs(a.z - b.z) < eps;
}

/// @brief Ordena los puntos CCW alrededor del centroide visto desde
///        +normal. Reusa la misma logica que `BrushMesh::sortFaceVerticesCCW`
///        (no expuesta — duplicado mínimo aceptable: ~10 LOC).
void sortVerticesCcw(std::vector<glm::vec3>& verts, const glm::vec3& normal) {
    if (verts.size() < 3) return;

    glm::vec3 centroid(0.0f);
    for (const auto& v : verts) centroid += v;
    centroid /= static_cast<f32>(verts.size());

    // Tangent basis estable a partir de la normal.
    const glm::vec3 helper =
        (std::fabs(normal.x) > 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                     : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 uAxis = glm::normalize(glm::cross(helper, normal));
    const glm::vec3 vAxis = glm::normalize(glm::cross(normal, uAxis));

    std::sort(verts.begin(), verts.end(),
              [&](const glm::vec3& a, const glm::vec3& b) {
        const glm::vec3 da = a - centroid;
        const glm::vec3 db = b - centroid;
        const f32 ang_a = std::atan2(glm::dot(da, vAxis), glm::dot(da, uAxis));
        const f32 ang_b = std::atan2(glm::dot(db, vAxis), glm::dot(db, uAxis));
        return ang_a < ang_b;
    });
}

/// @brief True si los dos sets de puntos son iguales como conjuntos
///        (ignorando orden) bajo `eps`. Se usa para detectar caras
///        internas: el mismo poligono en ambos brushes (uno con orden
///        CCW desde +n, el otro CCW desde -n => orden inverso visto
///        desde +n del primero).
bool polygonsMatch(const std::vector<glm::vec3>& a,
                    const std::vector<glm::vec3>& b,
                    f32 eps) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return false;

    std::vector<bool> matched(b.size(), false);
    for (const auto& pa : a) {
        bool found = false;
        for (usize j = 0; j < b.size(); ++j) {
            if (matched[j]) continue;
            if (pointsEqualEps(pa, b[j], eps)) {
                matched[j] = true;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

/// @brief Computa el poligono local de UNA cara — replica del helper
///        privado de `BrushMesh.cpp` (no expuesto). Igual algoritmo:
///        para cada par (j, k) de OTROS planos, intersect con el plano
///        de la cara y conserva el punto si pertenece al brush.
std::vector<glm::vec3> collectFaceLocalPolygon(const Brush& brush,
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

            bool duplicate = false;
            for (const auto& existing : verts) {
                if (pointsEqualEps(existing, p, kPlaneEpsilon)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) verts.push_back(p);
        }
    }
    return verts;
}

/// @brief Spatial hash key para weld. Tres ejes encodeados como i32.
struct CellKey {
    i32 x, y, z;
    bool operator==(const CellKey& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};
struct CellHash {
    usize operator()(const CellKey& k) const noexcept {
        // Hash polinomial con primos grandes — buena distribucion para
        // coordenadas world razonables (-1000 a +1000 metros).
        const u64 hx = static_cast<u64>(static_cast<i64>(k.x) * 73856093);
        const u64 hy = static_cast<u64>(static_cast<i64>(k.y) * 19349663);
        const u64 hz = static_cast<u64>(static_cast<i64>(k.z) * 83492791);
        return static_cast<usize>(hx ^ hy ^ hz);
    }
};

CellKey cellOf(const glm::vec3& p, f32 eps) {
    return CellKey{
        static_cast<i32>(std::floor(p.x / eps)),
        static_cast<i32>(std::floor(p.y / eps)),
        static_cast<i32>(std::floor(p.z / eps))
    };
}

/// @brief Estado del builder por submesh. Mantiene un spatial hash de
///        position -> indices candidatos para weld + accept (UV/normal
///        match dentro de `eps`).
struct SubmeshBuilder {
    CompiledSubmesh out;
    std::unordered_map<CellKey, std::vector<u32>, CellHash> hash;

    /// @brief Devuelve el index del vertice en el submesh — reusa si
    ///        encuentra un match (position + uv + normal todos ±eps),
    ///        si no agrega un vertex nuevo.
    u32 weldOrAdd(const CompiledVertex& v, f32 eps) {
        const CellKey base = cellOf(v.position, eps);
        // Buscar en celda + 26 vecinas (cubo 3x3x3 alrededor).
        for (i32 dz = -1; dz <= 1; ++dz)
        for (i32 dy = -1; dy <= 1; ++dy)
        for (i32 dx = -1; dx <= 1; ++dx) {
            const CellKey k{base.x + dx, base.y + dy, base.z + dz};
            auto it = hash.find(k);
            if (it == hash.end()) continue;
            for (u32 idx : it->second) {
                const CompiledVertex& existing = out.vertices[idx];
                if (!pointsEqualEps(existing.position, v.position, eps)) continue;
                if (!pointsEqualEps(existing.normal, v.normal, eps)) continue;
                if (std::fabs(existing.uv.x - v.uv.x) > eps) continue;
                if (std::fabs(existing.uv.y - v.uv.y) > eps) continue;
                return idx;
            }
        }
        const u32 newIdx = static_cast<u32>(out.vertices.size());
        out.vertices.push_back(v);
        hash[base].push_back(newIdx);
        return newIdx;
    }
};

} // namespace

std::vector<RawBrushFace> collectFaces(const std::vector<BrushSource>& sources) {
    std::vector<RawBrushFace> out;

    for (u32 b = 0; b < sources.size(); ++b) {
        const BrushSource& src = sources[b];
        const Brush& brush = src.brush;
        if (brush.faces.size() < 4) continue;

        // F2H15: mat3 inverse-transpose para transformar normales a world.
        const glm::mat3 normalMatrix =
            glm::inverseTranspose(glm::mat3(src.worldMatrix));

        for (u32 f = 0; f < brush.faces.size(); ++f) {
            std::vector<glm::vec3> localPoly = collectFaceLocalPolygon(brush, f);
            if (localPoly.size() < 3) continue;

            const BrushFace& face = brush.faces[f];
            const glm::vec3 worldNormal =
                glm::normalize(normalMatrix * face.plane.normal);

            // Sort en LOCAL primero (la normal local es estable), luego
            // transformamos al world. Esto garantiza orden CCW respecto
            // de la normal world incluso con escalas no uniformes
            // (siempre que el determinante sea positivo).
            sortVerticesCcw(localPoly, face.plane.normal);

            std::vector<glm::vec3> worldPoly;
            worldPoly.reserve(localPoly.size());
            for (const auto& p : localPoly) {
                worldPoly.push_back(
                    glm::vec3(src.worldMatrix * glm::vec4(p, 1.0f)));
            }

            RawBrushFace rf;
            rf.brushIndex = b;
            rf.faceIndex = f;
            // Resolver materialPath desde el slot de la cara.
            if (face.materialIndex < src.materialPaths.size()) {
                rf.materialPath = src.materialPaths[face.materialIndex];
            } else {
                rf.materialPath = "";  // cara apunta a slot inexistente
            }
            rf.worldPolygonCcw = std::move(worldPoly);
            rf.worldNormal = worldNormal;
            rf.uAxis = face.uAxis;
            rf.vAxis = face.vAxis;
            rf.uvOffset = face.uvOffset;
            rf.uvScale = face.uvScale;
            rf.uvRotation = face.uvRotation;
            rf.lockToWorld = face.lockToWorld;
            rf.worldMatrix = src.worldMatrix;
            out.push_back(std::move(rf));
        }
    }
    return out;
}

std::vector<bool> markInternalFaces(const std::vector<RawBrushFace>& faces,
                                      f32 epsilon) {
    std::vector<bool> culled(faces.size(), false);

    for (usize i = 0; i < faces.size(); ++i) {
        if (culled[i]) continue;
        const RawBrushFace& fi = faces[i];
        for (usize j = i + 1; j < faces.size(); ++j) {
            if (culled[j]) continue;
            const RawBrushFace& fj = faces[j];

            // Mismas brush no se cullean entre si (caso degenerado).
            if (fi.brushIndex == fj.brushIndex) continue;

            // Antiparalelas?
            if (glm::dot(fi.worldNormal, fj.worldNormal) >= kAntiparallelDot) {
                continue;
            }

            // Mismo poligono?
            if (!polygonsMatch(fi.worldPolygonCcw, fj.worldPolygonCcw, epsilon)) {
                continue;
            }

            culled[i] = true;
            culled[j] = true;
            break; // i ya no se procesa mas
        }
    }
    return culled;
}

CompiledMap compileMap(const std::vector<BrushSource>& sources,
                        f32 weldEpsilon) {
    CompiledMap out;
    out.stats.sourceBrushes = static_cast<u32>(sources.size());

    // Brushes degenerados (< 4 caras) se reportan en stats. collectFaces
    // los saltea silenciosamente.
    for (const auto& s : sources) {
        if (s.brush.faces.size() < 4) ++out.stats.brushesSkipped;
    }

    const auto rawFaces = collectFaces(sources);
    out.stats.sourceFaces = static_cast<u32>(rawFaces.size());

    const auto culled = markInternalFaces(rawFaces, weldEpsilon);
    for (bool c : culled) {
        if (c) ++out.stats.culledInternalFaces;
    }

    // PASO 1 — descubrir los materialPaths distintos (en orden de aparicion)
    // y contar vertices pre-weld para stats. El paso 2 hace la triangulacion
    // + weld real con builders paralelos.
    std::unordered_map<std::string, usize> pathToIndex;
    for (usize i = 0; i < rawFaces.size(); ++i) {
        if (culled[i]) continue;
        const RawBrushFace& face = rawFaces[i];
        if (pathToIndex.find(face.materialPath) == pathToIndex.end()) {
            pathToIndex[face.materialPath] = out.submeshes.size();
            CompiledSubmesh sub;
            sub.materialPath = face.materialPath;
            out.submeshes.push_back(std::move(sub));
        }
        out.stats.totalVerticesPreWeld +=
            static_cast<u32>(face.worldPolygonCcw.size());
    }

    // PASO 2 — generar los submeshes con weld. Builders paralelos para
    // mantener el spatial hash en O(1) amortizado por weldOrAdd.
    std::vector<SubmeshBuilder> builders(out.submeshes.size());
    for (usize i = 0; i < builders.size(); ++i) {
        builders[i].out.materialPath = out.submeshes[i].materialPath;
    }

    for (usize i = 0; i < rawFaces.size(); ++i) {
        if (culled[i]) continue;
        const RawBrushFace& face = rawFaces[i];

        const usize subIdx = pathToIndex.at(face.materialPath);
        SubmeshBuilder& b = builders[subIdx];

        glm::mat4 invWorld(1.0f);
        if (!face.lockToWorld) {
            invWorld = glm::inverse(face.worldMatrix);
        }
        const f32 cosR = std::cos(face.uvRotation);
        const f32 sinR = std::sin(face.uvRotation);

        // Computar indices de cada vertex del poligono, weldeando.
        std::vector<u32> polyIndices;
        polyIndices.reserve(face.worldPolygonCcw.size());
        for (const glm::vec3& worldP : face.worldPolygonCcw) {
            CompiledVertex cv;
            cv.position = worldP;
            cv.normal = face.worldNormal;

            // UV: misma logica que `BrushMesh::buildBrushMesh`. Si
            // lockToWorld, la position world es el input al UV calc;
            // si no, la position local (post-inverse del worldMatrix).
            const glm::vec3 pForUv = face.lockToWorld
                ? worldP
                : glm::vec3(invWorld * glm::vec4(worldP, 1.0f));
            const f32 uRaw = glm::dot(pForUv, face.uAxis);
            const f32 vRaw = glm::dot(pForUv, face.vAxis);
            const f32 uRot = cosR * uRaw - sinR * vRaw;
            const f32 vRot = sinR * uRaw + cosR * vRaw;
            cv.uv = glm::vec2(uRot * face.uvScale.x + face.uvOffset.x,
                              vRot * face.uvScale.y + face.uvOffset.y);

            polyIndices.push_back(b.weldOrAdd(cv, weldEpsilon));
        }

        // Fan-triangulate desde el primer vertex.
        const usize n = polyIndices.size();
        for (usize k = 1; k + 1 < n; ++k) {
            b.out.indices.push_back(polyIndices[0]);
            b.out.indices.push_back(polyIndices[k]);
            b.out.indices.push_back(polyIndices[k + 1]);
        }
    }

    // Commit builders -> out.submeshes.
    for (usize i = 0; i < builders.size(); ++i) {
        out.submeshes[i] = std::move(builders[i].out);
    }

    // Stats finales.
    for (const auto& sub : out.submeshes) {
        out.stats.totalVerticesUnique += static_cast<u32>(sub.vertices.size());
        out.stats.totalTriangles += static_cast<u32>(sub.indices.size() / 3);
    }
    out.stats.submeshCount = static_cast<u32>(out.submeshes.size());
    return out;
}

std::vector<BrushSource> collectBrushSourcesFromScene(
    Scene& scene, AssetManager& assets) {
    std::vector<BrushSource> out;
    scene.forEach<TransformComponent, BrushComponent>(
        [&](Entity, TransformComponent& tf, BrushComponent& bc) {
            BrushSource src;
            src.brush = bc.brush;
            src.worldMatrix = tf.worldMatrix();
            src.materialPaths.reserve(bc.materials.size());
            for (MaterialAssetId id : bc.materials) {
                src.materialPaths.push_back(assets.materialPathOf(id));
            }
            out.push_back(std::move(src));
        });
    return out;
}

} // namespace Mood::Csg
