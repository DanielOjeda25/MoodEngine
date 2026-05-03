// Tests de las primitivas de frustum culling (F2H3 Bloque B).
// Cubren extraccion de planos via Gribb-Hartmann, test conservador
// AABB-vs-frustum (truco p-vertex) y transformacion de AABB local
// a world (rotacion expande, escala escala, traslacion mueve).

#include <doctest/doctest.h>

#include "engine/render/pipeline/Frustum.h"

#include <glm/ext/matrix_clip_space.hpp>     // glm::perspective
#include <glm/ext/matrix_transform.hpp>      // glm::translate, rotate, scale, lookAt
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <cmath>

using namespace Mood;

namespace {

constexpr f32 k_eps = 1e-4f;

// Camara canonica para los tests: centrada en origen, mirando -Z, FOV 60°,
// aspect 16/9, near 0.1, far 100. La uso por toda la suite para no repetir
// boilerplate.
glm::mat4 canonicalViewProj() {
    const glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    return proj * view;
}

AABB makeAabb(const glm::vec3& center, f32 halfSize) {
    return AABB{center - glm::vec3(halfSize), center + glm::vec3(halfSize)};
}

} // namespace

TEST_CASE("frustumFromViewProj: 6 planos con normales unitarias") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    for (const auto& p : frustum.planes) {
        const f32 len = glm::length(p.normal);
        CHECK(len == doctest::Approx(1.0f).epsilon(0.001));
    }
}

TEST_CASE("aabbVisible: AABB en el centro del frustum es visible") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    // Cubo unitario a 5m delante de la camara (mirando -Z).
    const AABB cube = makeAabb(glm::vec3(0.0f, 0.0f, -5.0f), 0.5f);
    CHECK(aabbVisible(cube, frustum));
}

TEST_CASE("aabbVisible: AABB completamente detras de la camara NO es visible") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    // Camara mira -Z; +Z esta detras. AABB en z=+10 → fuera del near.
    const AABB cube = makeAabb(glm::vec3(0.0f, 0.0f, 10.0f), 0.5f);
    CHECK_FALSE(aabbVisible(cube, frustum));
}

TEST_CASE("aabbVisible: AABB lateral fuera del side plane NO es visible") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    // A 5m de profundidad + 50m al costado: bien fuera del FOV horizontal de 60°.
    const AABB cube = makeAabb(glm::vec3(50.0f, 0.0f, -5.0f), 0.5f);
    CHECK_FALSE(aabbVisible(cube, frustum));
}

TEST_CASE("aabbVisible: AABB mas alla del far plane NO es visible") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    // far = 100, AABB en z=-200.
    const AABB cube = makeAabb(glm::vec3(0.0f, 0.0f, -200.0f), 0.5f);
    CHECK_FALSE(aabbVisible(cube, frustum));
}

TEST_CASE("aabbVisible: AABB intersectando un plano lateral SI es visible (conservador)") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    // Cubo gigante de 10m centrado a 5m de profundidad: parte cae fuera
    // del FOV vertical/horizontal pero el centro esta dentro.
    const AABB big = makeAabb(glm::vec3(0.0f, 0.0f, -5.0f), 5.0f);
    CHECK(aabbVisible(big, frustum));
}

TEST_CASE("aabbVisible: AABB gigante que envuelve el origen es visible") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    const AABB world = makeAabb(glm::vec3(0.0f), 1000.0f);
    CHECK(aabbVisible(world, frustum));
}

TEST_CASE("worldAabb: identity matrix preserva AABB") {
    const AABB local{glm::vec3(-1.0f), glm::vec3(1.0f)};
    const AABB world = worldAabb(local, glm::mat4(1.0f));
    CHECK(world.min.x == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(world.min.y == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(world.min.z == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(world.max.x == doctest::Approx(1.0f).epsilon(k_eps));
    CHECK(world.max.y == doctest::Approx(1.0f).epsilon(k_eps));
    CHECK(world.max.z == doctest::Approx(1.0f).epsilon(k_eps));
}

TEST_CASE("worldAabb: translation mueve AABB sin cambiar tamanio") {
    const AABB local{glm::vec3(-1.0f), glm::vec3(1.0f)};
    const glm::mat4 m = glm::translate(glm::mat4(1.0f),
                                          glm::vec3(10.0f, 5.0f, -3.0f));
    const AABB world = worldAabb(local, m);
    CHECK(world.center().x == doctest::Approx(10.0f).epsilon(k_eps));
    CHECK(world.center().y == doctest::Approx(5.0f).epsilon(k_eps));
    CHECK(world.center().z == doctest::Approx(-3.0f).epsilon(k_eps));
    CHECK(world.size().x == doctest::Approx(2.0f).epsilon(k_eps));
    CHECK(world.size().y == doctest::Approx(2.0f).epsilon(k_eps));
    CHECK(world.size().z == doctest::Approx(2.0f).epsilon(k_eps));
}

TEST_CASE("worldAabb: scale 2x produce AABB 2x") {
    const AABB local{glm::vec3(-1.0f), glm::vec3(1.0f)};
    const glm::mat4 m = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    const AABB world = worldAabb(local, m);
    CHECK(world.size().x == doctest::Approx(4.0f).epsilon(k_eps));
    CHECK(world.size().y == doctest::Approx(4.0f).epsilon(k_eps));
    CHECK(world.size().z == doctest::Approx(4.0f).epsilon(k_eps));
}

TEST_CASE("worldAabb: rotacion 45° en Y crece el AABB (eje-alineado conservador)") {
    // Cubo 1x1x1 centrado en origen. Rotado 45° en Y, su bounding box
    // eje-alineado tiene size = sqrt(2) en X y Z (diagonal del cuadrado
    // base) — Y se mantiene.
    const AABB local{glm::vec3(-0.5f), glm::vec3(0.5f)};
    const glm::mat4 m = glm::rotate(glm::mat4(1.0f),
                                      glm::radians(45.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
    const AABB world = worldAabb(local, m);
    const f32 expected = std::sqrt(2.0f);
    CHECK(world.size().x == doctest::Approx(expected).epsilon(k_eps));
    CHECK(world.size().z == doctest::Approx(expected).epsilon(k_eps));
    CHECK(world.size().y == doctest::Approx(1.0f).epsilon(k_eps));
}

TEST_CASE("aabbVisible: cubo trasladado 50m al costado deja de verse") {
    const auto frustum = frustumFromViewProj(canonicalViewProj());
    const AABB local{glm::vec3(-0.5f), glm::vec3(0.5f)};
    // En el centro del frustum visible.
    const AABB centerWorld = worldAabb(
        local, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f)));
    CHECK(aabbVisible(centerWorld, frustum));

    // Trasladado 50m a la derecha (mismo z) sale del FOV horizontal.
    const AABB sideWorld = worldAabb(
        local, glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 0.0f, -5.0f)));
    CHECK_FALSE(aabbVisible(sideWorld, frustum));
}
