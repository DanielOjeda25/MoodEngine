#pragma once

// Compilador de brushes -> mesh estatica unificada (F2H20). Toma un
// conjunto de brushes con sus transforms world + paths logicos de
// material, y produce una mesh estatica:
//   - 1 submesh por path logico de material en uso (no por brush ni por
//     slot del brush — los caras con el mismo path se fusionan).
//   - Caras internas (pareja exacta entre dos brushes adyacentes con
//     normales antiparalelas y poligonos coincidentes) se eliminan.
//   - Vertices coplanares se sueldan globalmente con un epsilon en
//     world space (default `1e-3`). El match incluye position + UV +
//     normal: dos vertices en la misma posicion pero con UV distinta
//     no se sueldan (split estilo OBJ).
//
// La compilacion es **on-demand**: F2H20 NO la persiste en `.moodmap`
// ni la usa el runtime del MoodPlayer (sigue cargando los brushes
// individuales). El uso real es:
//   - Stats dialog desde menu `Mapa > Compilar mapa`.
//   - Export OBJ desde menu `Mapa > Exportar OBJ...` (ver
//     `MapExportObj.h`).
//
// Cull de caras (2 pasadas):
//   1. F2H20 — pareja exacta (poligonos identicos +-eps, normales
//      antiparalelas). Caras totalmente compartidas entre 2 brushes
//      pegados se eliminan ambas.
//   2. F2H25 — overlap parcial. Cada cara sobreviviente del paso 1 se
//      clipea contra los planos de cada otro brush usando BSP-style
//      polygon splitting (Sutherland-Hodgman extendido). La parte que
//      cae dentro del otro brush se descarta; la parte que queda fuera
//      se conserva (puede dividirse en N fragmentos). Reportado en
//      stats `culledOverlapTriangles` y `splitFragments`.

#include "core/Types.h"
#include "engine/world/csg/Brush.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {
class AssetManager;
class Scene;
}

namespace Mood::Csg {

/// @brief Vertice del compilado. Position y normal en world space; UV
///        ya con uvOffset/uvScale/uvRotation aplicados (igual que el
///        runtime via `buildBrushMesh`).
struct CompiledVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct CompiledSubmesh {
    /// @brief Path logico del material ("textures/grid.png", "" si la
    ///        cara no tiene material asignado, o `__default_material`
    ///        si esta apuntando al sentinel del AssetManager).
    std::string materialPath;
    std::vector<CompiledVertex> vertices;
    std::vector<u32> indices;  // triangle list, post-weld
};

struct CompileStats {
    u32 sourceBrushes = 0;
    u32 brushesSkipped = 0;       // < 4 caras o sin caras validas
    u32 sourceFaces = 0;          // total de caras antes de cull
    u32 culledInternalFaces = 0;
    /// @brief F2H25: triangulos descartados de caras que cayeron
    ///        ENTERAMENTE dentro de otro brush. Cada cara descartada
    ///        contribuye `(verts - 2)` tris (fan triangulation). NO
    ///        cuenta los tris ahorrados por splits parciales — el BSP
    ///        clipping subdivide la cara en N fragmentos cuya suma de
    ///        tris puede SUPERAR el original (cada fragmento
    ///        independiente requiere su propia triangulacion). El
    ///        beneficio del split es menor area visible, no menor
    ///        tri-count global.
    u32 culledOverlapTriangles = 0;
    /// @brief F2H25: caras que quedaron partidas en >1 fragmento por
    ///        intersectar parcialmente con otro brush (un quad cortado
    ///        por una pared queda en 2-3 fragmentos). Una cara que cae
    ///        completamente adentro o completamente afuera no cuenta
    ///        aca (cae adentro = solo culledOverlapTriangles; afuera =
    ///        ninguna stat). Stat orientativa: si el mapa tiene N
    ///        splits, hay overlap real entre brushes.
    u32 splitFragments = 0;
    u32 totalTriangles = 0;
    u32 totalVerticesPreWeld = 0;
    u32 totalVerticesUnique = 0;  // post-weld
    u32 submeshCount = 0;          // = paths distintos en uso
};

struct CompiledMap {
    std::vector<CompiledSubmesh> submeshes;
    CompileStats stats;
};

/// @brief Input al compilador: un brush + su transform world + el path
///        logico por slot de material (`face.materialIndex` indexea
///        este vector).
struct BrushSource {
    Brush brush;
    glm::mat4 worldMatrix{1.0f};
    std::vector<std::string> materialPaths;
};

/// @brief Cara del compilador con todos los datos necesarios para cull
///        + triangulacion. Se construye en `collectFaces`.
struct RawBrushFace {
    u32 brushIndex = 0;
    u32 faceIndex = 0;
    std::string materialPath;
    /// @brief Vertices del poligono de la cara, en world space, ordenados
    ///        CCW visto desde +worldNormal.
    std::vector<glm::vec3> worldPolygonCcw;
    glm::vec3 worldNormal{0.0f, 1.0f, 0.0f};
    /// @brief UV params per-cara (ejes en LOCAL space del brush). Para
    ///        computar UV de cada vertex el compilador transforma la
    ///        position world -> local con `inverse(worldMatrix)` cuando
    ///        `lockToWorld == false` (mismo flow que `buildBrushMesh`).
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, 1.0f, 0.0f};
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};
    f32       uvRotation = 0.0f;
    bool      lockToWorld = false;
    glm::mat4 worldMatrix{1.0f};
};

