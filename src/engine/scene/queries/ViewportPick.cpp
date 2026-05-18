#include "engine/scene/queries/ViewportPick.h"

#include "core/math/Ray.h"
#include "engine/world/grid/GridMap.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <cmath>

namespace Mood {

TilePickResult pickTile(const GridMap& map, const glm::vec3& origin,
                        const glm::mat4& view, const glm::mat4& projection,
                        const glm::vec2& ndc) {
    const glm::mat4 invVP = glm::inverse(projection * view);

    const auto nf = unprojectNearFar(invVP, ndc.x, ndc.y);
    if (!nf) {
        return {};
    }
    const glm::vec3 rayOrigin = nf->nearW;
    const glm::vec3 rayDir    = nf->farW - nf->nearW; // no hace falta normalizar
    if (std::abs(rayDir.y) < 1e-6f) {
        return {}; // rayo paralelo al piso
    }

    // Intersectar con plano y = 0 (piso del mapa; los cubos se extienden de
    // y=0 a y=tileSize). Clickear sobre un muro desde arriba da el mismo tile
    // que clickear en el hueco entre muros: ambos mapean a la misma (x, y).
    const float t = -rayOrigin.y / rayDir.y;
    if (t < 0.0f) {
        return {}; // el plano esta detras de la camara
    }
    const glm::vec3 hit = rayOrigin + t * rayDir;

    const float lx = hit.x - origin.x;
    const float lz = hit.z - origin.z;
    if (lx < 0.0f || lz < 0.0f) {
        return {};
    }
    const u32 tx = static_cast<u32>(lx / map.tileSize());
    const u32 ty = static_cast<u32>(lz / map.tileSize());
    if (tx >= map.width() || ty >= map.height()) {
        return {};
    }

    return TilePickResult{true, tx, ty, hit};
}

} // namespace Mood
