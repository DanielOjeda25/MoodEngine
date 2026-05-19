#include "systems/physics/RagdollSystem.h"

#include "core/Log.h"
#include "engine/animation/skeleton/Skeleton.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/physics/ragdoll/RagdollLayout.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>

#include <vector>

namespace Mood::RagdollSystem {

namespace {

// Helper: convierte las skinning matrices del Animator a bone globals en
// mesh-space (necesario para sembrar el ragdoll). La relacion es:
//   skinning[i] = global[i] * inverseBind[i]
// => global[i] = skinning[i] * inverse(inverseBind[i]) = skinning[i] * bind[i]
std::vector<glm::mat4> skinningToMeshGlobals(
    const Skeleton& skel,
    const std::vector<glm::mat4>& skinning) {
    std::vector<glm::mat4> globals(skel.bones.size(), glm::mat4(1.0f));
    if (skinning.size() != skel.bones.size()) return globals;
    for (usize i = 0; i < skel.bones.size(); ++i) {
        const glm::mat4 bind = glm::inverse(skel.bones[i].inverseBind);
        globals[i] = skinning[i] * bind;
    }
    return globals;
}

// Para cada ragdoll bone del layout, devuelve su pose en WORLD space
// (usando entity worldMatrix + bone mesh global).
std::vector<glm::mat4> computeRagdollPartWorldTransforms(
    const ragdoll::RagdollLayout& layout,
    const std::vector<glm::mat4>& boneMeshGlobals,
    const glm::mat4& entityWorldMatrix) {
    std::vector<glm::mat4> out(layout.bones.size(), glm::mat4(1.0f));
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const int boneIdx = layout.bones[i].boneIndex;
        if (boneIdx < 0 ||
            boneIdx >= static_cast<int>(boneMeshGlobals.size())) {
            // Bone faltante — usamos solo el entity world (degenerado pero
            // no crashea Jolt).
            out[i] = entityWorldMatrix;
            continue;
        }
        out[i] = entityWorldMatrix * boneMeshGlobals[boneIdx];
    }
    return out;
}

// Sync inverso: las world poses del ragdoll (post-step) -> mesh globals ->
// skinning matrices. Bones que NO estan en el ragdoll heredan del parent
// mas cercano que SI esta (manos, pies, dedos siguen el forearm/leg).
void writeRagdollPoseToSkeleton(
    const Skeleton& skel,
    const ragdoll::RagdollLayout& layout,
    const std::vector<glm::mat4>& ragdollWorlds,
    const glm::mat4& entityWorldMatrix,
    std::vector<glm::mat4>& outSkinning) {
    const usize n = skel.bones.size();
    outSkinning.assign(n, glm::mat4(1.0f));

    // Map boneIdx -> ragdoll part idx, -1 si el bone NO esta en el ragdoll.
    std::vector<int> boneToPart(n, -1);
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const int bIdx = layout.bones[i].boneIndex;
        if (bIdx >= 0 && bIdx < static_cast<int>(n)) {
            boneToPart[bIdx] = static_cast<int>(i);
        }
    }

    // Computamos globals en mesh-space (descontando entity transform) en
    // orden topologico (parent antes que child — garantizado por Skeleton).
    const glm::mat4 invEntity = glm::inverse(entityWorldMatrix);
    std::vector<glm::mat4> meshGlobals(n, glm::mat4(1.0f));

    for (usize i = 0; i < n; ++i) {
        const auto& bone = skel.bones[i];
        if (boneToPart[i] >= 0) {
            // Bone ragdolleado: world -> mesh-space.
            meshGlobals[i] = invEntity * ragdollWorlds[boneToPart[i]];
        } else if (bone.parent < 0) {
            // Root sin ragdoll (no deberia pasar si Hips esta presente,
            // defensivo igual): cae al bind local.
            meshGlobals[i] = bone.localBindTransform;
        } else {
            // Bone "montado": hereda parent y aplica su localBind.
            meshGlobals[i] = meshGlobals[static_cast<usize>(bone.parent)]
                              * bone.localBindTransform;
        }
        outSkinning[i] = meshGlobals[i] * bone.inverseBind;
    }
}

} // anonymous

