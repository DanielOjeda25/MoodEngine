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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <vector>

namespace Mood::Csg {

/// @brief F2H15: ejes tangentes ortonormales canonicos derivados
///        de la normal de un plano. Estables: depende solo de
///        la normal (mismo input -> mismo output). Usado por las
///        primitivas para inicializar los UV params de cada cara.
///        Algoritmo: elegir un helper axis (X o Y segun donde
///        apunta la normal); uAxis = normalize(cross(helper, n));
///        vAxis = normalize(cross(n, uAxis)).
void defaultTangentBasis(const glm::vec3& normal,
                          glm::vec3& outUAxis,
                          glm::vec3& outVAxis);

struct BrushFace {
    Plane plane;
    u32 materialIndex = 0;
    // F2H15: UV params per-cara. Defaults razonables (ejes
    // canonicos +X/+Y, scale unitario, sin offset, sin rotacion,
    // lock-to-world apagado). Las primitivas (makeBoxBrush, etc.)
    // sobrescriben uAxis/vAxis con defaultTangentBasis(normal) al
    // construirse para que las UVs salgan alineadas con la cara.
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, 1.0f, 0.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};
    glm::vec2 uvScale{1.0f, 1.0f};
    f32 uvRotation = 0.0f;        // radianes
    bool lockToWorld = false;
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

/// @brief F2H17: devuelve el poligono de UNA cara del brush en
///        WORLD space (vertices transformados por worldMatrix). El
///        orden de los vertices NO esta garantizado CCW; el caller
///        debe ordenar si necesita render. Vacio si la cara es
///        invalida (faceIndex fuera de rango o cara degenerada).
///
///        Usado por el outline render del face mode (E2H17 Bloque D)
///        y por pickFace internamente.
std::vector<glm::vec3> collectFaceWorldPolygon(const Brush& brush,
                                                  u32 faceIndex,
                                                  const glm::mat4& worldMatrix = glm::mat4(1.0f));

/// @brief F2H17: ray-cast contra los polígonos individuales de cada
///        cara del brush en world space. Devuelve el INDICE de la
///        cara mas cercana al rayOrigin, o nullopt si el rayo no
///        toca ninguna cara.
///
///        Algoritmo: para cada cara, compute el poligono local
///        (intersectThreePlanes filtrando puntos del brush), lo
///        transforma a world via worldMatrix, lo triangula con fan
///        desde el centroide, y aplica ray-triangle (Moller-Trumbore)
///        a cada triangulo. Trackea el `t` minimo positivo.
///
///        Usado por el viewport picking en Face Mode (F2H17).
std::optional<u32> pickFace(const Brush& brush,
                              const glm::vec3& rayOrigin,
                              const glm::vec3& rayDir,
                              const glm::mat4& worldMatrix = glm::mat4(1.0f));

/// @brief F2H30 Bloque B: enumera vertices unicos del brush con sus
///        planos incidentes. Triple loop intersectThreePlanes + filter
///        contra otros planos + dedup por proximidad. Para vertices
///        donde > 3 planos coinciden (ej. cilindro con cap y N caras
///        laterales en el polo), agrega TODOS los planos al vertex
///        unificado. Vacio para brushes degenerados (< 4 caras).
struct BrushVertex {
    glm::vec3 localPos{0.0f};
    std::vector<u32> planeIndices;
};
std::vector<BrushVertex> enumerateBrushVertices(const Brush& brush);

/// @brief F2H30 Bloque B: vertex picking de un brush en screen space
///        (NDC). Util para Vertex Mode del editor de mapas — picking
///        contra el grid de vertices en lugar de raycast volumetrico.
///        Algoritmo: enumera vertices unicos del brush (triple loop
///        intersectThreePlanes + filter + dedup), proyecta cada uno a
///        NDC via worldMatrix * view * projection, devuelve el mas
///        cercano dentro del `thresholdNdc` (default ~4 px en viewport
///        720p). Tambien registra los planos incidentes para que el
///        drag pueda mutarlos manteniendo isBrushValid.
struct VertexPick {
    glm::vec3 worldPos{0.0f};
    glm::vec3 localPos{0.0f};
    std::vector<u32> planeIndices;  // 3 para box; mas para cylinder/etc.
};
std::optional<VertexPick> pickVertex(const Brush& brush,
                                       const glm::mat4& worldMatrix,
                                       const glm::mat4& view,
                                       const glm::mat4& projection,
                                       f32 ndcX, f32 ndcY,
                                       f32 thresholdNdc = 0.04f);

/// @brief F2H30 Bloque B: edge picking de un brush en screen space.
///        Edge = interseccion de exactamente 2 caras. Para cada par
///        (i, j) de caras, encuentra vertices que esten en AMBAS;
///        si son exactamente 2, es un edge. Proyecta endpoints a NDC,
///        computa distancia punto-segmento. Devuelve el edge mas
///        cercano dentro del `thresholdNdc`.
struct EdgePick {
    glm::vec3 worldA{0.0f}, worldB{0.0f};
    glm::vec3 localA{0.0f}, localB{0.0f};
    u32 planeA = 0, planeB = 0;  // los 2 planos compartidos por el edge.
};
std::optional<EdgePick> pickEdge(const Brush& brush,
                                   const glm::mat4& worldMatrix,
                                   const glm::mat4& view,
                                   const glm::mat4& projection,
                                   f32 ndcX, f32 ndcY,
                                   f32 thresholdNdc = 0.04f);

} // namespace Mood::Csg
