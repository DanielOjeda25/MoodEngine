#include "engine/world/csg/BrushOps.h"

#include "engine/world/csg/BrushMesh.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace Mood::Csg {

namespace {

/// @brief Cuenta vertices unicos del brush (puntos de interseccion
///        de tripletes de planos que pertenecen al brush, o sea
///        signedDistance <= kPlaneEpsilon en todos los planos
///        ademas de los tres del triplete). Dedup por kPlaneEpsilon.
///
///        Reusa la misma logica de computeBrushAabb pero contando
///        en lugar de envolver. Util para isBrushValid.
usize countBrushVertices(const Brush& b) {
    if (b.faces.size() < 4) return 0;
    const usize n = b.faces.size();
    std::vector<glm::vec3> uniqueVerts;

    for (usize i = 0; i < n; ++i) {
        for (usize j = i + 1; j < n; ++j) {
            for (usize k = j + 1; k < n; ++k) {
                glm::vec3 p{};
                if (!intersectThreePlanes(b.faces[i].plane,
                                          b.faces[j].plane,
                                          b.faces[k].plane, p)) {
                    continue;
                }
                bool inside = true;
                for (usize m = 0; m < n; ++m) {
                    if (m == i || m == j || m == k) continue;
                    if (signedDistance(b.faces[m].plane, p) > kPlaneEpsilon) {
                        inside = false;
                        break;
                    }
                }
                if (!inside) continue;

                bool dup = false;
                for (const auto& existing : uniqueVerts) {
                    if (std::fabs(existing.x - p.x) < kPlaneEpsilon &&
                        std::fabs(existing.y - p.y) < kPlaneEpsilon &&
                        std::fabs(existing.z - p.z) < kPlaneEpsilon) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) uniqueVerts.push_back(p);
            }
        }
    }
    return uniqueVerts.size();
}

/// @brief Plano flipped: misma geometria, normal invertida.
///        Si el plano original era `dot(n,p) + d = 0`, el flipped
///        es `dot(-n,p) - d = 0`. Vuelve interior <-> exterior.
Plane flipPlane(const Plane& p) {
    return Plane{-p.normal, -p.distance};
}

/// @brief True si los dos planos representan el mismo half-space
///        (mismas normal y distance, mod kPlaneEpsilon). Util para
///        evitar agregar planos duplicados en intersect/subtract.
bool planesEqual(const Plane& a, const Plane& b) {
    return std::fabs(a.normal.x - b.normal.x) < kPlaneEpsilon
        && std::fabs(a.normal.y - b.normal.y) < kPlaneEpsilon
        && std::fabs(a.normal.z - b.normal.z) < kPlaneEpsilon
        && std::fabs(a.distance - b.distance) < kPlaneEpsilon;
}

/// @brief Agrega un plano como nueva cara al brush, salvo que ya
///        exista una cara con ese mismo plano (mod kPlaneEpsilon).
///        Recomputa el AABB.
void appendFaceUnique(Brush& b, const Plane& p, u32 materialIndex) {
    for (const auto& f : b.faces) {
        if (planesEqual(f.plane, p)) return;
    }
    BrushFace face;
    face.plane = p;
    face.materialIndex = materialIndex;
    b.faces.push_back(face);
}

} // namespace

bool isBrushValid(const Brush& b) {
    // Solido cerrado en 3D necesita al menos 4 caras (tetraedro).
    if (b.faces.size() < 4) return false;
    // Y al menos 4 vertices unicos pertenecientes al brush.
    return countBrushVertices(b) >= 4;
}

std::vector<Brush> subtract(const Brush& A, const Brush& B) {
    // Caso trivial: si A no es valido, no hay nada que restar.
    if (!isBrushValid(A)) return {};
    // Si B no es valido, devolver A como esta.
    if (!isBrushValid(B)) return {A};

    // Algoritmo Hammer/Quake clasico:
    //   Mantener un brush "remanente" R que arranca como A.
    //   Para cada plano P_i de B (i = 0..N-1):
    //     - frag_i = R ∪ {flip(P_i)}    // R recortado al lado de afuera de P_i
    //     - Si frag_i es valido, agregarlo al output.
    //     - R <- R ∪ {P_i}              // R recortado al lado de adentro de P_i
    //     - Si R no es valido, terminar (no hay mas remanente que recortar).
    //
    // Al final, los frag_i juntos forman A \ B (descomposicion convexa).
    // R al terminar representa A ∩ B (lo "adentro" de B), que se descarta
    // — no es parte de A \ B.
    std::vector<Brush> result;
    Brush remainder = A;
    // Material por defecto: heredado de A.
    const u32 inheritedMat = A.faces.empty() ? 0u : A.faces.front().materialIndex;

    for (const auto& bFace : B.faces) {
        // Skip planos de B que coinciden con caras de A (caso tangencial
        // — no aporta carving real, solo agregaria caras duplicadas).
        bool coincident = false;
        for (const auto& aFace : A.faces) {
            if (planesEqual(aFace.plane, bFace.plane)) {
                coincident = true;
                break;
            }
        }
        if (coincident) {
            // Aun asi cortamos el remainder con este plano (parte
            // dentro de B), porque el resto de los planos de B
            // pueden seguir cortando.
            appendFaceUnique(remainder, bFace.plane, inheritedMat);
            remainder.localAabb = computeBrushAabb(remainder);
            if (!isBrushValid(remainder)) break;
            continue;
        }

        // Generar fragment: remainder ∪ {flip(bFace.plane)}.
        Brush frag = remainder;
        appendFaceUnique(frag, flipPlane(bFace.plane), inheritedMat);
        frag.localAabb = computeBrushAabb(frag);
        if (isBrushValid(frag)) {
            result.push_back(std::move(frag));
        }

        // Remainder ∪ {bFace.plane}.
        appendFaceUnique(remainder, bFace.plane, inheritedMat);
        remainder.localAabb = computeBrushAabb(remainder);
        if (!isBrushValid(remainder)) break;
    }

    return result;
}

std::optional<Brush> intersectOp(const Brush& A, const Brush& B) {
    if (!isBrushValid(A) || !isBrushValid(B)) return std::nullopt;

    // A ∩ B = brush con A.faces ∪ B.faces (dedup).
    Brush result = A;
    for (const auto& bFace : B.faces) {
        appendFaceUnique(result, bFace.plane, bFace.materialIndex);
    }
    result.localAabb = computeBrushAabb(result);

    if (!isBrushValid(result)) return std::nullopt;
    return result;
}

std::vector<Brush> unionOp(const Brush& A, const Brush& B) {
    if (!isBrushValid(A) && !isBrushValid(B)) return {};
    if (!isBrushValid(A)) return {B};
    if (!isBrushValid(B)) return {A};

    // Detectar contencion: si A subtract B = vacio entonces A ⊆ B
    // (todos los pedazos de A estan dentro de B) -> union = [B].
    const std::vector<Brush> aMinusB = subtract(A, B);
    if (aMinusB.empty()) return {B};

    const std::vector<Brush> bMinusA = subtract(B, A);
    if (bMinusA.empty()) return {A};

    // Detectar disjunto: si A intersect B no tiene volumen, son
    // disjuntos -> union = [A, B].
    const auto inter = intersectOp(A, B);
    if (!inter.has_value()) {
        return {A, B};
    }

    // Overlap parcial: A ∪ B = (A \ B) ∪ {B}.
    // (A \ B) son los pedazos de A fuera de B; agregando B completo
    // cubrimos toda la union sin overlaps.
    std::vector<Brush> result = aMinusB;
    result.push_back(B);
    return result;
}

} // namespace Mood::Csg
