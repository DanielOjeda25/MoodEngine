// Tests del helper puro `computeShadowMatrices` (Hito 16). Validamos que
// las matrices generadas dejen la escena dentro del frustum de la luz, que
// el centro caiga en NDC z aprox medio, y que el caso degenerado de
// lightDir cero no rompa la matematica.

#include <doctest/doctest.h>

#include "engine/render/ShadowMath.h"

#include <glm/gtc/matrix_inverse.hpp>

using namespace Mood;

namespace {

// Proyecta un punto world a NDC con la matriz dada. Devuelve true si esta
// dentro del frustum [-1, 1] en x/y/z.
bool projectsInside(const glm::mat4& m, const glm::vec3& p) {
    const glm::vec4 c = m * glm::vec4(p, 1.0f);
    if (c.w <= 0.0f) return false;
    const glm::vec3 ndc = glm::vec3(c) / c.w;
    return ndc.x >= -1.0f && ndc.x <= 1.0f
        && ndc.y >= -1.0f && ndc.y <= 1.0f
        && ndc.z >= -1.0f && ndc.z <= 1.0f;
}

} // namespace

TEST_CASE("computeShadowMatrices: el centro de la escena cae en el centro NDC") {
    const glm::vec3 dir(0.0f, -1.0f, 0.0f);
    const glm::vec3 center(0.0f, 0.0f, 0.0f);
    const f32 radius = 10.0f;
    const ShadowMatrices sm = computeShadowMatrices(dir, center, radius);

    const glm::vec4 c = sm.lightSpace * glm::vec4(center, 1.0f);
    REQUIRE(c.w > 0.0f);
    const glm::vec3 ndc = glm::vec3(c) / c.w;
    CHECK(ndc.x == doctest::Approx(0.0f));
    CHECK(ndc.y == doctest::Approx(0.0f));
    // z ~ 0.5: el centro de la escena queda en mid-depth del frustum.
    CHECK(ndc.z == doctest::Approx(0.0f).epsilon(0.05f)); // ortho mapea center en 0
}

TEST_CASE("computeShadowMatrices: el bounding sphere cabe en NDC con padding") {
    const glm::vec3 dir(-0.4f, -1.0f, -0.3f);
    const glm::vec3 center(2.0f, 1.0f, -3.0f);
    const f32 radius = 12.0f;
    const ShadowMatrices sm = computeShadowMatrices(dir, center, radius);

    // 6 puntos extremos del sphere (axes principales).
    const glm::vec3 axes[6] = {
        { radius, 0.0f, 0.0f}, {-radius, 0.0f, 0.0f},
        { 0.0f,  radius, 0.0f}, { 0.0f, -radius, 0.0f},
        { 0.0f, 0.0f,  radius}, { 0.0f, 0.0f, -radius}};
    for (const auto& a : axes) {
        const glm::vec3 p = center + a;
        CHECK(projectsInside(sm.lightSpace, p));
    }
}

TEST_CASE("computeShadowMatrices: lightDir cero cae al fallback (0,-1,0)") {
    const glm::vec3 dirZero(0.0f, 0.0f, 0.0f);
    const glm::vec3 center(0.0f, 0.0f, 0.0f);
    const f32 radius = 5.0f;
    const ShadowMatrices smZero = computeShadowMatrices(dirZero, center, radius);
    const ShadowMatrices smRef = computeShadowMatrices(
        glm::vec3(0.0f, -1.0f, 0.0f), center, radius);

    // Mismas matrices: el zero-dir cae al fallback (0,-1,0).
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK(smZero.lightSpace[i][j] ==
                  doctest::Approx(smRef.lightSpace[i][j]).epsilon(1e-5f));
        }
    }
}

TEST_CASE("computeShadowMatrices: lightDir no normalizado da el mismo resultado") {
    const glm::vec3 dirN(-0.4f, -1.0f, -0.3f);
    const glm::vec3 dirNN = dirN * 7.5f; // misma direccion, escala distinta
    const glm::vec3 center(0.0f, 0.0f, 0.0f);
    const f32 radius = 10.0f;
    const ShadowMatrices smN  = computeShadowMatrices(dirN,  center, radius);
    const ShadowMatrices smNN = computeShadowMatrices(dirNN, center, radius);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK(smN.lightSpace[i][j] ==
                  doctest::Approx(smNN.lightSpace[i][j]).epsilon(1e-4f));
        }
    }
}

TEST_CASE("chooseLightUp: cae a (0,0,1) cuando la luz es casi vertical") {
    CHECK(chooseLightUp(glm::vec3(0.0f, -1.0f, 0.0f)) ==
          glm::vec3(0.0f, 0.0f, 1.0f));
    CHECK(chooseLightUp(glm::vec3(0.0f,  1.0f, 0.0f)) ==
          glm::vec3(0.0f, 0.0f, 1.0f));
    // Luz oblicua: up estandar (0,1,0).
    CHECK(chooseLightUp(glm::normalize(glm::vec3(0.5f, -0.7f, 0.5f))) ==
          glm::vec3(0.0f, 1.0f, 0.0f));
}
