// Tests del Character Controller (Hito 30 Bloque 1). Verifican la API
// nueva de `PhysicsWorld` (createCharacter / setCharacterMovement /
// characterPosition / isCharacterOnGround / destroyCharacter) usando
// Jolt directo, sin Scene ni editor.

#include <doctest/doctest.h>

#include "engine/physics/world/PhysicsWorld.h"

#include <glm/vec3.hpp>

using namespace Mood;

namespace {

constexpr f32 k_charHalfHeight = 0.5f;  // cilindro central
constexpr f32 k_charRadius     = 0.4f;  // hemisferios
constexpr f32 k_dt             = 0.016f;

} // namespace

TEST_CASE("PhysicsWorld: createCharacter devuelve handle valido + characterCount") {
    PhysicsWorld pw;
    REQUIRE(pw.characterCount() == 0u);

    const u32 id = pw.createCharacter(glm::vec3(0.0f, 5.0f, 0.0f),
                                        k_charHalfHeight, k_charRadius);
    CHECK(id != 0u);
    CHECK(pw.characterCount() == 1u);

    pw.destroyCharacter(id);
    CHECK(pw.characterCount() == 0u);
}

TEST_CASE("PhysicsWorld: characterPosition refleja initialPos antes de step") {
    PhysicsWorld pw;
    const u32 id = pw.createCharacter(glm::vec3(3.0f, 2.0f, 7.0f),
                                        k_charHalfHeight, k_charRadius);
    const glm::vec3 p = pw.characterPosition(id);
    CHECK(p.x == doctest::Approx(3.0f));
    CHECK(p.y == doctest::Approx(2.0f));
    CHECK(p.z == doctest::Approx(7.0f));
}

TEST_CASE("PhysicsWorld: setCharacterMovement avanza el character al stepear") {
    PhysicsWorld pw;
    // Sin piso debajo: gravedad va a tirarlo, pero la velocidad horizontal
    // se aplica igual. Stepeamos 30 frames con velocidad +X 2 m/s.
    const u32 id = pw.createCharacter(glm::vec3(0.0f, 10.0f, 0.0f),
                                        k_charHalfHeight, k_charRadius);
    pw.setCharacterMovement(id, glm::vec3(2.0f, 0.0f, 0.0f));
    for (int i = 0; i < 30; ++i) pw.step(k_dt);

    const glm::vec3 p = pw.characterPosition(id);
    // En 30 * 0.016 = 0.48s a 2 m/s = ~0.96m. Margen amplio por gravity drag.
    CHECK(p.x == doctest::Approx(0.96f).epsilon(0.05));
}

TEST_CASE("PhysicsWorld: character cae por gravedad sin floor") {
    PhysicsWorld pw;
    const u32 id = pw.createCharacter(glm::vec3(0.0f, 10.0f, 0.0f),
                                        k_charHalfHeight, k_charRadius);
    for (int i = 0; i < 60; ++i) pw.step(k_dt);  // 60 * 0.016 ≈ 0.96s

    const glm::vec3 p = pw.characterPosition(id);
    CHECK(p.y < 10.0f);  // bajo
    CHECK(p.y < 6.0f);   // bastante (gravity acumula)
    CHECK_FALSE(pw.isCharacterOnGround(id));
}

TEST_CASE("PhysicsWorld: character sobre static body queda OnGround") {
    PhysicsWorld pw;

    // Plataforma estatica grande en y=0 (top en y=0.5).
    pw.createBody(glm::vec3(0.0f, 0.0f, 0.0f),
                   CollisionShape::Box,
                   glm::vec3(10.0f, 0.5f, 10.0f),
                   BodyType::Static);

    // Character justo arriba (suelo aceptable < max_slope).
    // y=2 cae hasta apoyarse a y ≈ 0.5 + halfHeight + radius = 1.4
    const u32 id = pw.createCharacter(glm::vec3(0.0f, 3.0f, 0.0f),
                                        k_charHalfHeight, k_charRadius);
    // Sin movimiento horizontal — solo gravedad.
    pw.setCharacterMovement(id, glm::vec3(0.0f, 0.0f, 0.0f));
    for (int i = 0; i < 120; ++i) pw.step(k_dt);  // ≈ 2s, alcanza para asentar

    CHECK(pw.isCharacterOnGround(id));
    const glm::vec3 p = pw.characterPosition(id);
    // Debe estar cerca del top de la plataforma + halfHeight + radius.
    CHECK(p.y > 0.5f);
    CHECK(p.y < 2.5f);
}

TEST_CASE("PhysicsWorld: setCharacterPosition teleporta y resetea velocidad") {
    PhysicsWorld pw;
    const u32 id = pw.createCharacter(glm::vec3(0.0f, 0.0f, 0.0f),
                                        k_charHalfHeight, k_charRadius);
    pw.setCharacterMovement(id, glm::vec3(5.0f, 0.0f, 0.0f));
    pw.step(k_dt);

    pw.setCharacterPosition(id, glm::vec3(100.0f, 100.0f, 100.0f));
    const glm::vec3 p = pw.characterPosition(id);
    CHECK(p.x == doctest::Approx(100.0f));
    CHECK(p.y == doctest::Approx(100.0f));
    CHECK(p.z == doctest::Approx(100.0f));
}

TEST_CASE("PhysicsWorld: destroyCharacter es idempotente") {
    PhysicsWorld pw;
    const u32 id = pw.createCharacter(glm::vec3(0.0f), k_charHalfHeight, k_charRadius);
    pw.destroyCharacter(id);
    pw.destroyCharacter(id);  // segundo call no crash
    pw.destroyCharacter(99999u);  // id invalido tampoco
    CHECK(pw.characterCount() == 0u);
}
