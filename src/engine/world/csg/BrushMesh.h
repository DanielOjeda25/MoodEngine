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
// UV F2H15: cada cara guarda UV params (uAxis, vAxis, uvOffset,
// uvScale, uvRotation, lockToWorld). Las UVs se computan en el
// build de la mesh proyectando cada vertex sobre los ejes
// tangentes de su cara y aplicando los params. Si lockToWorld
// esta activo, la posicion world (post-transform) se usa para
// la proyeccion en lugar de la local — la textura "queda fija"
// al mundo y no se deforma al mover el brush.

#include "core/Types.h"
#include "engine/world/csg/Brush.h"

#include <glm/mat4x4.hpp>
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
///
///        @param worldMatrix F2H15: matriz local->world del brush.
///                Cuando alguna cara tiene `lockToWorld=true`, el
///                vertex se transforma por esta matriz ANTES de
///                proyectar sobre los ejes tangentes. Si todas las
///                caras tienen `lockToWorld=false`, la matriz se
///                ignora y el caller puede pasar identity.
BrushMeshData buildBrushMesh(const Brush& brush,
                              const glm::mat4& worldMatrix = glm::mat4(1.0f));

/// @brief Convierte el BrushMeshData (vertices+indices) al layout
///        interleaved que consume el shader PBR del motor:
///          pos.xyz (3), color.rgb (3), uv.xy (2), normal.xyz (3) = 11 floats.
///        Expande los indices: cada vertice del triangulo aparece
///        duplicado en el buffer (no usamos EBOs en este path,
///        igual que createCubeMesh). El color se setea en blanco
///        (1,1,1) para que el `albedoTint` del material domine
///        (mismo patron que createSphereMesh).
///        Resultado: `vertices.size() == indices.size() * 11`.
std::vector<f32> brushMeshDataToInterleaved(const BrushMeshData& data);

} // namespace Mood::Csg
