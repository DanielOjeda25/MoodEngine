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

TEST_CASE("PhysicsWorld::raycast: ignoredBodyId saltea el body (Hito 34 B)") {
    PhysicsWorld pw;
    // Dos boxes alineados. Ignorar el cercano: deberia pegarle al de atras.
    const u32 nearId = pw.createBody(glm::vec3(0.0f, 0.0f, -3.0f),
                                       CollisionShape::Box,
                                       glm::vec3(0.5f),
                                       BodyType::Static);
    const u32 farId  = pw.createBody(glm::vec3(0.0f, 0.0f, -8.0f),
                                       CollisionShape::Box,
                                       glm::vec3(0.5f),
                                       BodyType::Static);

    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f,
                                  /*ignoredBodyId=*/nearId);

    CHECK(hit.hit);
    CHECK(hit.bodyId == farId);
    CHECK(hit.distance == doctest::Approx(7.5f).epsilon(0.01));
}

TEST_CASE("PhysicsWorld::raycast: ignoredBodyId del unico body devuelve miss (Hito 34 B)") {
    PhysicsWorld pw;
    const u32 only = pw.createBody(glm::vec3(0.0f, 0.0f, -3.0f),
                                     CollisionShape::Box,
                                     glm::vec3(0.5f),
                                     BodyType::Static);

    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f,
                                  /*ignoredBodyId=*/only);

    CHECK_FALSE(hit.hit);
    CHECK(hit.bodyId == 0u);
}

TEST_CASE("PhysicsWorld::raycast: layerMask filtra Static vs Moving (Hito 39 C)") {
    PhysicsWorld pw;
    // Static box mas cercano + Dynamic box mas lejos en la misma direccion.
    const u32 staticId  = pw.createBody(glm::vec3(0.0f, 0.0f, -3.0f),
                                          CollisionShape::Box,
                                          glm::vec3(0.5f),
                                          BodyType::Static);
    const u32 dynamicId = pw.createBody(glm::vec3(0.0f, 0.0f, -8.0f),
                                          CollisionShape::Box,
                                          glm::vec3(0.5f),
                                          BodyType::Dynamic);

    // Mask = 1 (solo Static layer 0): pega al static.
    auto hitStatic = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f, 0u, /*layerMask=*/1u);
    CHECK(hitStatic.hit);
    CHECK(hitStatic.bodyId == staticId);

    // Mask = 2 (solo Moving layer 1): se saltea el static, pega al dynamic.
    auto hitDynamic = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f, 0u, /*layerMask=*/2u);
    CHECK(hitDynamic.hit);
    CHECK(hitDynamic.bodyId == dynamicId);

    // Mask = 0 (nada): miss.
    auto hitNone = pw.raycast(glm::vec3(0.0f),
                                glm::vec3(0.0f, 0.0f, -1.0f),
                                20.0f, 0u, /*layerMask=*/0u);
    CHECK_FALSE(hitNone.hit);
}

TEST_CASE("PhysicsWorld::raycast: ignoredBodyId == 0 (default) no filtra nada (Hito 34 B)") {
    PhysicsWorld pw;
    const u32 boxId = pw.createBody(glm::vec3(0.0f, 0.0f, -5.0f),
                                      CollisionShape::Box,
                                      glm::vec3(0.5f),
                                      BodyType::Static);

    // Sin filtro (default arg) — equivalente a no pasar nada.
    const auto hit = pw.raycast(glm::vec3(0.0f),
                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                  20.0f);

    CHECK(hit.hit);
    CHECK(hit.bodyId == boxId);
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
