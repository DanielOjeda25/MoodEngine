#include "systems/PhysicsSystem.h"

#include <algorithm>
#include <cmath>

namespace Mood {

namespace {

constexpr f32 k_boundaryEps = 1e-5f;

/// @brief Indice de tile que cubre una coordenada local (con offset ya
///        descontado). Se resta un epsilon al borde superior para evitar
///        tomar el tile siguiente cuando el AABB termina exactamente en el
///        limite entre dos tiles.
i32 tileIndexMin(f32 coord, f32 tileSize) {
    return static_cast<i32>(std::floor(coord / tileSize));
}
i32 tileIndexMax(f32 coord, f32 tileSize) {
    return static_cast<i32>(std::floor((coord - k_boundaryEps) / tileSize));
}

/// @brief True si el tile (x,z) bloquea movimiento: solido o fuera del mapa.
bool tileBlocks(const GridMap& map, i32 x, i32 z) {
    if (x < 0 || z < 0) return true;
    if (x >= static_cast<i32>(map.width()) || z >= static_cast<i32>(map.height())) return true;
    return map.isSolid(static_cast<u32>(x), static_cast<u32>(z));
}

/// @brief AABB en world coords del tile (x,z), soportando indices negativos
///        (tiles virtuales fuera del mapa para clampeo).
AABB tileWorldAabb(const GridMap& map, i32 x, i32 z, const glm::vec3& origin) {
    const f32 ts = map.tileSize();
    const f32 fx = static_cast<f32>(x) * ts;
    const f32 fz = static_cast<f32>(z) * ts;
    return AABB{
        origin + glm::vec3(fx, 0.0f, fz),
        origin + glm::vec3(fx + ts, ts, fz + ts),
    };
}

/// @brief Resuelve el delta en UN eje horizontal (0 = X, 2 = Z). Asume que
///        `box` ya esta resuelto en los ejes previos. Devuelve el delta
///        ajustado (clampeado si toca una pared).
f32 resolveHorizontalAxis(const GridMap& map, const AABB& box, f32 delta,
                          int axis, const glm::vec3& origin) {
    if (std::abs(delta) < k_boundaryEps) return 0.0f;

    AABB proposed = box;
    if (axis == 0) {
        proposed.min.x += delta;
        proposed.max.x += delta;
    } else {
        proposed.min.z += delta;
        proposed.max.z += delta;
    }

    const f32 ts = map.tileSize();
    const i32 minX = tileIndexMin(proposed.min.x - origin.x, ts);
    const i32 maxX = tileIndexMax(proposed.max.x - origin.x, ts);
    const i32 minZ = tileIndexMin(proposed.min.z - origin.z, ts);
    const i32 maxZ = tileIndexMax(proposed.max.z - origin.z, ts);

    for (i32 z = minZ; z <= maxZ; ++z) {
        for (i32 x = minX; x <= maxX; ++x) {
            if (!tileBlocks(map, x, z)) continue;

            const AABB t = tileWorldAabb(map, x, z, origin);
            if (axis == 0) {
                if (delta > 0.0f) {
                    // Moviendo +X: box.max.x no puede pasar t.min.x.
                    const f32 allowed = t.min.x - box.max.x;
                    if (allowed < delta) delta = std::max(0.0f, allowed);
                } else {
                    // Moviendo -X: box.min.x no puede quedar antes de t.max.x.
                    const f32 allowed = t.max.x - box.min.x;
                    if (allowed > delta) delta = std::min(0.0f, allowed);
                }
            } else { // axis == 2
                if (delta > 0.0f) {
                    const f32 allowed = t.min.z - box.max.z;
                    if (allowed < delta) delta = std::max(0.0f, allowed);
                } else {
                    const f32 allowed = t.max.z - box.min.z;
                    if (allowed > delta) delta = std::min(0.0f, allowed);
                }
            }
        }
    }
    return delta;
}

} // namespace

glm::vec3 moveAndSlide(const GridMap& map,
                       const glm::vec3& mapWorldOrigin,
                       AABB& box,
                       const glm::vec3& desiredDelta) {
    glm::vec3 actual{0.0f};

    // Eje X primero. Aplicamos su delta a la caja antes de evaluar Z, para
    // que el slide contra una esquina funcione: X se pega a la pared, y luego
    // Z puede deslizar porque el rango de tiles ya refleja la nueva X.
    actual.x = resolveHorizontalAxis(map, box, desiredDelta.x, 0, mapWorldOrigin);
    box.min.x += actual.x;
    box.max.x += actual.x;

    actual.z = resolveHorizontalAxis(map, box, desiredDelta.z, 2, mapWorldOrigin);
    box.min.z += actual.z;
    box.max.z += actual.z;

    // Y no se colisiona contra el grid en este hito (muros infinitos en Y).
    actual.y = desiredDelta.y;
    box.min.y += actual.y;
    box.max.y += actual.y;

    return actual;
}

} // namespace Mood
