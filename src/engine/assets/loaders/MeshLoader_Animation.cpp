// F2H24 Bloque C: parseo de animaciones del MeshLoader.
// parseAnimations — convierte aiAnimation -> AnimationClip con tracks
// per-bone (position/rotation/scale keys), filtrando tracks de nodos
// que no son bones del esqueleto.

#include "engine/assets/loaders/MeshLoader_Internal.h"

namespace Mood::detail {

// F2H49: limpia nombres de clip provenientes de exporters varios.
//
// assimp suele entregar nombres como:
//   "Armature|Take 001"                  (Blender)
//   "Armature|mixamo.com|Layer0"         (Mixamo FBX)
//   "mixamo.com"                         (Mixamo FBX simple)
//   "Take 001"                           (FBX legacy)
//
// Como `clip.name` se usa como key para `animator.play("idle")` desde
// scripting/UI, queremos un identificador estable y manejable. Reglas:
//   1. Si hay `|`, nos quedamos con el ultimo segmento (el real).
//   2. Reemplazar espacios, puntos y caracteres no alfanumericos por `_`.
//   3. Si tras limpiar queda vacio, devolver "" (el caller pone fallback).
std::string sanitizeClipName(std::string raw) {
    const auto pipe = raw.find_last_of('|');
    if (pipe != std::string::npos && pipe + 1 < raw.size()) {
        raw = raw.substr(pipe + 1);
    }
    std::string clean;
    clean.reserve(raw.size());
    for (char c : raw) {
        const bool alnum = (c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9');
        clean.push_back(alnum ? c : '_');
    }
    // Colapsar `__` consecutivos y trim de underscores en extremos.
    std::string out;
    out.reserve(clean.size());
    bool prevUnder = false;
    for (char c : clean) {
        if (c == '_') {
            if (!prevUnder && !out.empty()) out.push_back('_');
            prevUnder = true;
        } else {
            out.push_back(c);
            prevUnder = false;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

// F2H49: helper extraido — convierte un aiNodeAnim a BoneTrack. El caller
// resuelve `boneIndex` despues (parseAnimations: contra el skeleton del mesh;
// loader standalone: bind lazy en runtime). `boneName` se llena siempre.
BoneTrack convertChannel(const aiNodeAnim& ch, float tps) {
    BoneTrack tr;
    tr.boneName = ch.mNodeName.C_Str();
    tr.positionKeys.reserve(ch.mNumPositionKeys);
    tr.rotationKeys.reserve(ch.mNumRotationKeys);
    tr.scaleKeys.reserve(ch.mNumScalingKeys);

    for (u32 k = 0; k < ch.mNumPositionKeys; ++k) {
        const auto& key = ch.mPositionKeys[k];
        VectorKey v;
        v.time  = static_cast<float>(key.mTime) / tps;
        v.value = toGlm(key.mValue);
        tr.positionKeys.push_back(v);
    }
    for (u32 k = 0; k < ch.mNumRotationKeys; ++k) {
        const auto& key = ch.mRotationKeys[k];
        QuatKey q;
        q.time  = static_cast<float>(key.mTime) / tps;
        q.value = toGlm(key.mValue);
        tr.rotationKeys.push_back(q);
    }
    for (u32 k = 0; k < ch.mNumScalingKeys; ++k) {
        const auto& key = ch.mScalingKeys[k];
        VectorKey v;
        v.time  = static_cast<float>(key.mTime) / tps;
        v.value = toGlm(key.mValue);
        tr.scaleKeys.push_back(v);
    }
    return tr;
}

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
        clip.name = sanitizeClipName(anim->mName.C_Str());
        if (clip.name.empty()) clip.name = "anim_" + std::to_string(ai);
        clip.duration = static_cast<float>(anim->mDuration) / tps;
        clip.tracks.reserve(anim->mNumChannels);

        for (u32 ci = 0; ci < anim->mNumChannels; ++ci) {
            const aiNodeAnim* ch = anim->mChannels[ci];
            if (ch == nullptr) continue;
            const std::string nodeName = ch->mNodeName.C_Str();
            auto it = indexByName.find(nodeName);
            if (it == indexByName.end()) continue; // canal de un nodo no-bone

            BoneTrack tr = convertChannel(*ch, tps);
            tr.boneIndex = it->second;  // bind directo (skeleton conocido)
            clip.tracks.push_back(std::move(tr));
        }

        outClips.push_back(std::move(clip));
    }
}

} // namespace Mood::detail
