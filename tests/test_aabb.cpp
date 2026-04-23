// Tests de la primitiva AABB (src/core/math/AABB.h). Cubren los invariantes
// que el resto del motor asume: interseccion simetrica, contencion, expansion
// simetrica, merge, y comportamiento en cajas degeneradas (min == max).

#include <doctest/doctest.h>

#include "core/math/AABB.h"

using namespace Mood;

namespace {
constexpr float k_eps = 1e-5f;
} // namespace

TEST_CASE("AABB default es degenerada y no valida") {
    AABB a{};
    CHECK_FALSE(a.isValid());
    CHECK(a.size().x == doctest::Approx(0.0f));
    CHECK(a.size().y == doctest::Approx(0.0f));
    CHECK(a.size().z == doctest::Approx(0.0f));
}

TEST_CASE("AABB unitaria: center, size, extents, isValid") {
    AABB a{glm::vec3(0.0f), glm::vec3(2.0f, 4.0f, 6.0f)};
    CHECK(a.isValid());
    CHECK(a.size().x == doctest::Approx(2.0f));
    CHECK(a.size().y == doctest::Approx(4.0f));
    CHECK(a.size().z == doctest::Approx(6.0f));
    CHECK(a.center().x == doctest::Approx(1.0f));
    CHECK(a.center().y == doctest::Approx(2.0f));
    CHECK(a.center().z == doctest::Approx(3.0f));
    CHECK(a.extents().x == doctest::Approx(1.0f));
    CHECK(a.extents().y == doctest::Approx(2.0f));
    CHECK(a.extents().z == doctest::Approx(3.0f));
}

TEST_CASE("intersects: autointerseccion, solapamiento, contacto y separacion") {
    const AABB unit{glm::vec3(0.0f), glm::vec3(1.0f)};

    SUBCASE("interseccion consigo misma") {
        CHECK(intersects(unit, unit));
    }
    SUBCASE("solapamiento parcial") {
        const AABB other{glm::vec3(0.5f), glm::vec3(1.5f)};
        CHECK(intersects(unit, other));
        CHECK(intersects(other, unit)); // simetria
    }
    SUBCASE("contacto borde-a-borde cuenta como interseccion") {
        const AABB touching{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(2.0f, 1.0f, 1.0f)};
        CHECK(intersects(unit, touching));
    }
    SUBCASE("separadas por un eje") {
        const AABB far{glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(3.0f, 1.0f, 1.0f)};
        CHECK_FALSE(intersects(unit, far));
    }
    SUBCASE("separacion diagonal") {
        const AABB far{glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(3.0f, 3.0f, 3.0f)};
        CHECK_FALSE(intersects(unit, far));
    }
}

TEST_CASE("contains: punto interior, borde y exterior") {
    const AABB box{glm::vec3(-1.0f), glm::vec3(1.0f)};
    CHECK(contains(box, glm::vec3(0.0f)));
    CHECK(contains(box, glm::vec3(0.5f, -0.5f, 0.2f)));
    // Borde inclusivo.
    CHECK(contains(box, glm::vec3(1.0f, 0.0f, 0.0f)));
    CHECK(contains(box, glm::vec3(-1.0f, -1.0f, -1.0f)));
    // Fuera por epsilon.
    CHECK_FALSE(contains(box, glm::vec3(1.0f + k_eps, 0.0f, 0.0f)));
    CHECK_FALSE(contains(box, glm::vec3(0.0f, 2.0f, 0.0f)));
}

TEST_CASE("expanded crece simetricamente en cada eje") {
    const AABB box{glm::vec3(1.0f), glm::vec3(3.0f)};
    const AABB big = expanded(box, 0.5f);
    CHECK(big.min.x == doctest::Approx(0.5f));
    CHECK(big.min.y == doctest::Approx(0.5f));
    CHECK(big.min.z == doctest::Approx(0.5f));
    CHECK(big.max.x == doctest::Approx(3.5f));
    CHECK(big.max.y == doctest::Approx(3.5f));
    CHECK(big.max.z == doctest::Approx(3.5f));
    // El centro no se mueve.
    CHECK(big.center().x == doctest::Approx(box.center().x));
    CHECK(big.center().y == doctest::Approx(box.center().y));
    CHECK(big.center().z == doctest::Approx(box.center().z));
}

TEST_CASE("merge contiene a ambos operandos") {
    const AABB a{glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 1.0f)};
    const AABB b{glm::vec3(2.0f, -1.0f, 0.5f), glm::vec3(3.0f, 0.5f, 2.0f)};
    const AABB m = merge(a, b);

    CHECK(m.min.x == doctest::Approx(-1.0f));
    CHECK(m.min.y == doctest::Approx(-1.0f));
    CHECK(m.min.z == doctest::Approx(0.0f));
    CHECK(m.max.x == doctest::Approx(3.0f));
    CHECK(m.max.y == doctest::Approx(1.0f));
    CHECK(m.max.z == doctest::Approx(2.0f));

    // Merge debe ser idempotente y simetrico.
    CHECK(merge(a, a).min == a.min);
    CHECK(merge(a, a).max == a.max);
    const AABB ba = merge(b, a);
    CHECK(ba.min == m.min);
    CHECK(ba.max == m.max);
}

TEST_CASE("AABB degenerada (min == max) se comporta como punto") {
    const AABB point{glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 2.0f, 3.0f)};
    CHECK_FALSE(point.isValid());
    CHECK(contains(point, glm::vec3(1.0f, 2.0f, 3.0f)));
    const AABB box{glm::vec3(0.0f), glm::vec3(2.0f, 3.0f, 4.0f)};
    CHECK(intersects(point, box));
}
