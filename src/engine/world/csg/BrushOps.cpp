#include "engine/world/csg/BrushOps.h"

#include "engine/world/csg/BrushMesh.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <limits>

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
    // Al menos 4 vertices unicos pertenecientes al brush.
    if (countBrushVertices(b) < 4) return false;
    // AABB no-degenerada: extension > kPlaneEpsilon en los 3 ejes.
    // Sin esto, un brush "cuadrilatero plano" (4 vertices coplanares,
    // ej. y=0.5 con x,z variando) pasaba el check de 4 vertices y
    // las ops booleanas generaban brushes degenerados (subtract de
    // disjuntos producia hasta 4 copias planas de A).
    const AABB aabb = computeBrushAabb(b);
    const glm::vec3 size = aabb.size();
    if (size.x < kPlaneEpsilon ||
        size.y < kPlaneEpsilon ||
        size.z < kPlaneEpsilon) {
        return false;
    }
    return true;
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

std::vector<Brush> clipBrushByPlane(const Brush& A,
                                     const Plane& clipPlane,
                                     bool keepFront,
                                     bool keepBack) {
    std::vector<Brush> out;
    if (!isBrushValid(A)) return out;
    if (!keepFront && !keepBack) return out;

    // F2H32 Bloque B: el "front" del split es el half-space del lado
    // positivo de la normal del clipPlane (signedDistance > 0).
    // Convencion CSG del motor: las normales de las caras del brush
    // apuntan AFUERA, el interior es la interseccion de half-spaces
    // negativos (signedDistance <= 0). Para mantener el "front"
    // agregamos un plano con normal NEGADA (interior queda del lado
    // positivo del clipPlane original); para "back" agregamos el
    // plano tal cual.

    auto makeSide = [&](bool front) -> Brush {
        Brush copy = A;
        BrushFace newFace;
        if (front) {
            newFace.plane.normal   = -clipPlane.normal;
            newFace.plane.distance = -clipPlane.distance;
        } else {
            newFace.plane = clipPlane;
        }
        // Heredar material slot + UV defaults del primer face del
        // brush original (mismo patron que F2H12 boolean ops).
        if (!copy.faces.empty()) {
            newFace.materialIndex = copy.faces.front().materialIndex;
            newFace.uAxis     = copy.faces.front().uAxis;
            newFace.vAxis     = copy.faces.front().vAxis;
            newFace.uvOffset  = copy.faces.front().uvOffset;
            newFace.uvScale   = copy.faces.front().uvScale;
            newFace.uvRotation= copy.faces.front().uvRotation;
            newFace.lockToWorld = copy.faces.front().lockToWorld;
        }
        copy.faces.push_back(newFace);
        copy.localAabb = computeBrushAabb(copy);
        return copy;
    };

    if (keepFront) {
        Brush f = makeSide(true);
        if (isBrushValid(f)) out.push_back(std::move(f));
    }
    if (keepBack) {
        Brush b = makeSide(false);
        if (isBrushValid(b)) out.push_back(std::move(b));
    }
    return out;
}

// --- F2H33 Bloque D: texture alignment ops ---

bool FaceUvRect::isValid() const {
    return (uMax - uMin) > kPlaneEpsilon &&
           (vMax - vMin) > kPlaneEpsilon;
}

