// Tests de la primitiva Plane (src/core/math/Plane.h). Verifican la
// convencion del motor (dot(n,p) + d = 0) y los helpers usados por
// F2H3 (frustum culling) y F2H11 (CSG brush -> mesh):
//   - signedDistance: signos consistentes con la normal.
//   - planeFromPointAndNormal: el punto satisface el plano que genera.
//   - intersectThreePlanes: regla de Cramer, casos resoluble e
//     no-resoluble (paralelos, coincidentes, casi-paralelos).
//
// El test mas critico para CSG es "interseccion en vertice de cubo":
// las 3 caras +X / +Y / +Z de una box unitaria centrada en origen
// deben intersectar exactamente en el vertice (0.5, 0.5, 0.5).

#include <doctest/doctest.h>

#include "core/math/Plane.h"

#include <glm/geometric.hpp>

using namespace Mood;

namespace {
constexpr f32 k_eps = 1e-4f;
}

// ============================================================
// signedDistance
// ============================================================

TEST_CASE("signedDistance: punto sobre el plano da 0") {
    const Plane p{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    CHECK(signedDistance(p, glm::vec3(0.0f)) == doctest::Approx(0.0f));
    CHECK(signedDistance(p, glm::vec3(5.0f, 0.0f, -3.0f))
              == doctest::Approx(0.0f));
}

TEST_CASE("signedDistance: punto del lado de la normal es positivo") {
    const Plane p{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};  // y = 0
    CHECK(signedDistance(p, glm::vec3(0.0f, 1.0f, 0.0f))
              == doctest::Approx(1.0f));
    CHECK(signedDistance(p, glm::vec3(0.0f, 0.5f, 0.0f))
              == doctest::Approx(0.5f));
}

TEST_CASE("signedDistance: punto del lado opuesto a la normal es negativo") {
    const Plane p{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    CHECK(signedDistance(p, glm::vec3(0.0f, -1.0f, 0.0f))
              == doctest::Approx(-1.0f));
}

TEST_CASE("signedDistance: respeta el offset distance") {
    // Plano y = 0.5  =>  dot(n,p) + d = 0  =>  d = -0.5 con n=(0,1,0)
    const Plane p{glm::vec3(0.0f, 1.0f, 0.0f), -0.5f};
    CHECK(signedDistance(p, glm::vec3(0.0f, 0.5f, 0.0f))
              == doctest::Approx(0.0f));
    CHECK(signedDistance(p, glm::vec3(0.0f, 1.0f, 0.0f))
              == doctest::Approx(0.5f));
    CHECK(signedDistance(p, glm::vec3(0.0f, 0.0f, 0.0f))
              == doctest::Approx(-0.5f));
}

TEST_CASE("signedDistance: caso CSG cara +X de box unitaria") {
    // Cara +X de una box centrada en origen lado 1: x = 0.5,
    // normal hacia afuera (+X). Punto (0.6,0,0) esta fuera del brush
    // por esa cara -> signedDistance > 0.
    const Plane p = planeFromPointAndNormal(glm::vec3(0.5f, 0.0f, 0.0f),
                                             glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK(signedDistance(p, glm::vec3(0.6f, 0.0f, 0.0f))
              == doctest::Approx(0.1f));
    CHECK(signedDistance(p, glm::vec3(0.4f, 0.0f, 0.0f))
              == doctest::Approx(-0.1f));
}

// ============================================================
// planeFromPointAndNormal
// ============================================================

TEST_CASE("planeFromPointAndNormal: el punto pertenece al plano resultante") {
    const glm::vec3 pt(3.0f, -2.0f, 5.0f);
    const glm::vec3 n = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    const Plane p = planeFromPointAndNormal(pt, n);
    CHECK(signedDistance(p, pt) == doctest::Approx(0.0f).epsilon(k_eps));
}

TEST_CASE("planeFromPointAndNormal: normal preservada literal") {
    const glm::vec3 n(0.0f, 0.0f, -1.0f);
    const Plane p = planeFromPointAndNormal(glm::vec3(7.0f, 8.0f, 9.0f), n);
    CHECK(p.normal.x == doctest::Approx(n.x));
    CHECK(p.normal.y == doctest::Approx(n.y));
    CHECK(p.normal.z == doctest::Approx(n.z));
}

// ============================================================
// intersectThreePlanes
// ============================================================

TEST_CASE("intersectThreePlanes: tres planos canonicos cortan en origen") {
    const Plane px{glm::vec3(1.0f, 0.0f, 0.0f), 0.0f};  // x = 0
    const Plane py{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};  // y = 0
    const Plane pz{glm::vec3(0.0f, 0.0f, 1.0f), 0.0f};  // z = 0
    glm::vec3 out{99.0f};
    REQUIRE(intersectThreePlanes(px, py, pz, out));
    CHECK(out.x == doctest::Approx(0.0f));
    CHECK(out.y == doctest::Approx(0.0f));
    CHECK(out.z == doctest::Approx(0.0f));
}

TEST_CASE("intersectThreePlanes: vertice (+X,+Y,+Z) de box unitaria") {
    // Las tres caras "positivas" de una box centrada en origen
    // de lado 1 deben intersectar en (0.5, 0.5, 0.5).
    const Plane px = planeFromPointAndNormal(glm::vec3(0.5f, 0.0f, 0.0f),
                                              glm::vec3(1.0f, 0.0f, 0.0f));
    const Plane py = planeFromPointAndNormal(glm::vec3(0.0f, 0.5f, 0.0f),
                                              glm::vec3(0.0f, 1.0f, 0.0f));
    const Plane pz = planeFromPointAndNormal(glm::vec3(0.0f, 0.0f, 0.5f),
                                              glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 out{};
    REQUIRE(intersectThreePlanes(px, py, pz, out));
    CHECK(out.x == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(out.y == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(out.z == doctest::Approx(0.5f).epsilon(k_eps));
}

TEST_CASE("intersectThreePlanes: vertice (-X,-Y,-Z) de box unitaria") {
    const Plane nx = planeFromPointAndNormal(glm::vec3(-0.5f, 0.0f, 0.0f),
                                              glm::vec3(-1.0f, 0.0f, 0.0f));
    const Plane ny = planeFromPointAndNormal(glm::vec3(0.0f, -0.5f, 0.0f),
                                              glm::vec3(0.0f, -1.0f, 0.0f));
    const Plane nz = planeFromPointAndNormal(glm::vec3(0.0f, 0.0f, -0.5f),
                                              glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 out{};
    REQUIRE(intersectThreePlanes(nx, ny, nz, out));
    CHECK(out.x == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(out.y == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(out.z == doctest::Approx(-0.5f).epsilon(k_eps));
}

TEST_CASE("intersectThreePlanes: dos planos paralelos no resuelven") {
    const Plane a{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};   // y = 0
    const Plane b{glm::vec3(0.0f, 1.0f, 0.0f), -1.0f};  // y = 1 (paralelo a a)
    const Plane c{glm::vec3(1.0f, 0.0f, 0.0f), 0.0f};   // x = 0
    glm::vec3 out{1.0f, 2.0f, 3.0f};
    CHECK_FALSE(intersectThreePlanes(a, b, c, out));
}

TEST_CASE("intersectThreePlanes: tres planos coincidentes no resuelven") {
    const Plane a{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    const Plane b{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    const Plane c{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    glm::vec3 out{};
    CHECK_FALSE(intersectThreePlanes(a, b, c, out));
}

TEST_CASE("intersectThreePlanes: tres planos casi paralelos no resuelven") {
    // Normales separadas por menos de kPlaneEpsilon en magnitud del cross.
    const Plane a{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    const Plane b{glm::normalize(glm::vec3(0.0f, 1.0f, 1e-6f)), -1.0f};
    const Plane c{glm::normalize(glm::vec3(1e-6f, 1.0f, 0.0f)), -2.0f};
    glm::vec3 out{};
    CHECK_FALSE(intersectThreePlanes(a, b, c, out));
}

TEST_CASE("intersectThreePlanes: el punto resultante satisface los 3 planos") {
    // Tres planos arbitrarios no canonicos.
    const Plane a = planeFromPointAndNormal(
        glm::vec3(1.0f, 2.0f, 3.0f),
        glm::normalize(glm::vec3(1.0f, 0.5f, 0.0f)));
    const Plane b = planeFromPointAndNormal(
        glm::vec3(-1.0f, 4.0f, 0.0f),
        glm::normalize(glm::vec3(0.2f, -1.0f, 0.5f)));
    const Plane c = planeFromPointAndNormal(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::normalize(glm::vec3(0.3f, 0.7f, 1.0f)));
    glm::vec3 out{};
    REQUIRE(intersectThreePlanes(a, b, c, out));
    // Cada plano debe dar signedDistance ~0 al punto interseccion.
    CHECK(signedDistance(a, out) == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(signedDistance(b, out) == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(signedDistance(c, out) == doctest::Approx(0.0f).epsilon(k_eps));
}

TEST_CASE("intersectThreePlanes: determinismo bit-a-bit") {
    const Plane a{glm::vec3(1.0f, 0.0f, 0.0f), -0.5f};
    const Plane b{glm::vec3(0.0f, 1.0f, 0.0f), -0.3f};
    const Plane c{glm::vec3(0.0f, 0.0f, 1.0f), -0.1f};
    glm::vec3 r1{}, r2{};
    REQUIRE(intersectThreePlanes(a, b, c, r1));
    REQUIRE(intersectThreePlanes(a, b, c, r2));
    CHECK(r1.x == r2.x);
    CHECK(r1.y == r2.y);
    CHECK(r1.z == r2.z);
}

TEST_CASE("intersectThreePlanes: el orden de los planos no cambia el punto") {
    // Las tres normales son linealmente independientes; cualquier
    // permutacion de los planos resuelve al mismo punto (mod precision).
    const Plane a{glm::vec3(1.0f, 0.0f, 0.0f), -0.5f};
    const Plane b{glm::vec3(0.0f, 1.0f, 0.0f), -0.3f};
    const Plane c{glm::vec3(0.0f, 0.0f, 1.0f), -0.1f};
    glm::vec3 r_abc{}, r_bca{}, r_cab{};
    REQUIRE(intersectThreePlanes(a, b, c, r_abc));
    REQUIRE(intersectThreePlanes(b, c, a, r_bca));
    REQUIRE(intersectThreePlanes(c, a, b, r_cab));
    CHECK(r_abc.x == doctest::Approx(r_bca.x).epsilon(k_eps));
    CHECK(r_abc.y == doctest::Approx(r_bca.y).epsilon(k_eps));
    CHECK(r_abc.z == doctest::Approx(r_bca.z).epsilon(k_eps));
    CHECK(r_abc.x == doctest::Approx(r_cab.x).epsilon(k_eps));
    CHECK(r_abc.y == doctest::Approx(r_cab.y).epsilon(k_eps));
    CHECK(r_abc.z == doctest::Approx(r_cab.z).epsilon(k_eps));
}

TEST_CASE("intersectThreePlanes: outPoint no se modifica si falla") {
    // Contrato no documentado pero util: si falla, el llamador puede
    // confiar en que `outPoint` queda con su valor previo... PERO el
    // codigo actual no lo garantiza explicitamente. Este test
    // documenta el comportamiento real: si falla, outPoint mantiene
    // su valor anterior.
    const Plane a{glm::vec3(0.0f, 1.0f, 0.0f), 0.0f};
    const Plane b{glm::vec3(0.0f, 1.0f, 0.0f), -1.0f};  // paralelo a a
    const Plane c{glm::vec3(1.0f, 0.0f, 0.0f), 0.0f};
    const glm::vec3 sentinel(123.0f, 456.0f, 789.0f);
    glm::vec3 out = sentinel;
    REQUIRE_FALSE(intersectThreePlanes(a, b, c, out));
    CHECK(out.x == sentinel.x);
    CHECK(out.y == sentinel.y);
    CHECK(out.z == sentinel.z);
}
