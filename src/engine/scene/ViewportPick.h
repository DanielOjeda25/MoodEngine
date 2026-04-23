#pragma once

// Tile picking: convertir una coordenada NDC del cursor sobre el panel
// Viewport en un tile del GridMap bajo el cursor. Usa un raycast cam-to-ndc
// intersectado con el plano y=0 del mundo (piso del mapa).

#include "core/Types.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class GridMap;

struct TilePickResult {
    bool hit = false;    ///< Si el rayo intersecto dentro del area del mapa.
    u32 tileX = 0;
    u32 tileY = 0;
    glm::vec3 worldPoint{0.0f}; ///< Punto exacto del hit en coords de mundo.
};

/// @brief Dispara un rayo desde la camara (definida por `view`, `projection`)
///        a traves del punto `ndc` (rango [-1, 1], Y+ arriba segun convencion
///        OpenGL). Intersecta con el plano y=0 del mundo y, si el punto cae
///        dentro de [mapWorldOrigin, mapWorldOrigin + width*tileSize x
///        height*tileSize], devuelve las coords del tile.
TilePickResult pickTile(const GridMap& map, const glm::vec3& mapWorldOrigin,
                         const glm::mat4& view, const glm::mat4& projection,
                         const glm::vec2& ndc);

} // namespace Mood
