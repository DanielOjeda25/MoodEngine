// F2H49: loader de `AnimationClip` standalone — usado para FBX "anim-only"
// como los que Mixamo entrega cuando bajas una animacion con el modo
// "Without Skin". El archivo trae solo tracks de animacion + jerarquia de
// nodos del esqueleto fuente, pero NO mesh ni `aiBone`.
//
// Diferencias vs `loadMeshWithAssimp`:
//   - No requiere mesh (`mNumMeshes == 0` es OK).
//   - No construye `Skeleton` (no hay aiBone). Las tracks quedan con
//     `boneIndex = -1`; el bind contra un esqueleto destino lo hace el
//     `AnimationSystem` la primera vez que el clip se reproduce.
//   - Devuelve `unique_ptr<AnimationClip>` directo (no `MeshAsset`).
//
// Reusa los mismos flags FBX que el loader principal (Mixamo cm->m,
// pivots colapsados) para que coordenadas y nombres de bones sean
// consistentes entre los dos paths.

#include "engine/assets/loaders/MeshLoader.h"
#include "engine/assets/loaders/MeshLoader_Internal.h"

#include "core/Log.h"
#include "engine/animation/clips/AnimationClip.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Mood {

std::unique_ptr<AnimationClip> loadAnimationClipWithAssimp(
    const std::string& logicalPath,
    const std::string& filesystemPath) {

    Assimp::Importer importer;

    // Mismos config flags que `loadMeshWithAssimp` — los clips Mixamo
    // vienen en cm con pivots, normalizamos a m y colapsamos.
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, false);
    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);

    // Solo necesitamos animaciones — no procesamos geometria.
    const u32 flags = aiProcess_GlobalScale;
    const aiScene* scene = importer.ReadFile(filesystemPath, flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0
        || scene->mRootNode == nullptr) {
        Log::assets()->warn("AnimationClipLoader: fallo '{}' ({})", logicalPath,
                             importer.GetErrorString());
        return nullptr;
    }
    if (scene->mNumAnimations == 0) {
        Log::assets()->warn(
            "AnimationClipLoader: '{}' no contiene animaciones",
            logicalPath);
        return nullptr;
    }

    // Solo nos quedamos con la PRIMERA animacion. Convencion Mixamo: cada
    // archivo "Without Skin" trae exactamente 1 clip. Si en el futuro
    // aparecen FBXs con multiples, el caller tendra que pedir por indice.
    const aiAnimation* anim = scene->mAnimations[0];
    if (anim == nullptr || anim->mNumChannels == 0) {
        Log::assets()->warn(
            "AnimationClipLoader: '{}' tiene animacion vacia (0 channels)",
            logicalPath);
        return nullptr;
    }

    // tps fallback igual a parseAnimations.
    const float tps = (anim->mTicksPerSecond > 0.0)
                       ? static_cast<float>(anim->mTicksPerSecond)
                       : 25.0f;

    auto clip = std::make_unique<AnimationClip>();
    clip->name = detail::sanitizeClipName(anim->mName.C_Str());
    if (clip->name.empty()) {
        // Fallback: usar el nombre del archivo sin extension. Para Mixamo
        // suele venir como "mixamo.com" -> sanitized "mixamo_com". Usar
        // el filename ("anim_idle") es mas util que un id generico.
        const auto slash = logicalPath.find_last_of('/');
        std::string base = (slash == std::string::npos)
                            ? logicalPath
                            : logicalPath.substr(slash + 1);
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        clip->name = base.empty() ? std::string{"anim_0"} : base;
    }
    clip->duration = static_cast<float>(anim->mDuration) / tps;
    clip->tracks.reserve(anim->mNumChannels);

    // F2H49: NO filtramos canales por "es bone" — el esqueleto destino se
    // resuelve recien en el bind pass. Aceptamos todo canal, salvo los
    // nodos auxiliares que assimp inserta y suelen colapsarse al apagar
    // PRESERVE_PIVOTS (`$AssimpFbx$_*`). Por defensiva los filtramos por
    // si quedo algun residuo: no tienen contraparte en un esqueleto real.
    for (u32 ci = 0; ci < anim->mNumChannels; ++ci) {
        const aiNodeAnim* ch = anim->mChannels[ci];
        if (ch == nullptr) continue;
        const std::string nodeName = ch->mNodeName.C_Str();
        if (nodeName.find("$AssimpFbx$") != std::string::npos) continue;

        BoneTrack tr = detail::convertChannel(*ch, tps);
        // boneIndex queda en -1 (default). El AnimationSystem hara el
        // bind pass cuando el clip se asigne a un AnimatorComponent con
        // un esqueleto disponible.
        clip->tracks.push_back(std::move(tr));
    }

    if (clip->tracks.empty()) {
        Log::assets()->warn(
            "AnimationClipLoader: '{}' quedo sin tracks tras filtrar pivots",
            logicalPath);
        return nullptr;
    }

    Log::assets()->info(
        "AnimationClipLoader: '{}' cargado (clip='{}', duration={:.2f}s, "
        "{} tracks)",
        logicalPath, clip->name, clip->duration, clip->tracks.size());

    return clip;
}

} // namespace Mood
