// Tests del A* sobre GridMap (Hito 23 Bloque 1). Headless: solo
// GridMap + Pathfinding, sin GL ni AssetManager.

#include <doctest/doctest.h>

#include "engine/world/GridMap.h"
#include "engine/world/Pathfinding.h"

using namespace Mood;
using Pathfinding::TileCoord;

namespace {

// Verifica que el path conecte start->goal con saltos de 1 tile cada
// vez (sin teleports) y que ningun tile sea solido.
bool isValidPath(const GridMap& map,
                 TileCoord start, TileCoord goal,
                 const std::vector<TileCoord>& path) {
    if (path.empty()) return false;
    if (path.front() != start) return false;
    if (path.back() != goal)   return false;
    for (size_t i = 0; i < path.size(); ++i) {
        if (map.isSolid(path[i].x, path[i].y)) return false;
        if (i + 1 < path.size()) {
            const auto a = path[i];
            const auto b = path[i + 1];
            const u32 dx = (a.x > b.x) ? (a.x - b.x) : (b.x - a.x);
            const u32 dy = (a.y > b.y) ? (a.y - b.y) : (b.y - a.y);
            if (dx + dy != 1) return false; // 4-vecindad
        }
    }
    return true;
}

} // namespace

TEST_CASE("Pathfinding: sala vacia 8x8 produce path directo") {
    GridMap map(8u, 8u);
    const auto path = Pathfinding::findPath(map, {0, 0}, {7, 7});

    CHECK(isValidPath(map, {0, 0}, {7, 7}, path));
    // En sala vacia con Manhattan 4-conn, el path optimo = dx+dy+1 = 15.
    CHECK(path.size() == 15);
}

TEST_CASE("Pathfinding: muro al medio bordea") {
    // Sala 5x5 con una columna vertical solida en x=2 desde y=0 a y=3
    // (deja y=4 libre como pasaje).
    GridMap map(5u, 5u);
    map.setTile(2u, 0u, TileType::SolidWall);
    map.setTile(2u, 1u, TileType::SolidWall);
    map.setTile(2u, 2u, TileType::SolidWall);
    map.setTile(2u, 3u, TileType::SolidWall);

    const auto path = Pathfinding::findPath(map, {0, 0}, {4, 0});

    CHECK(isValidPath(map, {0, 0}, {4, 0}, path));
    // Tiene que haber pasado por y=4 al menos una vez para rodear.
    bool wentAround = false;
    for (const auto& t : path) if (t.y == 4) wentAround = true;
    CHECK(wentAround);
}

TEST_CASE("Pathfinding: sin path posible devuelve vacio") {
    GridMap map(5u, 5u);
    // Encerramos goal con paredes en sus 4 vecinos.
    map.setTile(3u, 4u, TileType::SolidWall);
    map.setTile(4u, 3u, TileType::SolidWall);
    // (4,5) y (5,4) ya son fuera del mapa = solidos por convencion.

    const auto path = Pathfinding::findPath(map, {0, 0}, {4, 4});
    CHECK(path.empty());
}

TEST_CASE("Pathfinding: start == goal devuelve [start]") {
    GridMap map(8u, 8u);
    const auto path = Pathfinding::findPath(map, {3, 3}, {3, 3});
    REQUIRE(path.size() == 1);
    CHECK(path[0] == TileCoord{3, 3});
}

TEST_CASE("Pathfinding: start o goal fuera del grid devuelve vacio") {
    GridMap map(5u, 5u);
    CHECK(Pathfinding::findPath(map, {10, 0}, {0, 0}).empty());
    CHECK(Pathfinding::findPath(map, {0, 0}, {0, 99}).empty());
}

TEST_CASE("Pathfinding: goal solido devuelve vacio") {
    GridMap map(5u, 5u);
    map.setTile(3u, 3u, TileType::SolidWall);
    const auto path = Pathfinding::findPath(map, {0, 0}, {3, 3});
    CHECK(path.empty());
}
