// Tests del helper puro `computeShadowMatrices` (Hito 16). Validamos que
// las matrices generadas dejen la escena dentro del frustum de la luz, que
// el centro caiga en NDC z aprox medio, y que el caso degenerado de
// lightDir cero no rompa la matematica.

#include <doctest/doctest.h>

#include "engine/render/pipeline/ShadowMath.h"

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

// =============================================================================
// F2H60: tests del esquema PSSM + computeCascadeShadowMatrices
// =============================================================================

TEST_CASE("computeCsmSplits: 1 cascada devuelve [near, far]") {
    const auto s = computeCsmSplits(0.1f, 100.0f, 1, 0.5f);
    CHECK(s[0] == doctest::Approx(0.1f));
    CHECK(s[1] == doctest::Approx(100.0f));
}

TEST_CASE("computeCsmSplits: 4 cascadas con lambda 1.0 (puramente log) son monotonas crecientes") {
    const auto s = computeCsmSplits(0.1f, 100.0f, 4, 1.0f);
    CHECK(s[0] == doctest::Approx(0.1f));
    CHECK(s[4] == doctest::Approx(100.0f));
    // Cada split estrictamente mayor que el anterior.
    for (u32 i = 1; i <= 4; ++i) {
        CHECK(s[i] > s[i - 1]);
    }
    // Log puro: ratio constante entre splits consecutivos.
    // ratio = (far/near)^(1/N) = (100/0.1)^(1/4) ≈ 5.62.
    const f32 expectedRatio = std::pow(1000.0f, 0.25f);
    for (u32 i = 1; i <= 4; ++i) {
        const f32 actualRatio = s[i] / s[i - 1];
        CHECK(actualRatio == doctest::Approx(expectedRatio).epsilon(1e-4f));
    }
}

TEST_CASE("computeCsmSplits: lambda 0.0 (puramente lineal) da splits uniformes") {
    const auto s = computeCsmSplits(0.0f, 100.0f, 4, 0.0f);
    // El clamp interno fuerza near >= 1e-4, asi que el primer split queda
    // muy cerca de 0 pero no exacto.
    CHECK(s[1] == doctest::Approx(25.0f).epsilon(1e-3f));
    CHECK(s[2] == doctest::Approx(50.0f).epsilon(1e-3f));
    CHECK(s[3] == doctest::Approx(75.0f).epsilon(1e-3f));
    CHECK(s[4] == doctest::Approx(100.0f));
}

TEST_CASE("computeCsmSplits: cascadeCount=0 cae a 1 cascada (sanity)") {
    const auto s = computeCsmSplits(0.5f, 50.0f, 0, 0.5f);
    CHECK(s[0] == doctest::Approx(0.5f));
    CHECK(s[1] == doctest::Approx(50.0f));
}

TEST_CASE("computeCascadeShadowMatrices: cascada cerca cubre menos area que cascada lejos") {
    // Camera perspectiva tipica.
    const glm::mat4 camProj = glm::perspective(glm::radians(60.0f),
                                                  16.0f / 9.0f, 0.1f, 100.0f);
    const glm::mat4 camView = glm::lookAt(glm::vec3(0.0f, 1.6f, 0.0f),
                                            glm::vec3(0.0f, 1.6f, -1.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 lightDir(0.0f, -1.0f, 0.0f);
    const auto splits = computeCsmSplits(0.1f, 100.0f, 4, 0.5f);

    // Inversa del lightProj nos da el "tamano" del frustum ortografico:
    // si extraemos el bounding box NDC y lo transformamos, el area
    // (height * width) es proporcional al radio. Mejor: leer directamente
    // del [0][0] del proj ortografico (= 2/width).
    const auto smNear = computeCascadeShadowMatrices(camView, camProj,
                                                       splits[0], splits[1],
                                                       lightDir, 2048);
    const auto smFar  = computeCascadeShadowMatrices(camView, camProj,
                                                       splits[3], splits[4],
                                                       lightDir, 2048);
    const f32 widthNear = 2.0f / smNear.proj[0][0];
    const f32 widthFar  = 2.0f / smFar.proj[0][0];
    // La cascada lejos debe ser MUCHO mas grande.
    CHECK(widthFar > widthNear * 3.0f);
}

TEST_CASE("computeCascadeShadowMatrices: el centro del sub-frustum cae dentro del ortho") {
    const glm::mat4 camProj = glm::perspective(glm::radians(60.0f), 1.0f,
                                                  0.1f, 100.0f);
    const glm::mat4 camView = glm::lookAt(glm::vec3(0.0f, 5.0f, 10.0f),
                                            glm::vec3(0.0f, 0.0f, 0.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f));
    const auto splits = computeCsmSplits(0.1f, 100.0f, 4, 0.5f);
    const glm::vec3 lightDir(-0.3f, -1.0f, -0.4f);

    for (u32 i = 0; i < 4; ++i) {
        const auto sm = computeCascadeShadowMatrices(camView, camProj,
                                                       splits[i], splits[i + 1],
                                                       lightDir, 2048);
        // Un punto centro del sub-frustum: interpolar entre near plane
        // de camara y far plane de camara segun la cascada.
        const f32 z = -(splits[i] + splits[i + 1]) * 0.5f;
        const glm::vec4 viewCenter(0.0f, 0.0f, z, 1.0f);
        const glm::mat4 invView = glm::inverse(camView);
        const glm::vec3 worldCenter = glm::vec3(invView * viewCenter);

        const glm::vec4 ls = sm.lightSpace * glm::vec4(worldCenter, 1.0f);
        REQUIRE(ls.w > 0.0f);
        const glm::vec3 ndc = glm::vec3(ls) / ls.w;
        CHECK(std::abs(ndc.x) < 1.0f);
        CHECK(std::abs(ndc.y) < 1.0f);
        CHECK(std::abs(ndc.z) < 1.0f);
    }
}
