#pragma once

// Skeleton (Hito 19): jerarquia de huesos del bind pose de un mesh.
//
// Convenciones:
// - `bones[i].parent` es indice al mismo array (-1 si es root). El orden
//   topologico esta garantizado por `MeshLoader`: un padre siempre tiene
//   indice menor que sus hijos. Eso permite componer matrices globales en
//   un solo pass O(N) sin recursion ni stack.
// - `inverseBind` es la matriz que lleva un vertice de mesh-space al
//   espacio local del hueso EN BIND POSE. Se aplica una sola vez en el
//   shader (por hueso) y el resultado es que el vertice ya esta listo para
//   ser empujado por la pose actual.
// - `localBindTransform` se guarda solo para diagnostico / fallback al
//   bind pose cuando no hay clip activo. El shader nunca lo ve.
//
// Header-only: estructuras puras (sin GL ni assimp). Los tests pueden
// linkearlo sin arrastrar el motor entero.

#include "core/Types.h"

#include <glm/mat4x4.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Mood {

/// Cap de huesos por esqueleto. Matchea `MAX_BONES` en `pbr_skinned.vert`.
/// Cambiarlo aqui obliga a cambiar el shader (no hay generacion automatica).
constexpr u32 k_maxBonesPerSkeleton = 128;

struct Bone {
    std::string name;
    int parent = -1;                    // -1 = root; sino indice al mismo array
    glm::mat4 inverseBind{1.0f};        // mesh-space -> bone-space (bind pose)
    glm::mat4 localBindTransform{1.0f}; // TRS local relativo al padre, en bind
};

struct Skeleton {
    std::vector<Bone> bones;

    /// @brief Busca un hueso por nombre. -1 si no existe.
    int boneIndex(std::string_view name) const {
        for (usize i = 0; i < bones.size(); ++i) {
            if (bones[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    /// @brief true si el array esta vacio (mesh sin esqueleto).
    bool empty() const { return bones.empty(); }
};

/// @brief Compone matrices de skinning a partir de una pose LOCAL (TRS por
///        hueso, en espacio del padre). El resultado por hueso es:
///
///            global(i) = global(parent(i)) * local(i)
///            skinning(i) = global(i) * inverseBind(i)
///
///        Esa `skinning(i)` es exactamente lo que el shader sube como
///        `uBoneMatrices[i]`: aplicada a un vertice del bind pose en
///        mesh-space, devuelve la posicion en mesh-space para la pose
///        actual.
///
///        `localPose` debe tener size == skel.bones.size(). Si esta
///        vacio, se usan los `localBindTransform` (resulta en matrices
///        identidad de skinning, mesh en bind pose).
inline void computeSkinningMatrices(const Skeleton& skel,
                                     const std::vector<glm::mat4>& localPose,
                                     std::vector<glm::mat4>& outSkinning) {
    const usize n = skel.bones.size();
    outSkinning.assign(n, glm::mat4(1.0f));
    if (n == 0) return;

    // Buffer auxiliar: globales (en mesh-space, sin inverseBind aun).
    std::vector<glm::mat4> global(n, glm::mat4(1.0f));

    for (usize i = 0; i < n; ++i) {
        const Bone& b = skel.bones[i];
        const glm::mat4& local = (localPose.size() == n) ? localPose[i]
                                                          : b.localBindTransform;
        if (b.parent < 0) {
            global[i] = local;
        } else {
            // Por la garantia de orden topologico (padre antes que hijo),
            // global[parent] ya esta escrito.
            global[i] = global[static_cast<usize>(b.parent)] * local;
        }
        outSkinning[i] = global[i] * b.inverseBind;
    }
}

} // namespace Mood
