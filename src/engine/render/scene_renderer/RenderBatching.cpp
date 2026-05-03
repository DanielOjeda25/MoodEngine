#include "engine/render/scene_renderer/RenderBatching.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

namespace Mood {

BatchingResult groupByBatch(Scene& scene,
                              AssetManager& assets,
                              const Frustum& frustum) {
    BatchingResult result;

    scene.forEach<TransformComponent, MeshRendererComponent>(
        [&](Entity e, TransformComponent& t, MeshRendererComponent& mr) {
            // Skinned: lo maneja el pase aparte. Lo ignoramos aca y NO lo
            // metemos en non-batchable porque no debe pasar por el
            // path PBR static.
            if (e.hasComponent<SkeletonComponent>()) return;

            // Mesh asset null -> entidad pasa al fallback (path no-instanced
            // del F2H3). Eso preserva el comportamiento existente: meshes
            // invalidos se renderean igual con el "missing mesh".
            const MeshAsset* asset = assets.getMesh(mr.mesh);
            if (asset == nullptr) {
                result.nonBatchable.push_back(e);
                return;
            }

            // Frustum cull: AABB world-space del mesh. Si esta fuera, no
            // entra a ningun lado (ni batch ni non-batchable). Mismo
            // comportamiento que F2H3.
            const AABB localAabb{asset->aabbMin, asset->aabbMax};
            const glm::mat4 worldMat = t.worldMatrix();
            if (localAabb.isValid()) {
                const AABB worldBox = worldAabb(localAabb, worldMat);
                if (!aabbVisible(worldBox, frustum)) {
                    ++result.culledCount;
                    return;
                }
            }

            // Decision de "batcheable":
            //   - exactamente 1 submesh (ej. cubo primitivo, esfera, props
            //     simples). Multi-submesh requeriria batch por submesh con
            //     materiales independientes — postergado.
            //   - materials.size() <= 1 (todos los submeshes ven el mismo
            //     material; con 1 submesh esto es trivialmente cierto).
            const bool oneSubmesh = (asset->submeshes.size() == 1u);
            const bool simpleMaterials = (mr.materials.size() <= 1u);
            if (!(oneSubmesh && simpleMaterials)) {
                result.nonBatchable.push_back(e);
                return;
            }

            const MaterialAssetId matId = mr.materialOrMissing(0);
            const BatchKey key{mr.mesh, matId};
            result.batches[key].models.push_back(worldMat);
        });

    return result;
}

} // namespace Mood
