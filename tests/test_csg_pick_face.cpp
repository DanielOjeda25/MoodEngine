// Tests del pickFace (F2H17). Ray-cast contra los polígonos
// individuales de cada cara del brush en world space.

#include <doctest/doctest.h>

#include "engine/world/csg/Brush.h"
#include "engine/world/csg/Primitives.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

using namespace Mood;
using namespace Mood::Csg;

namespace {
constexpr f32 k_eps = 1e-3f;
}

TEST_CASE("pickFace: ray contra cara +X de box unitaria hit") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray desde (5, 0, 0) hacia -X. Debe pegar en la cara +X
    // (en x=0.5).
    const auto hit = pickFace(b, glm::vec3(5.0f, 0.0f, 0.0f),
                                  glm::vec3(-1.0f, 0.0f, 0.0f));
    REQUIRE(hit.has_value());
    // La normal de la cara hit debe ser (+1, 0, 0).
    CHECK(b.faces[*hit].plane.normal.x == doctest::Approx(1.0f).epsilon(k_eps));
}

TEST_CASE("pickFace: ray contra cara -Y (abajo) hit") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray desde (0, -5, 0) hacia +Y. Pega en cara -Y (en y=-0.5).
    const auto hit = pickFace(b, glm::vec3(0.0f, -5.0f, 0.0f),
                                  glm::vec3(0.0f, 1.0f, 0.0f));
    REQUIRE(hit.has_value());
    CHECK(b.faces[*hit].plane.normal.y == doctest::Approx(-1.0f).epsilon(k_eps));
}

TEST_CASE("pickFace: ray que NO toca el brush devuelve nullopt") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray paralelo al eje X desde (5, 5, 0) hacia +X. El brush
    // esta centrado en origen — el ray pasa por arriba.
    const auto hit = pickFace(b, glm::vec3(5.0f, 5.0f, 0.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("pickFace: ray detras del origen de la cara no hit") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray desde (5, 0, 0) hacia +X (alejandose de la box).
    // No debe hit ninguna cara.
    const auto hit = pickFace(b, glm::vec3(5.0f, 0.0f, 0.0f),
                                  glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("pickFace: ray con worldMatrix transformado") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));  // local box
    // worldMatrix la traslada a (10, 0, 0).
    const glm::mat4 world = glm::translate(glm::mat4(1.0f),
                                              glm::vec3(10.0f, 0.0f, 0.0f));
    // Ray desde (15, 0, 0) hacia -X. Box en world tiene cara +X
    // en x=10.5.
    const auto hit = pickFace(b, glm::vec3(15.0f, 0.0f, 0.0f),
                                  glm::vec3(-1.0f, 0.0f, 0.0f), world);
    REQUIRE(hit.has_value());
    CHECK(b.faces[*hit].plane.normal.x == doctest::Approx(1.0f).epsilon(k_eps));
}

TEST_CASE("pickFace: brush degenerado <4 caras devuelve nullopt") {
    Brush b;
    b.faces.push_back({Plane{glm::vec3(1, 0, 0), 0.0f}, 0});
    const auto hit = pickFace(b, glm::vec3(5.0f, 0.0f, 0.0f),
                                  glm::vec3(-1.0f, 0.0f, 0.0f));
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("pickFace: cara mas cercana gana cuando hay multiples hits") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray desde (5, 0, 0) hacia -X. Va a atravesar la box: pega
    // primero en cara +X (x=0.5), después en cara -X (x=-0.5).
    // Debe devolver la cara +X (closer = menor t).
    const auto hit = pickFace(b, glm::vec3(5.0f, 0.0f, 0.0f),
                                  glm::vec3(-1.0f, 0.0f, 0.0f));
    REQUIRE(hit.has_value());
    CHECK(b.faces[*hit].plane.normal.x == doctest::Approx(1.0f).epsilon(k_eps));
}

TEST_CASE("pickFace: ray paralelo a la cara no hit (numericamente)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Ray paralelo a la cara +X (en y, sin componente x).
    // Aun con x = 0.5 (sobre la cara), no debe hit la cara +X
    // exactamente porque el ray es paralelo. Otras caras pueden
    // hit; lo que validamos es que la cara hit (si la hay) NO sea
    // la +X.
    const auto hit = pickFace(b, glm::vec3(0.5001f, 5.0f, 0.0f),
                                  glm::vec3(0.0f, -1.0f, 0.0f));
    if (hit.has_value()) {
        // Si pega en algo, no debe ser la cara +X (paralela al ray).
        // Probablemente pega en +Y al entrar.
        CHECK_FALSE(b.faces[*hit].plane.normal.x > 0.99f);
    }
}

TEST_CASE("pickFace: cilindro con N caras encuentra una") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 8);
    // Ray desde (5, 0, 0) hacia -X. Pega en alguna cara lateral
    // (la del lado +X del cilindro).
    const auto hit = pickFace(b, glm::vec3(5.0f, 0.0f, 0.0f),
                                  glm::vec3(-1.0f, 0.0f, 0.0f));
    REQUIRE(hit.has_value());
    // La cara hit debe ser una lateral (normal en plano XZ, y ≈ 0).
    CHECK(std::fabs(b.faces[*hit].plane.normal.y) < k_eps);
    CHECK(b.faces[*hit].plane.normal.x > 0.5f);  // del lado +X
}
