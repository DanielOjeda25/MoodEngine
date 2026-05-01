// Tests del PhysicsWorld::raycast (Hito 33 Bloque 1).
// Crea bodies estaticos directos via Jolt y verifica el resultado del
// CastRay (hit/miss, distancia, normal, bodyId).

#include <doctest/doctest.h>

#include "engine/physics/PhysicsWorld.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

using namespace Mood;

TEST_CASE("PhysicsWorld::raycast: impacto frontal contra box estatico") {
    PhysicsWorld pw;

    // Box estatico de 1x1x1m centrado en (0, 0, -5). Cara mas cercana al
    // origen en z=-4.5.
    const u32 boxId = pw.createBody(glm::vec3(0.0f, 0.0f, -5.0f),
                                      CollisionShape::Box,
                                      glm::vec3(0.5f),
                                      BodyType::Static);
    REQUIRE(boxId != 0u);

    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f);

    CHECK(hit.hit);
    CHECK(hit.distance == doctest::Approx(4.5f).epsilon(0.01));
    CHECK(hit.point.z == doctest::Approx(-4.5f).epsilon(0.01));
    // Normal debe apuntar hacia el origen (eje +Z).
    CHECK(hit.normal.z == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(hit.bodyId == boxId);
}

TEST_CASE("PhysicsWorld::raycast: rayo apunta al vacio devuelve hit=false") {
    PhysicsWorld pw;
    pw.createBody(glm::vec3(0.0f, 0.0f, -5.0f),
                   CollisionShape::Box,
                   glm::vec3(0.5f),
                   BodyType::Static);

    // Rayo +X — el box esta en -Z, no lo cruza.
    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f),
                                  100.0f);

    CHECK_FALSE(hit.hit);
    CHECK(hit.bodyId == 0u);
}

TEST_CASE("PhysicsWorld::raycast: maxDistance corta el rayo antes del body") {
    PhysicsWorld pw;
    pw.createBody(glm::vec3(0.0f, 0.0f, -10.0f),
                   CollisionShape::Box,
                   glm::vec3(0.5f),
                   BodyType::Static);

    // Box a ~9.5m de distancia. Limite = 5m -> deberia missear.
    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  5.0f);

    CHECK_FALSE(hit.hit);
}

TEST_CASE("PhysicsWorld::raycast: direccion no normalizada es respetada") {
    // El header dice que la direccion no necesita estar normalizada — se
    // escala internamente por maxDistance. Verificamos que un vector de
    // largo 2 no afecta el resultado.
    PhysicsWorld pw;
    const u32 boxId = pw.createBody(glm::vec3(0.0f, 0.0f, -5.0f),
                                      CollisionShape::Box,
                                      glm::vec3(0.5f),
                                      BodyType::Static);

    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -2.0f),
                                  20.0f);

    CHECK(hit.hit);
    CHECK(hit.bodyId == boxId);
    CHECK(hit.distance == doctest::Approx(4.5f).epsilon(0.01));
}

TEST_CASE("PhysicsWorld::raycast: detecta el primer body en el path") {
    PhysicsWorld pw;
    // Dos boxes alineados — el rayo debe pegar al mas cercano.
    const u32 nearId = pw.createBody(glm::vec3(0.0f, 0.0f, -3.0f),
                                       CollisionShape::Box,
                                       glm::vec3(0.5f),
                                       BodyType::Static);
    pw.createBody(glm::vec3(0.0f, 0.0f, -8.0f),
                   CollisionShape::Box,
                   glm::vec3(0.5f),
                   BodyType::Static);

    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f);

    CHECK(hit.hit);
    CHECK(hit.bodyId == nearId);
    CHECK(hit.distance == doctest::Approx(2.5f).epsilon(0.01));
}
