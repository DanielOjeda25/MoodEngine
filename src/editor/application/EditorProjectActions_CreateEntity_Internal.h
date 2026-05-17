#pragma once

// Header interno compartido entre los .cpp del modulo "Crear Entidad":
//   - `EditorProjectActions_CreateEntity.cpp` — handler de import + light helpers
//   - `EditorProjectActions_CreateEntity_PickModal.cpp` — modal "Elegir mesh"
//
// Split aplicado en AUDIT-2 (2026-05-17) cuando el archivo original paso
// los 813 LOC (hard cap 800). El helper `rotatedAabbWorldY_local` se usa
// en ambos siblings para anclar el bottom del AABB rotado al piso.
// Convencion: privado al subdir, no incluir desde afuera.

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>

namespace Mood::CreateEntityHelpers {

struct WorldYBoundsLocal {
    float minY = 0.0f;
    float maxY = 0.0f;
};

inline WorldYBoundsLocal rotatedAabbWorldY_local(const glm::vec3& aabbMin,
                                                  const glm::vec3& aabbMax,
                                                  const glm::vec3& importEuler) {
    const glm::vec3 corners[8] = {
        {aabbMin.x, aabbMin.y, aabbMin.z}, {aabbMax.x, aabbMin.y, aabbMin.z},
        {aabbMin.x, aabbMax.y, aabbMin.z}, {aabbMax.x, aabbMax.y, aabbMin.z},
        {aabbMin.x, aabbMin.y, aabbMax.z}, {aabbMax.x, aabbMin.y, aabbMax.z},
        {aabbMin.x, aabbMax.y, aabbMax.z}, {aabbMax.x, aabbMax.y, aabbMax.z},
    };
    const glm::mat4 rot =
        glm::rotate(glm::mat4(1.0f), importEuler.y, glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(1.0f), importEuler.x, glm::vec3(1, 0, 0)) *
        glm::rotate(glm::mat4(1.0f), importEuler.z, glm::vec3(0, 0, 1));
    WorldYBoundsLocal wy{1e30f, -1e30f};
    for (const auto& c : corners) {
        const glm::vec3 r = glm::vec3(rot * glm::vec4(c, 1.0f));
        wy.minY = std::min(wy.minY, r.y);
        wy.maxY = std::max(wy.maxY, r.y);
    }
    return wy;
}

} // namespace Mood::CreateEntityHelpers
