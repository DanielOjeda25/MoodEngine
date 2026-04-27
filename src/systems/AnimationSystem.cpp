#include "systems/AnimationSystem.h"

#include "engine/animation/AnimationClip.h"
#include "engine/animation/Skeleton.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/MeshAsset.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Mood {

namespace {

// Devuelve el clip activo del MeshAsset segun el name del Animator. Si
// `clipName` es vacio o no matchea, cae al primer clip. nullptr si el
// asset no trae animaciones.
const AnimationClip* resolveActiveClip(const MeshAsset& mesh,
                                        const AnimatorComponent& anim) {
    if (mesh.animations.empty()) return nullptr;
    if (!anim.clipName.empty()) {
        for (const AnimationClip& c : mesh.animations) {
            if (c.name == anim.clipName) return &c;
        }
    }
    return &mesh.animations.front();
}

// Buffer scratch reutilizable para evitar realloc por entidad. No es
// thread-safe, pero el `update` corre single-threaded en el hilo del
// render — no hay races por ahora.
thread_local std::vector<glm::mat4> g_localPose;

} // namespace

void AnimationSystem::update(Scene& scene, AssetManager& assets, f32 dt) {
    scene.forEach<MeshRendererComponent, AnimatorComponent, SkeletonComponent>(
        [&](Entity, MeshRendererComponent& mr, AnimatorComponent& anim,
            SkeletonComponent& skel) {
            MeshAsset* asset = assets.getMesh(mr.mesh);
            if (asset == nullptr || !asset->hasSkeleton()) {
                skel.skinningMatrices.clear();
                return;
            }
            const Skeleton& skeleton = *asset->skeleton;

            const AnimationClip* clip = resolveActiveClip(*asset, anim);
            if (clip == nullptr || clip->duration <= 0.0f) {
                // Sin clips: pose en bind. Igual computamos el skinning para
                // que el shader skinneado vea matrices identity-equivalent.
                g_localPose.assign(skeleton.bones.size(), glm::mat4(1.0f));
                for (usize i = 0; i < skeleton.bones.size(); ++i) {
                    g_localPose[i] = skeleton.bones[i].localBindTransform;
                }
                computeSkinningMatrices(skeleton, g_localPose,
                                         skel.skinningMatrices);
                return;
            }

            // Avanzar el tiempo. `playing=false` lo congela; `speed` escala.
            if (anim.playing) {
                anim.time += dt * anim.speed;
            }
            // Loop / clamp.
            const float dur = clip->duration;
            if (anim.loop) {
                // fmod con guard contra dt negativo (rewind). Aseguramos
                // que `time` quede en [0, dur).
                anim.time = std::fmod(anim.time, dur);
                if (anim.time < 0.0f) anim.time += dur;
            } else {
                anim.time = std::clamp(anim.time, 0.0f, dur);
            }

            clip->evaluate(anim.time, skeleton, g_localPose);
            computeSkinningMatrices(skeleton, g_localPose,
                                     skel.skinningMatrices);
        });
}

} // namespace Mood
