#include "engine/scene/ViewportPick.h"

#include "engine/world/GridMap.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <cmath>

namespace Mood {

TilePickResult pickTile(const GridMap& map, const glm::vec3& origin,
                        const glm::mat4& view, const glm::mat4& projection,
                        const glm::vec2& ndc) {
    // Unproyectar el punto NDC en el frustum: dos puntos en z=-1 (near) y
    // z=+1 (far) permiten definir el rayo camara->mundo sin depender de la
    // convencion de handed-ness del sistema (el signo de rayDir.y importa
    // para la interseccion con el plano).
    const glm::mat4 invVP = glm::inverse(projection * view);

    const glm::vec4 nearH = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    const glm::vec4 farH  = invVP * glm::vec4(ndc.x, ndc.y, +1.0f, 1.0f);
    if (nearH.w == 0.0f || farH.w == 0.0f) {
        return {};
    }
    const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
    const glm::vec3 farW  = glm::vec3(farH)  / farH.w;

    const glm::vec3 rayOrigin = nearW;
    const glm::vec3 rayDir = farW - nearW; // no hace falta normalizar
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
