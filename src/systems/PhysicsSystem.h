#pragma once

// Colisiones AABB-vs-grid del Hito 4. Sistema sin estado: una funcion libre
// que el caller invoca por frame con el delta deseado. Las paredes del mapa
// se consideran infinitas en Y (no colisionamos el eje vertical aun).
//
// Jolt entra en el Hito 12 y reemplaza esta implementacion.

#include "core/Types.h"
#include "core/math/AABB.h"
#include "engine/world/GridMap.h"

#include <glm/vec3.hpp>

namespace Mood {

/// @brief Mueve `box` en el mundo intentando aplicar `desiredDelta`.
///        Resuelve eje por eje (X, luego Z) para permitir deslizar contra
///        paredes en las esquinas. Y se aplica directo (sin colision).
///        Los tiles fuera del mapa se tratan como pared solida.
///
/// @param map             Mapa cuyos tiles isSolid determinan los bloqueos.
/// @param mapWorldOrigin  Desplazamiento del tile (0,0) del mapa en el mundo.
///                        El mismo offset que usa el renderer.
/// @param box             AABB del jugador en world coords. Se modifica
///                        aplicandole el delta final.
/// @param desiredDelta    Delta que se quiere mover este frame, en world.
/// @return                El delta realmente aplicado (puede ser menor que
///                        desiredDelta en alguno de los ejes si hubo colision).
glm::vec3 moveAndSlide(const GridMap& map,
                       const glm::vec3& mapWorldOrigin,
                       AABB& box,
                       const glm::vec3& desiredDelta);

} // namespace Mood
