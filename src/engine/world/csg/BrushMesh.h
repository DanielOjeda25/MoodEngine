#pragma once

// Algoritmo brush implicito -> mesh triangulada (F2H11). Para cada
// cara del brush:
//   1. Intersecta su plano con todos los pares de OTROS planos para
//      obtener vertices candidatos (intersectThreePlanes).
//   2. Filtra los candidatos que NO pertenecen al brush
//      (signedDistance > kPlaneEpsilon contra cualquier OTRO plano
//      ademas del propio).
//   3. Dedup vertices coincidentes (igualdad bajo kPlaneEpsilon).
//   4. Ordena CCW alrededor del centroide de la cara (visto desde
//      fuera, i.e. en el sentido de face.plane.normal).
//   5. Fan-triangulate desde el vertice 0 (valido porque las caras
//      de brush son **convexas por construccion**).
//
// Esta es la parte critica del modulo CSG: si este algoritmo es
// correcto y robusto, todo el resto (booleanos en F2H12, primitivas
// en F2H13, etc.) se construye encima sin sufrir.
//
// UV en F2H11: proyeccion planar trivial (mapear U, V al par de
// ejes mundiales mas perpendiculares a la normal de la cara). En
// F2H14 se reemplaza por UVs reales con lock-to-world.

#include "core/Types.h"
#include "engine/world/csg/Brush.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mood::Csg {

struct BrushMeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    u32 materialIndex;
};

struct BrushMeshData {
    std::vector<BrushMeshVertex> vertices;
    std::vector<u32> indices;  // triangulos: 3 indices por triangulo
};

/// @brief Construye la mesh triangulada del brush. Si el brush es
///        degenerado (< 4 caras o caras que no producen poligono
///        valido) la mesh resultante puede tener menos vertices /
///        ningun indice — el llamador debe chequear `data.indices.size()`.
BrushMeshData buildBrushMesh(const Brush& brush);

} // namespace Mood::Csg
