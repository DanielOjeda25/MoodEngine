#pragma once

// Agrupacion de entidades del pase opaco estatico para instanced rendering
// (F2H4). Este modulo es PURO: no depende de OpenGL ni de ningun backend
// grafico. Toma una `Scene + AssetManager + Frustum` y devuelve los batches
// agrupados por `(mesh, material)` mas la lista de entidades non-batchable
// que necesitan el path no-instanced (fallback del F2H3).
//
// Una entidad es batcheable si:
//   - tiene MeshRendererComponent + TransformComponent + NO tiene SkeletonComponent.
//   - el mesh asset existe en el AssetManager.
//   - el mesh tiene exactamente 1 submesh.
//   - `materials.size() <= 1` (todos los submeshes ven el mismo material).
//   - el AABB world-space pasa el frustum cull (helper de F2H3).
//
// Las entidades que no cumplen alguna de las condiciones de "batcheable"
// (excepto el cull, que las descarta del todo) caen a `nonBatchable` y
// el SceneRenderer las dibuja con el path non-instanced existente.

#include "core/Types.h"
#include "engine/render/pipeline/Frustum.h"
#include "engine/scene/core/Entity.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Mood {

class AssetManager;
class Scene;

using MeshAssetId = u32;
using MaterialAssetId = u32;

/// @brief Clave de agrupacion. Dos entidades con la misma key pueden
///        dibujarse en un solo `glDrawArraysInstanced`.
///        F2H6: incluye `lod` — entidades del mismo (mesh, material)
///        pero distinto LOD van a batches separados.
struct BatchKey {
    MeshAssetId mesh{0};
    MaterialAssetId material{0};
    u32 lod{0};

    bool operator==(const BatchKey& other) const noexcept {
        return mesh == other.mesh
            && material == other.material
            && lod == other.lod;
    }
};

struct BatchKeyHash {
    std::size_t operator()(const BatchKey& k) const noexcept {
        // Mezcla simple — tipo boost::hash_combine. Para los rangos
        // tipicos de IDs (<10K cada uno) la dispersion es buena.
        return std::hash<u32>{}(k.mesh)
             ^ (std::hash<u32>{}(k.material) << 1)
             ^ (std::hash<u32>{}(k.lod) << 2);
    }
};

/// @brief Lista de matrices `model` para todas las instancias visibles
///        que comparten una `BatchKey`.
struct InstanceBatch {
    std::vector<glm::mat4> models;
};

using BatchMap = std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash>;

/// @brief Resultado de la agrupacion: batches listos para draw instanced
///        + lista de entidades que cayeron al path non-instanced.
struct BatchingResult {
    BatchMap batches;
    /// Entidades que el SceneRenderer debe dibujar con el path
    /// individual (mismo loop que F2H3, ya con cull aplicado).
    std::vector<Entity> nonBatchable;
    /// Conteo de entidades descartadas por el frustum cull (mismo
    /// numero que F2H3 reportaba con `PBR::CulledStatic`).
    u32 culledCount{0};
};

/// @brief Recorre la escena y produce batches agrupados + non-batchable +
///        culled. PURO: no toca GL. Es la entrada del pipeline F2H3+F2H4+F2H6.
///        `cameraPos` se usa para elegir el LOD por entidad via
///        `selectLod(distance, asset->lodDistances)`. Si el mesh no tiene
///        LODs generados (skinned, muy chico, o cache invalido), todas las
///        entidades caen en `lod = 0`.
BatchingResult groupByBatch(Scene& scene,
                              AssetManager& assets,
                              const Frustum& frustum,
                              const glm::vec3& cameraPos);

} // namespace Mood
