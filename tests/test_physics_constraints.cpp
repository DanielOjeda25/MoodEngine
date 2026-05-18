// F2H65 Bloque H: tests headless de PhysicsWorld constraint API.
// Verifican que los 3 tipos (Hinge/Distance/Point) crean, agregan, y
// destruyen constraints en Jolt sin Scene ni editor.

#include <doctest/doctest.h>

#include "engine/physics/world/PhysicsWorld.h"

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>  // glm::distance

using namespace Mood;

namespace {

constexpr f32 k_dt = 0.016f;

} // namespace

TEST_CASE("PhysicsWorld F2H65: createHingeConstraint devuelve handle + constraintCount") {
    PhysicsWorld pw;
    const u32 a = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 b = pw.createBody(glm::vec3(2, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    REQUIRE(pw.constraintCount() == 0u);

    const u32 c = pw.createHingeConstraint(
        a, b,
        /*pivot*/   glm::vec3(1.0f, 5.0f, 0.0f),
        /*axis*/    glm::vec3(0.0f, 1.0f, 0.0f),
        /*limitMin*/ -180.0f, /*limitMax*/ 180.0f);
    CHECK(c != 0u);
    CHECK(pw.constraintCount() == 1u);

    pw.destroyConstraint(c);
    CHECK(pw.constraintCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H65: createDistanceConstraint mantiene distancia entre bodies") {
    // 2 bodies a distancia 3.0. Constraint min=max=3.0 -> rigido.
    PhysicsWorld pw;
    const u32 a = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 b = pw.createBody(glm::vec3(3, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);

    const u32 c = pw.createDistanceConstraint(
        a, b,
        glm::vec3(0, 5, 0), glm::vec3(3, 5, 0),
        3.0f, 3.0f);
    REQUIRE(c != 0u);

    // Tick varios frames -- los 2 bodies caen por gravedad pero la
    // distancia entre ellos debe mantenerse ~3.0 (constraint rigido).
    for (int i = 0; i < 30; ++i) pw.step(k_dt);

    const glm::vec3 pa = pw.bodyPosition(a);
    const glm::vec3 pb = pw.bodyPosition(b);
    const f32 dist = glm::distance(pa, pb);
    CHECK(dist == doctest::Approx(3.0f).epsilon(0.05));  // 5% slop
}

TEST_CASE("PhysicsWorld F2H65: createPointConstraint comparte pivot entre bodies") {
    // Body B fijo (Static), Body A dynamic. El point constraint fuerza
    // que A no se aleje del pivot world (1, 5, 0). Sin physics A caeria
    // por gravedad; con el constraint, queda colgando del pivot.
    PhysicsWorld pw;
    const u32 a = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 b = pw.createBody(glm::vec3(1, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Static, 1.0f);

    const u32 c = pw.createPointConstraint(a, b, glm::vec3(0.5f, 5.0f, 0.0f));
    REQUIRE(c != 0u);

    // Tick varios frames. Body A oscila alrededor del pivot pero su
    // posicion world inicial (0, 5, 0) queda dentro de un radio
    // razonable (~1m) del pivot (0.5, 5, 0) -- el constraint impone
    // distancia ≈ 0.5 al pivot del joint en A.
    for (int i = 0; i < 60; ++i) pw.step(k_dt);

    const glm::vec3 pa = pw.bodyPosition(a);
    // No assertamos posicion exacta (penduliza). Solo que no haya caido
    // mas alla de 2m del pivot -- el constraint lo mantiene cerca.
    const f32 distToPivot = glm::distance(pa, glm::vec3(0.5f, 5.0f, 0.0f));
    CHECK(distToPivot < 2.0f);
}

TEST_CASE("PhysicsWorld F2H65: createHingeConstraint con bodyA=0 retorna 0") {
    PhysicsWorld pw;
    const u32 b = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 c = pw.createHingeConstraint(0, b, glm::vec3(0), glm::vec3(0,1,0),
                                              -180.0f, 180.0f);
    CHECK(c == 0u);
    CHECK(pw.constraintCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H65: destroyConstraint idempotente para id invalido") {
    PhysicsWorld pw;
    pw.destroyConstraint(0);    // id 0
    pw.destroyConstraint(999);  // id inexistente
    CHECK(pw.constraintCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H65: multiples constraints coexisten y se destruyen selectivamente") {
    PhysicsWorld pw;
    // 4 bodies en linea, cada par enlazado por un constraint distinto.
    const u32 a = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 b = pw.createBody(glm::vec3(2, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Static, 1.0f);
    const u32 c = pw.createBody(glm::vec3(4, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Dynamic, 1.0f);
    const u32 d = pw.createBody(glm::vec3(6, 5, 0), CollisionShape::Box,
                                  glm::vec3(0.5f), BodyType::Static, 1.0f);

    const u32 hinge = pw.createHingeConstraint(
        a, b, glm::vec3(1, 5, 0), glm::vec3(0, 1, 0), -90.0f, 90.0f);
    const u32 dist  = pw.createDistanceConstraint(
        c, d, glm::vec3(4, 5, 0), glm::vec3(6, 5, 0), 2.0f, 2.0f);
    const u32 point = pw.createPointConstraint(a, d, glm::vec3(0, 5, 0));
    REQUIRE(hinge != 0u);
    REQUIRE(dist  != 0u);
    REQUIRE(point != 0u);
    CHECK(pw.constraintCount() == 3u);
    // Los 3 ids deben ser distintos (handles unicos del map interno).
    CHECK(hinge != dist);
    CHECK(dist  != point);
    CHECK(hinge != point);

    // Destruir el del medio (Distance) deja los otros 2 intactos.
    pw.destroyConstraint(dist);
    CHECK(pw.constraintCount() == 2u);
    // Re-destruir el mismo id es no-op.
    pw.destroyConstraint(dist);
    CHECK(pw.constraintCount() == 2u);

    pw.destroyConstraint(hinge);
    pw.destroyConstraint(point);
    CHECK(pw.constraintCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H65: Hinge dynamic anclada a static no cae libremente") {
    // Body A dynamic colgado de un static B via Hinge horizontal (Z axis).
    // Gravedad genera torque, pero el hinge fija el pivot world -> A
    // oscila como pendulo en lugar de caer al infinito. Comprobamos que
    // tras varios ticks A sigue en un rango Y razonable (sin cero-G no
    // caeria mas de unos metros del pivot).
    PhysicsWorld pw;
    const u32 anchor = pw.createBody(glm::vec3(0, 5, 0), CollisionShape::Sphere,
                                       glm::vec3(0.2f), BodyType::Static, 1.0f);
    const u32 swing  = pw.createBody(glm::vec3(1, 5, 0), CollisionShape::Box,
                                       glm::vec3(0.2f, 0.5f, 0.2f),
                                       BodyType::Dynamic, 1.0f);
    const u32 c = pw.createHingeConstraint(
        swing, anchor,
        /*pivot*/ glm::vec3(0, 5, 0),
        /*axis*/  glm::vec3(0, 0, 1),
        /*min*/  -180.0f, /*max*/ 180.0f);
    REQUIRE(c != 0u);

    for (int i = 0; i < 90; ++i) pw.step(k_dt);

    const glm::vec3 p = pw.bodyPosition(swing);
    // En caida libre (sin constraint) tras 90 frames a dt=0.016 = 1.44s,
    // body habria caido 10m+ y se aleja del pivot. Con hinge, queda
    // dentro de un radio ~2m del pivot (limitado por la longitud del
    // brazo ~1m).
    const f32 dist = glm::distance(p, glm::vec3(0, 5, 0));
    CHECK(dist < 2.0f);
}
