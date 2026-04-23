// Tests del sistema de colision moveAndSlide. Cubren los casos que el
// Play Mode dispara al caminar: una sola pared, esquinas con slide, y
// seguridad contra AABB degenerada + movimiento nulo.

#include <doctest/doctest.h>

#include "core/math/AABB.h"
#include "engine/world/GridMap.h"
#include "systems/PhysicsSystem.h"

#include <glm/vec3.hpp>

using namespace Mood;

namespace {

/// @brief Arma un mapa vacio con bordes solidos. Util como base de los tests.
GridMap makeBorderedMap(u32 w, u32 h, f32 tileSize = 1.0f) {
    GridMap m{w, h, tileSize};
    for (u32 i = 0; i < w; ++i) {
        m.setTile(i, 0u,     TileType::SolidWall);
        m.setTile(i, h - 1u, TileType::SolidWall);
    }
    for (u32 j = 0; j < h; ++j) {
        m.setTile(0u,     j, TileType::SolidWall);
        m.setTile(w - 1u, j, TileType::SolidWall);
    }
    return m;
}

/// @brief AABB de jugador centrada en pos con half-extent de 0.2 en XZ.
AABB makePlayerAabb(glm::vec3 pos) {
    const glm::vec3 half(0.2f, 0.45f, 0.2f);
    return AABB{pos - half, pos + half};
}

} // namespace

TEST_CASE("moveAndSlide: delta cero no cambia nada") {
    const GridMap m = makeBorderedMap(5u, 5u);
    AABB box = makePlayerAabb({2.5f, 0.5f, 2.5f});
    const AABB before = box;
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, glm::vec3(0.0f));
    CHECK(actual.x == doctest::Approx(0.0f));
    CHECK(actual.y == doctest::Approx(0.0f));
    CHECK(actual.z == doctest::Approx(0.0f));
    CHECK(box.min == before.min);
    CHECK(box.max == before.max);
}

TEST_CASE("moveAndSlide: area libre aplica el delta completo") {
    const GridMap m = makeBorderedMap(8u, 8u);
    AABB box = makePlayerAabb({3.5f, 0.5f, 3.5f});
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, {0.1f, 0.0f, 0.1f});
    CHECK(actual.x == doctest::Approx(0.1f));
    CHECK(actual.z == doctest::Approx(0.1f));
}

TEST_CASE("moveAndSlide: pared en +X bloquea pero no el eje Z") {
    const GridMap m = makeBorderedMap(5u, 5u);
    // Jugador cerca del borde derecho: box.max.x = 3.3, pared en tile 4 (world 4..5).
    AABB box = makePlayerAabb({3.1f, 0.5f, 2.5f});
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, {1.0f, 0.0f, 0.5f});

    // X debe clampear para que box.max.x no pase 4.0 (min de tile(4,?)): permitido = 4.0 - 3.3 = 0.7.
    CHECK(actual.x == doctest::Approx(0.7f).epsilon(1e-3f));
    // Z pasa completo (no hay pared en esa direccion).
    CHECK(actual.z == doctest::Approx(0.5f));
    // La caja quedo pegada al muro.
    CHECK(box.max.x == doctest::Approx(4.0f).epsilon(1e-3f));
}

TEST_CASE("moveAndSlide: pared en -X bloquea al moverse en esa direccion") {
    const GridMap m = makeBorderedMap(5u, 5u);
    AABB box = makePlayerAabb({1.3f, 0.5f, 2.5f}); // box.min.x = 1.1
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, {-1.0f, 0.0f, 0.0f});

    // Permitido = 1.0 - 1.1 = -0.1 (el muro de tile(0,?) termina en x=1.0).
    CHECK(actual.x == doctest::Approx(-0.1f).epsilon(1e-3f));
    CHECK(box.min.x == doctest::Approx(1.0f).epsilon(1e-3f));
}

TEST_CASE("moveAndSlide: esquina - X bloquea pero Z desliza contra otra pared") {
    // Mapa 5x5 con paredes en los bordes + una "esquina interna" que forma una L.
    GridMap m = makeBorderedMap(5u, 5u);

    AABB box = makePlayerAabb({1.3f, 0.5f, 1.3f});
    // Intentamos movernos diagonal hacia la esquina NE-interior (y negativo).
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, {-0.5f, 0.0f, -0.5f});

    // X clampea contra pared izquierda (tile (0,?), termina en x=1.0).
    CHECK(actual.x == doctest::Approx(-0.1f).epsilon(1e-3f));
    // Z clampea contra pared superior (tile (?,0), termina en z=1.0).
    CHECK(actual.z == doctest::Approx(-0.1f).epsilon(1e-3f));
}

TEST_CASE("moveAndSlide: Y pasa directo (muros infinitos en Y)") {
    const GridMap m = makeBorderedMap(4u, 4u);
    AABB box = makePlayerAabb({1.5f, 0.5f, 1.5f});
    // Pre-move: min.y = 0.05, max.y = 0.95.
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), box, {0.0f, 5.0f, 0.0f});
    CHECK(actual.y == doctest::Approx(5.0f));
    CHECK(box.min.y == doctest::Approx(5.05f).epsilon(1e-3f));
    CHECK(box.max.y == doctest::Approx(5.95f).epsilon(1e-3f));
}

TEST_CASE("moveAndSlide: respeta mapWorldOrigin") {
    const GridMap m = makeBorderedMap(4u, 4u);
    const glm::vec3 origin(-2.0f, 0.0f, -2.0f);
    // Con origin (-2,0,-2), el tile (0,0) ocupa world X [-2,-1] Z [-2,-1] (pared).
    // Interior tile (1,1) ocupa world X [-1,0], Z [-1,0].
    AABB box = makePlayerAabb({-0.5f, 0.5f, -0.5f});
    // Mover -X hasta pegarse: pared termina en world X=-1.0; box.min.x=-0.7; permitido=-0.3.
    const glm::vec3 actual = moveAndSlide(m, origin, box, {-1.0f, 0.0f, 0.0f});
    CHECK(actual.x == doctest::Approx(-0.3f).epsilon(1e-3f));
    CHECK(box.min.x == doctest::Approx(-1.0f).epsilon(1e-3f));
}

TEST_CASE("moveAndSlide: AABB degenerada (punto) no crashea y bloquea") {
    const GridMap m = makeBorderedMap(3u, 3u);
    AABB point{glm::vec3(1.5f, 0.5f, 0.99f), glm::vec3(1.5f, 0.5f, 0.99f)};
    // Con Z muy cerca de la pared norte (tile (?,0), termina en z=1.0): permitido muy chico.
    const glm::vec3 actual = moveAndSlide(m, glm::vec3(0.0f), point, {0.0f, 0.0f, -0.5f});
    CHECK(actual.z >= -0.01f);
    CHECK(actual.z <= 0.0f);
}
