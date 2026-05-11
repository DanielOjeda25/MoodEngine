#include "systems/animation/AnimationSystem.h"

#include "engine/animation/clips/AnimationClip.h"
#include "engine/animation/skeleton/Skeleton.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Mood {

namespace {

/// @brief Resultado de la resolucion del clip activo. `remap` no-null indica
///        un clip standalone (F2H49) que debe evaluarse con
///        `evaluateClipWithRemap`; null = clip embedded del MeshAsset y se
///        usa la `evaluate()` interna que lee `track.boneIndex`.
struct ClipResolution {
    const AnimationClip* clip = nullptr;
    const std::vector<int>* remap = nullptr;
};

/// @brief Resuelve el clip activo del Animator con prioridad:
///        1) Alias en `externalClips` (F2H49) → clip standalone del
///           AssetManager + remap cacheado en `externalBindCache`.
///        2) Match por nombre en `mesh.animations` (clips embedded).
///        3) Primer clip embedded como default.
///
///        El bind contra el esqueleto se hace lazy: la primera vez que
///        un clipId aparece (o cuando el cache quedo stale por tamano)
///        se llena `externalBindCache[clipId]` via `bindClipToSkeleton`.
///        La invalidacion ante cambio de skeleton es responsabilidad del
///        consumidor (limpia `externalBindCache.clear()` al swappear mesh).
ClipResolution resolveActiveClip(const MeshAsset& mesh,
                                  AnimatorComponent& anim,
                                  AssetManager& assets) {
    if (!anim.clipName.empty() && mesh.skeleton.has_value()) {
        for (const auto& [alias, clipId] : anim.externalClips) {
            if (alias != anim.clipName) continue;
            const AnimationClip* clip = assets.getAnimationClip(clipId);
            if (clip == nullptr || clip->tracks.empty()) break;
            std::vector<int>& remap = anim.externalBindCache[clipId];
            if (remap.size() != clip->tracks.size()) {
                bindClipToSkeleton(*clip, *mesh.skeleton, remap);
            }
            return {clip, &remap};
        }
    }
    if (mesh.animations.empty()) return {};
    if (!anim.clipName.empty()) {
        for (const AnimationClip& c : mesh.animations) {
            if (c.name == anim.clipName) return {&c, nullptr};
        }
    }
    return {&mesh.animations.front(), nullptr};
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

            const ClipResolution res = resolveActiveClip(*asset, anim, assets);
            const AnimationClip* clip = res.clip;
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

            if (res.remap != nullptr) {
                evaluateClipWithRemap(*clip, anim.time, skeleton, *res.remap,
                                       g_localPose);
            } else {
                clip->evaluate(anim.time, skeleton, g_localPose);
            }
            computeSkinningMatrices(skeleton, g_localPose,
                                     skel.skinningMatrices);
        });
}

} // namespace Mood
