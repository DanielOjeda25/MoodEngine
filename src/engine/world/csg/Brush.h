#pragma once

// Brush implicito para CSG estilo Hammer / TrenchBroom (F2H11). Un
// brush es un solido convexo definido por la interseccion de N
// half-spaces (uno por cara). Cada cara lleva su plano + metadata
// para texturizado por cara (que F2H14 amplia con UVs reales).
//
// Convencion: el `normal` de cada cara apunta hacia AFUERA del brush.
// Un punto p pertenece al brush <=> signedDistance(face.plane, p)
// <= kPlaneEpsilon en todas las caras.
//
// Por que implicito y no triangulado: la representacion por planos
// permite mover una cara y regenerar la geometria sin perder UVs
// con lock-to-world (F2H14), y permite operaciones booleanas
// limpias (F2H12).

#include "core/Types.h"
#include "core/math/AABB.h"
#include "core/math/Plane.h"

#include <glm/mat4x4.hpp>

#include <vector>

namespace Mood::Csg {

struct BrushFace {
    Plane plane;
    u32 materialIndex = 0;
    // Placeholders para F2H14 (UV editor con lock-to-world):
    //   glm::vec3 uAxis;
    //   glm::vec3 vAxis;
    //   glm::vec2 uvOffset;
    //   glm::vec2 uvScale;
    //   f32 uvRotation;
    //   bool lockToWorld;
};

struct Brush {
    std::vector<BrushFace> faces;  // minimo 4 para ser solido cerrado
    AABB localAabb{glm::vec3(0.0f), glm::vec3(0.0f)};
};

/// @brief Construye un Box brush arbitrariamente orientado. La box
///        unitaria local va de (-0.5,-0.5,-0.5) a (+0.5,+0.5,+0.5),
///        6 caras con normales canonicas (+X, -X, +Y, -Y, +Z, -Z),
///        transformadas por `worldFromLocal`. Para la box "default"
///        del editor pasar `glm::mat4(1.0f)`.
///
///        Las normales se transforman con la inversa-transpuesta
///        (estandard para vectores normales bajo transforms con
///        escala no uniforme); las distancias se recomputan a partir
///        de un punto de referencia de la cara (centro local) llevado
///        a world. La AABB local resultante se calcula intersectando
///        los 8 vertices del cubo en world.
Brush makeBoxBrush(const glm::mat4& worldFromLocal,
                   u32 materialIndex = 0);

/// @brief Computa la AABB del brush a partir de sus planos: encuentra
///        todos los vertices candidatos (intersectThreePlanes de cada
///        triplete) que pertenecen al brush, los envuelve. Si el brush
///        es degenerado (< 4 caras o no produce vertices validos)
///        devuelve una AABB no-valida (min == max).
AABB computeBrushAabb(const Brush& brush);

} // namespace Mood::Csg
