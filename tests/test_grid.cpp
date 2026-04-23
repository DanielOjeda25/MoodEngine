// Tests de GridMap: construccion, setTile/tileAt, isSolid dentro y fuera
// del mapa, aabbOfTile. Las coords fuera del mapa se tratan como pared
// solida (invariante del que depende PhysicsSystem).

#include <doctest/doctest.h>

#include "engine/world/GridMap.h"

using namespace Mood;

TEST_CASE("GridMap default se construye vacio") {
    GridMap m{4u, 3u, 2.0f};
    CHECK(m.width() == 4u);
    CHECK(m.height() == 3u);
    CHECK(m.tileSize() == doctest::Approx(2.0f));
    CHECK(m.solidCount() == 0u);
    for (u32 y = 0; y < m.height(); ++y) {
        for (u32 x = 0; x < m.width(); ++x) {
            CHECK(m.tileAt(x, y) == TileType::Empty);
            CHECK_FALSE(m.isSolid(x, y));
        }
    }
}

TEST_CASE("setTile modifica celdas validas y solidCount refleja el total") {
    GridMap m{3u, 3u};
    m.setTile(1u, 1u, TileType::SolidWall);
    m.setTile(0u, 2u, TileType::SolidWall);
    m.setTile(2u, 0u, TileType::SolidWall);
    CHECK(m.isSolid(1u, 1u));
    CHECK(m.isSolid(0u, 2u));
    CHECK(m.isSolid(2u, 0u));
    CHECK_FALSE(m.isSolid(0u, 0u));
    CHECK(m.solidCount() == 3u);

    // Sobrescribir vuelve el tile a Empty.
    m.setTile(1u, 1u, TileType::Empty);
    CHECK_FALSE(m.isSolid(1u, 1u));
    CHECK(m.solidCount() == 2u);
}

TEST_CASE("setTile fuera del mapa es silencioso (no crash, no efecto)") {
    GridMap m{2u, 2u};
    m.setTile(5u, 0u, TileType::SolidWall);
    m.setTile(0u, 99u, TileType::SolidWall);
    CHECK(m.solidCount() == 0u);
}

TEST_CASE("isSolid fuera del mapa devuelve true (bordes = pared)") {
    GridMap m{4u, 4u};
    // Invariante clave: PhysicsSystem asume que out-of-bounds bloquea.
    CHECK(m.isSolid(4u, 0u));
    CHECK(m.isSolid(0u, 4u));
    CHECK(m.isSolid(10u, 10u));
    // Tambien el caso u32 con wrap-around (cast desde i32 negativo).
    CHECK(m.isSolid(static_cast<u32>(-1), 0u));
    CHECK(m.isSolid(0u, static_cast<u32>(-5)));
}

TEST_CASE("aabbOfTile en map-local coords respeta tileSize") {
    GridMap m{4u, 4u, 1.5f};
    const AABB a = m.aabbOfTile(0u, 0u);
    CHECK(a.min.x == doctest::Approx(0.0f));
    CHECK(a.min.y == doctest::Approx(0.0f));
    CHECK(a.min.z == doctest::Approx(0.0f));
    CHECK(a.max.x == doctest::Approx(1.5f));
    CHECK(a.max.y == doctest::Approx(1.5f));
    CHECK(a.max.z == doctest::Approx(1.5f));

    const AABB b = m.aabbOfTile(2u, 3u);
    CHECK(b.min.x == doctest::Approx(3.0f));
    CHECK(b.min.z == doctest::Approx(4.5f));
    CHECK(b.max.x == doctest::Approx(4.5f));
    CHECK(b.max.z == doctest::Approx(6.0f));
    // Y siempre en [0, tileSize].
    CHECK(b.min.y == doctest::Approx(0.0f));
    CHECK(b.max.y == doctest::Approx(1.5f));
}