/// @brief Recolecta las caras de cada brush en world space, ordenadas
///        CCW. Saltea brushes degenerados (< 4 caras) y caras que no
///        producen poligono valido (< 3 vertices). El orden del output
///        es por brush (todas las caras del brush 0, luego brush 1, etc.).
std::vector<RawBrushFace> collectFaces(const std::vector<BrushSource>& sources);

/// @brief Marca como "cara interna" cada par de caras que cumplen:
///        - dot(normal_i, normal_j) < -1 + eps  (antiparalelas).
///        - Mismo set de vertices ±eps (orden libre).
///        Devuelve un vector<bool> paralelo al input. `true` => quitar.
std::vector<bool> markInternalFaces(const std::vector<RawBrushFace>& faces,
                                      f32 epsilon = 1e-3f);

/// @brief Compila la lista de brushes a una mesh estatica unificada.
///        Garantiza:
///        - Vertices en world space.
///        - 1 submesh por materialPath distinto en uso (en orden de
///          aparicion de la primera cara con ese path).
///        - Triangulos por fan-triangulation desde el primer vertice
///          de cada cara CCW.
///        - Weld global con `weldEpsilon` (match incluye position + UV +
///          normal).
///        - `stats` reporta brushes / caras / vertices pre-weld y
///          post-weld / culled internas.
CompiledMap compileMap(const std::vector<BrushSource>& sources,
                        f32 weldEpsilon = 1e-3f);

/// @brief Helper de conveniencia: itera el scene buscando entidades con
///        TransformComponent + BrushComponent y arma el vector de
///        `BrushSource`. Resuelve material slot -> path logico via
///        `AssetManager::materialPathOf`. El sentinel de slot 0
///        (`__default_material`) se preserva tal cual; el caller decide
///        como manifestarlo en el OBJ/MTL.
std::vector<BrushSource> collectBrushSourcesFromScene(
    Scene& scene, AssetManager& assets);

/// @brief F2H26: convierte UN `CompiledSubmesh` al layout PBR interleaved
///        (pos.xyz + color.rgb=1 + uv.xy + normal.xyz = 11 floats por
///        vertex), expandiendo los indices (cada vertice del triangulo
///        aparece duplicado en el output). Mismo formato que
///        `brushSubmeshToInterleaved` para que el SceneRenderer use el
///        shader PBR estandar y el `kPbrAttrs` de los brushes runtime.
///        Usado por el editor al persistir la mesh compilada en
///        `.moodmap` v13.
std::vector<f32> compiledSubmeshToInterleaved(const CompiledSubmesh& sub);

} // namespace Mood::Csg
