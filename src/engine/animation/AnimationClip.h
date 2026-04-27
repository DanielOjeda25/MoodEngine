#pragma once

// AnimationClip (Hito 19): keyframes por hueso para una sola animacion
// (idle, walk, jump, etc.).
//
// Modelo simple: un `BoneTrack` por hueso del esqueleto, con tres listas
// independientes de keyframes (posicion, rotacion, escala). Se evalua a
// un tiempo `t` (segundos) interpolando linealmente entre keys vecinos.
// `t` fuera de [0, duration] queda clampeado al primer/ultimo key
// (la decision de loop vs clamp la toma `AnimationSystem`, no este header).
//
// LERP de quaternions normalizados — no SLERP. La diferencia es visible
// solo cuando los keys estan muy separados (>~30 grados); con clips
// densos como los de Mixamo es imperceptible y mucho mas barato.
//
// Header-only puro (sin GL ni assimp); testeable directo desde doctest.

#include "core/Types.h"
#include "engine/animation/Skeleton.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace Mood {

struct VectorKey {
    float time = 0.0f;
    glm::vec3 value{0.0f};
};

struct QuatKey {
    float time = 0.0f;
    glm::quat value{1.0f, 0.0f, 0.0f, 0.0f}; // identity (w,x,y,z)
};

struct BoneTrack {
    int boneIndex = -1; // indice en `Skeleton::bones`; -1 = track huerfano
    std::vector<VectorKey> positionKeys;
    std::vector<QuatKey>   rotationKeys;
    std::vector<VectorKey> scaleKeys;

    bool empty() const {
        return positionKeys.empty() && rotationKeys.empty() && scaleKeys.empty();
    }
};

namespace detail {

/// @brief Devuelve [i0, i1, alpha] tal que `keys[i0].time <= t <= keys[i1].time`
///        y alpha es el factor de mezcla [0, 1] entre ambos.
template <typename KeyT>
inline void findKeyInterval(const std::vector<KeyT>& keys, float t,
                             usize& i0, usize& i1, float& alpha) {
    if (keys.empty()) {
        i0 = i1 = 0;
        alpha = 0.0f;
        return;
    }
    if (keys.size() == 1 || t <= keys.front().time) {
        i0 = i1 = 0;
        alpha = 0.0f;
        return;
    }
    if (t >= keys.back().time) {
        i0 = i1 = keys.size() - 1;
        alpha = 0.0f;
        return;
    }
    // Busqueda lineal: clips tipicos tienen <100 keys por track. Cuando
    // crezca a miles de keys (timeline largo) migrar a binary search.
    for (usize i = 1; i < keys.size(); ++i) {
        if (t < keys[i].time) {
            i0 = i - 1;
            i1 = i;
            const float dt = keys[i1].time - keys[i0].time;
            alpha = (dt > 0.0f) ? (t - keys[i0].time) / dt : 0.0f;
            return;
        }
    }
    // Defensivo: nunca deberia llegar aca por las guards de arriba.
    i0 = i1 = keys.size() - 1;
    alpha = 0.0f;
}

} // namespace detail

/// @brief Muestrea position en `t` (segundos). Identidad (vec3(0)) si no hay keys.
inline glm::vec3 samplePosition(const BoneTrack& tr, float t) {
    if (tr.positionKeys.empty()) return glm::vec3(0.0f);
    usize i0, i1; float a;
    detail::findKeyInterval(tr.positionKeys, t, i0, i1, a);
    return glm::mix(tr.positionKeys[i0].value, tr.positionKeys[i1].value, a);
}

/// @brief Muestrea rotation en `t`. Identidad si no hay keys. LERP normalizado.
inline glm::quat sampleRotation(const BoneTrack& tr, float t) {
    if (tr.rotationKeys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    usize i0, i1; float a;
    detail::findKeyInterval(tr.rotationKeys, t, i0, i1, a);
    const glm::quat& q0 = tr.rotationKeys[i0].value;
    const glm::quat& q1 = tr.rotationKeys[i1].value;
    // LERP por componentes con sign-flip si dot < 0 (evita el "long way around").
    const float d = glm::dot(q0, q1);
    const glm::quat q1c = (d < 0.0f) ? glm::quat(-q1.w, -q1.x, -q1.y, -q1.z) : q1;
    glm::quat r = glm::quat(
        glm::mix(q0.w, q1c.w, a),
        glm::mix(q0.x, q1c.x, a),
        glm::mix(q0.y, q1c.y, a),
        glm::mix(q0.z, q1c.z, a)
    );
    return glm::normalize(r);
}

/// @brief Muestrea scale en `t`. Identidad (vec3(1)) si no hay keys.
inline glm::vec3 sampleScale(const BoneTrack& tr, float t) {
    if (tr.scaleKeys.empty()) return glm::vec3(1.0f);
    usize i0, i1; float a;
    detail::findKeyInterval(tr.scaleKeys, t, i0, i1, a);
    return glm::mix(tr.scaleKeys[i0].value, tr.scaleKeys[i1].value, a);
}

/// @brief Compone la matriz local (TRS) del track en `t`.
inline glm::mat4 sampleLocalTRS(const BoneTrack& tr, float t) {
    const glm::vec3 p = samplePosition(tr, t);
    const glm::quat r = sampleRotation(tr, t);
    const glm::vec3 s = sampleScale(tr, t);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), p);
    m *= glm::mat4_cast(r);
    m = glm::scale(m, s);
    return m;
}

struct AnimationClip {
    std::string name;
    float duration = 0.0f;        // segundos; 0 = clip vacio o no inicializado
    std::vector<BoneTrack> tracks;

    /// @brief Evalua el clip al tiempo `t` y rellena `localPoseOut` con
    ///        matrices LOCALES (TRS) por hueso. Huesos sin track activo
    ///        (canal no presente en el clip) caen al `localBindTransform`
    ///        del esqueleto — eso replica la pose de bind para esos huesos
    ///        en lugar de mandarlos a identity (que produciria un mesh
    ///        explotando alrededor del root).
    void evaluate(float t, const Skeleton& skeleton,
                  std::vector<glm::mat4>& localPoseOut) const {
        const usize n = skeleton.bones.size();
        localPoseOut.resize(n);
        for (usize i = 0; i < n; ++i) {
            localPoseOut[i] = skeleton.bones[i].localBindTransform;
        }
        for (const BoneTrack& tr : tracks) {
            if (tr.boneIndex < 0) continue;
            if (static_cast<usize>(tr.boneIndex) >= n) continue;
            if (tr.empty()) continue;
            localPoseOut[static_cast<usize>(tr.boneIndex)] = sampleLocalTRS(tr, t);
        }
    }
};

} // namespace Mood
