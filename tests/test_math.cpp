// Tests de la matematica basica usada por el motor. Sirven tanto para
// validar las operaciones como para confirmar que la infraestructura de
// doctest + GLM quedo bien enganchada al build.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>

TEST_CASE("glm::mat4 identidad transforma vec4 trivialmente") {
    const glm::mat4 id(1.0f);
    const glm::vec4 v(1.0f, 2.0f, 3.0f, 1.0f);
    const glm::vec4 r = id * v;
    CHECK(r.x == doctest::Approx(1.0f));
    CHECK(r.y == doctest::Approx(2.0f));
    CHECK(r.z == doctest::Approx(3.0f));
    CHECK(r.w == doctest::Approx(1.0f));
}

TEST_CASE("rotacion de 90 grados en Y mapea +X a -Z") {
    const glm::mat4 rot = glm::rotate(
        glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec4 x(1.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 r = rot * x;
    CHECK(r.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(r.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(r.z == doctest::Approx(-1.0f).epsilon(1e-4));
}

TEST_CASE("lookAt y perspective producen matrices invertibles") {
    const glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    // Invertibilidad: det != 0.
    CHECK(glm::determinant(view) != doctest::Approx(0.0f));
    CHECK(glm::determinant(proj) != doctest::Approx(0.0f));
}

TEST_CASE("perspective respeta aspect ratio simetrico") {
    // Con aspect=1 las columnas x e y deben tener la misma escala.
    const glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    CHECK(std::abs(proj[0][0]) == doctest::Approx(std::abs(proj[1][1])));
}