void tick(Scene& scene, PhysicsWorld& physicsWorld, AssetManager& assets) {
    // Buffer reusado entre entities (todas las iteraciones son secuenciales).
    std::vector<glm::mat4> partTransforms;
    std::vector<glm::mat4> ragdollWorlds;

    scene.forEach<RagdollComponent, AnimatorComponent, SkeletonComponent,
                   MeshRendererComponent, TransformComponent>(
        [&](Entity, RagdollComponent& rag, AnimatorComponent& anim,
            SkeletonComponent& skel, MeshRendererComponent& mr,
            TransformComponent& tf) {
            // No-op si animado (sin ragdoll que tocar) o sin mesh.
            if (rag.state == RagdollComponent::State::Animated) return;

            const MeshAsset* mesh = assets.getMesh(mr.mesh);
            if (mesh == nullptr || !mesh->skeleton.has_value()) return;
            const Skeleton& skeleton = *mesh->skeleton;
            if (skeleton.empty()) return;

            const glm::mat4 entityW = tf.worldMatrix();

            // --- TRANSICION Animated -> Ragdolling ---
            // Detectada por state==Ragdolling pero ragdollId todavia 0.
            if (rag.ragdollId == 0) {
                // 1) Construir layout desde el skeleton del mesh.
                const auto layout = ragdoll::buildMixamoLayout(
                    skeleton, rag.totalMass, rag.limbRadius);
                if (layout.empty()) {
                    // Layout vacio (skeleton no-Mixamo o bones faltantes).
                    // Volvemos a Animated y avisamos.
                    rag.state = RagdollComponent::State::Animated;
                    Log::physics()->warn(
                        "RagdollSystem: layout vacio para el mesh actual; "
                        "ragdoll desactivado (revisar bones Mixamo).");
                    return;
                }

                // 2) Bone mesh globals desde las skinning matrices actuales
                //    (el Animator las dejo del frame previo).
                const auto boneGlobals = skinningToMeshGlobals(
                    skeleton, skel.skinningMatrices);

                // 3) Part world transforms (mesh global * entity).
                partTransforms = computeRagdollPartWorldTransforms(
                    layout, boneGlobals, entityW);

                // 4) Create ragdoll.
                rag.ragdollId = physicsWorld.createRagdoll(
                    layout, partTransforms, rag.useGravity);
                if (rag.ragdollId == 0) {
                    rag.state = RagdollComponent::State::Animated;
                    Log::physics()->error(
                        "RagdollSystem: createRagdoll fallo; ragdoll "
                        "desactivado.");
                    return;
                }

                // 5) Pausar el Animator — desde ahora el ragdoll comanda.
                anim.playing = false;

                // 6) Spawn impulse opcional al torso (Spine2 = index 3 del
                //    layout estandar Mixamo). Solo si magnitud > 0.
                if (glm::length(rag.spawnImpulse) > 1e-4f
                    && layout.torsoIndex >= 0) {
                    physicsWorld.applyRagdollImpulse(
                        rag.ragdollId, layout.torsoIndex, rag.spawnImpulse);
                }
                // No-return aca: continuamos al sync para que el primer
                // frame ya tenga el mesh deformado por el ragdoll (evita
                // pop visual).
            }

            // --- SYNC ragdoll -> skeleton ---
            if (rag.ragdollId != 0) {
                if (!physicsWorld.readRagdollPose(rag.ragdollId, ragdollWorlds)) {
                    return;
                }
                // Rebuild layout transient — no lo guardamos en el component
                // para no atar el lifecycle del struct al PhysicsWorld.
                const auto layout = ragdoll::buildMixamoLayout(
                    skeleton, rag.totalMass, rag.limbRadius);
                if (layout.bones.size() != ragdollWorlds.size()) return;
                writeRagdollPoseToSkeleton(skeleton, layout, ragdollWorlds,
                                            entityW, skel.skinningMatrices);
            }
        });
}

} // namespace Mood::RagdollSystem
