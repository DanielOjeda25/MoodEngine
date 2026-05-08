// F2H24 Bloque C: parseo de animaciones del MeshLoader.
// parseAnimations — convierte aiAnimation -> AnimationClip con tracks
// per-bone (position/rotation/scale keys), filtrando tracks de nodos
// que no son bones del esqueleto.

#include "engine/assets/loaders/MeshLoader_Internal.h"

namespace Mood::detail {

void parseAnimations(const aiScene& scene,
                      const std::unordered_map<std::string, int>& indexByName,
                      std::vector<AnimationClip>& outClips) {
    for (u32 ai = 0; ai < scene.mNumAnimations; ++ai) {
        const aiAnimation* anim = scene.mAnimations[ai];
        if (anim == nullptr) continue;

        // Convencion: si mTicksPerSecond es 0 (algunos exporters), asumir 25.
        const float tps = (anim->mTicksPerSecond > 0.0)
                           ? static_cast<float>(anim->mTicksPerSecond)
                           : 25.0f;

        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        if (clip.name.empty()) clip.name = "anim_" + std::to_string(ai);
        clip.duration = static_cast<float>(anim->mDuration) / tps;
        clip.tracks.reserve(anim->mNumChannels);

        for (u32 ci = 0; ci < anim->mNumChannels; ++ci) {
            const aiNodeAnim* ch = anim->mChannels[ci];
            if (ch == nullptr) continue;
            auto it = indexByName.find(ch->mNodeName.C_Str());
            if (it == indexByName.end()) continue; // canal de un nodo no-bone

            BoneTrack tr;
            tr.boneIndex = it->second;
            tr.positionKeys.reserve(ch->mNumPositionKeys);
            tr.rotationKeys.reserve(ch->mNumRotationKeys);
            tr.scaleKeys.reserve(ch->mNumScalingKeys);

            for (u32 k = 0; k < ch->mNumPositionKeys; ++k) {
                const auto& key = ch->mPositionKeys[k];
                VectorKey v;
                v.time  = static_cast<float>(key.mTime) / tps;
                v.value = toGlm(key.mValue);
                tr.positionKeys.push_back(v);
            }
            for (u32 k = 0; k < ch->mNumRotationKeys; ++k) {
                const auto& key = ch->mRotationKeys[k];
                QuatKey q;
                q.time  = static_cast<float>(key.mTime) / tps;
                q.value = toGlm(key.mValue);
                tr.rotationKeys.push_back(q);
            }
            for (u32 k = 0; k < ch->mNumScalingKeys; ++k) {
                const auto& key = ch->mScalingKeys[k];
                VectorKey v;
                v.time  = static_cast<float>(key.mTime) / tps;
                v.value = toGlm(key.mValue);
                tr.scaleKeys.push_back(v);
            }
            clip.tracks.push_back(std::move(tr));
        }

        outClips.push_back(std::move(clip));
    }
}

} // namespace Mood::detail
