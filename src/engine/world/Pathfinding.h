#pragma once

// A* pathfinding sobre `GridMap` para entidades con NavAgent (Hito 23
// Bloque 1). Funcion pura: recibe el mapa + start + goal y devuelve la
// secuencia de tiles (en grid coords) desde start hasta goal inclusive.
//
// V1 — 4-connected (sin diagonales) + heuristica Manhattan. Sin
// corner-cutting, sin diagonales-cortadas-por-pared, sin path
// smoothing. El NavSystem hace el steering en world space sobre los
// waypoints crudos.

#include "core/Types.h"

#include <vector>

namespace Mood {

class GridMap;

namespace Pathfinding {

struct TileCoord {
    u32 x = 0;
    u32 y = 0;

    bool operator==(const TileCoord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const TileCoord& o) const { return !(*this == o); }
};

/// @brief A* en `map` desde `start` a `goal`. Retorna:
///   - `[start]` si `start == goal`.
///   - `[]` si no hay camino, si start o goal estan fuera del mapa,
///     o si el goal es un tile solido.
///   - `[start, ..., goal]` con cada tile contiguo (4-vecindad).
///
/// El start solido SI es valido (un agente que arranco "atravesando"
/// una pared puede salir caminando) — pero la mayoria de los callers
/// no van a tener este caso porque las entidades arrancan en tiles
/// libres. Goal solido es invalido (no hay como pisarlo).
std::vector<TileCoord> findPath(const GridMap& map,
                                 TileCoord start,
                                 TileCoord goal);

} // namespace Pathfinding

} // namespace Mood