FaceUvRect computeFaceUvRect(const Brush& brush, u32 faceIdx,
                              const glm::mat4& worldMatrix,
                              const glm::vec3* overrideUAxis,
                              const glm::vec3* overrideVAxis,
                              const f32* overrideRotation) {
    FaceUvRect rect;
    if (faceIdx >= brush.faces.size()) return rect;
    const BrushFace& face = brush.faces[faceIdx];

    // Recolectar el poligono local de la cara (worldMatrix=identity para
    // que `pos` quede en local space; lockToWorld lo aplicamos manual).
    const auto poly = collectFaceWorldPolygon(brush, faceIdx, glm::mat4(1.0f));
    if (poly.size() < 3) return rect;

    const glm::vec3& uAxis = overrideUAxis ? *overrideUAxis : face.uAxis;
    const glm::vec3& vAxis = overrideVAxis ? *overrideVAxis : face.vAxis;
    const f32 rot = overrideRotation ? *overrideRotation : face.uvRotation;
    const f32 cosR = std::cos(rot);
    const f32 sinR = std::sin(rot);

    f32 uMin = std::numeric_limits<f32>::max();
    f32 uMax = std::numeric_limits<f32>::lowest();
    f32 vMin = std::numeric_limits<f32>::max();
    f32 vMax = std::numeric_limits<f32>::lowest();
    for (const auto& pLocal : poly) {
        // Mismo flow que BrushMesh.cpp: lockToWorld decide si se usa
        // pos local o pos transformada por worldMatrix.
        const glm::vec3 pForUV = face.lockToWorld
            ? glm::vec3(worldMatrix * glm::vec4(pLocal, 1.0f))
            : pLocal;
        const f32 uRaw = glm::dot(pForUV, uAxis);
        const f32 vRaw = glm::dot(pForUV, vAxis);
        const f32 uRot = cosR * uRaw - sinR * vRaw;
        const f32 vRot = sinR * uRaw + cosR * vRaw;
        if (uRot < uMin) uMin = uRot;
        if (uRot > uMax) uMax = uRot;
        if (vRot < vMin) vMin = vRot;
        if (vRot > vMax) vMax = vRot;
    }
    rect.uMin = uMin; rect.uMax = uMax;
    rect.vMin = vMin; rect.vMax = vMax;
    return rect;
}

void alignFaceToFace(BrushFace& face) {
    glm::vec3 newU(1, 0, 0), newV(0, 1, 0);
    defaultTangentBasis(face.plane.normal, newU, newV);
    face.uAxis = newU;
    face.vAxis = newV;
    face.uvOffset = glm::vec2(0.0f, 0.0f);
    face.uvScale = glm::vec2(1.0f, 1.0f);
    face.uvRotation = 0.0f;
    // lockToWorld se preserva — es decision del dev independiente
    // del default UV.
}

void fitFaceToRect(BrushFace& face, const FaceUvRect& rect) {
    if (!rect.isValid()) return;
    // finalUV(rect.min) = 0; finalUV(rect.max) = 1.
    //   finalU = uRot * uvScale.x + uvOffset.x
    //   want: rect.uMin -> 0, rect.uMax -> 1
    //         uvScale.x = 1 / (uMax - uMin)
    //         uvOffset.x = -uMin * uvScale.x
    face.uvScale.x = 1.0f / rect.width();
    face.uvScale.y = 1.0f / rect.height();
    face.uvOffset.x = -rect.uMin * face.uvScale.x;
    face.uvOffset.y = -rect.vMin * face.uvScale.y;
}

void justifyFaceToRect(BrushFace& face, const FaceUvRect& rect,
                        JustifySide side) {
    if (!rect.isValid()) return;
    // Mantener uvScale + uvRotation; ajustar uvOffset para que el
    // borde correspondiente toque UV=0 (L/T) o UV=1 (R/B).
    switch (side) {
        case JustifySide::Left:
            // finalU(uMin) = 0 -> uvOffset.x = -uMin * uvScale.x
            face.uvOffset.x = -rect.uMin * face.uvScale.x;
            break;
        case JustifySide::Right:
            // finalU(uMax) = 1 -> uvOffset.x = 1 - uMax * uvScale.x
            face.uvOffset.x = 1.0f - rect.uMax * face.uvScale.x;
            break;
        case JustifySide::Top:
            // Convencion textura: V=0 arriba. finalV(vMin) = 0.
            face.uvOffset.y = -rect.vMin * face.uvScale.y;
            break;
        case JustifySide::Bottom:
            // V=1 abajo. finalV(vMax) = 1.
            face.uvOffset.y = 1.0f - rect.vMax * face.uvScale.y;
            break;
    }
}

} // namespace Mood::Csg
