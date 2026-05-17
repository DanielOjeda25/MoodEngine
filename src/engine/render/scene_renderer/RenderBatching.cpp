#include "engine/render/scene_renderer/RenderBatching.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"  // F2H62 Bloque E
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/VisGroup.h"  // F2H33: hide gate
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#include <glm/geometric.hpp>  // glm::distance

namespace Mood {

BatchingResult groupByBatch(Scene& scene,
                              AssetManager& assets,
                              const Frustum& frustum,
                              const glm::vec3& cameraPos) {
    BatchingResult result;

    scene.forEach<TransformComponent, MeshRendererComponent>(
        [&](Entity e, TransformComponent& t, MeshRendererComponent& mr) {
            // F2H33: skipear entities en VisGroups hidden — no entran ni
            // al batch ni al non-batchable (no se rendean).
            if (isEntityHiddenByVisGroup(scene, e)) return;

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

            // F2H6: seleccion de LOD por distancia camera→entidad. La
            // distancia se calcula desde el centro del AABB world-space
            // (mas estable que t.position cuando hay rotaciones que mueven
            // el centro de masa visual). Si el mesh no tiene LOD para el
            // nivel calculado, `submeshesForLod()` cae a LOD 0 — el
            // BatchKey igual lo agrupa por nivel pedido para que las
            // entidades con/sin LOD del mismo mesh queden separadas
            // (asi el caller decide que IMesh dibujar con `submeshesForLod`).
            const glm::vec3 entityCenter = localAabb.isValid()
                ? glm::vec3(worldMat * glm::vec4(localAabb.center(), 1.0f))
                : glm::vec3(worldMat[3]); // fallback: traslacion pura
            const f32 distance = glm::distance(cameraPos, entityCenter);
            const u32 lod = selectLod(distance, asset->lodDistances);

            const MaterialAssetId matId = mr.materialOrMissing(0);

            // F2H62 Bloque E: materiales con shaderGraphPath caen al
            // path nonBatchable. Razon: el path instanced (A.1) usa
            // pbr_instanced.vert que el ShaderGraphCache v1 no soporta
            // (solo conoce pbr.vert estatico). Pasando por nonBatchable
            // (A.2) el cache si se aplica y el shader graph se renderea.
            // Cost: pierde el batching para estos materiales (pocos en
            // la practica -- shaders graph son para efectos especiales,
            // no para terrain tiles repetidos).
            const MaterialAsset* matAsset = assets.getMaterial(matId);
            if (matAsset != nullptr && !matAsset->shaderGraphPath.empty()) {
                result.nonBatchable.push_back(e);
                return;
            }

            const BatchKey key{mr.mesh, matId, lod};
            result.batches[key].models.push_back(worldMat);
        });

    return result;
}

} // namespace Mood
