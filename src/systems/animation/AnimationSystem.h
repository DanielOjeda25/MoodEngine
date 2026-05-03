#pragma once

// AnimationSystem (Hito 19): avanza el tiempo de cada `AnimatorComponent`
// y rellena el `SkeletonComponent::skinningMatrices` que el render lee.
//
// Sin estado persistente entre frames — el time vive en el componente,
// asi que pause/replay/rewind se manejan editando el componente.

#include "core/Types.h"

namespace Mood {

class Scene;
class AssetManager;

class AnimationSystem {
public:
    /// @brief Para cada entidad con (MeshRenderer + Animator + Skeleton):
    ///        - Resuelve el MeshAsset y su skeleton (si no tiene, skip).
    ///        - Resuelve el clip activo por `clipName` (vacio = primer clip).
    ///        - Avanza `time` por `dt * speed`. Loop o clamp segun flag.
    ///        - Evalua el clip a una pose local + compone matrices skinning.
    void update(Scene& scene, AssetManager& assets, f32 dt);
};

} // namespace Mood
