// Tests numericos del fog (Hito 15 Bloque 2). El header `Fog.h` replica la
// matematica del shader `lit.frag`. Validamos que los modos coincidan con
// la formula declarada y que los limites (start, end, density) se
// comporten como esperamos.

#include <doctest/doctest.h>

#include "engine/render/Fog.h"

using namespace Mood;

TEST_CASE("Fog Off devuelve siempre factor 0") {
    FogParams p{};
    p.mode = FogMode::Off;
    CHECK(computeFogFactor(p, 0.0f)   == doctest::Approx(0.0f));
    CHECK(computeFogFactor(p, 100.0f) == doctest::Approx(0.0f));
}

TEST_CASE("Fog Linear: 0 antes de start, 1 despues de end, lineal en medio") {
    FogParams p{};
    p.mode = FogMode::Linear;
    p.linearStart = 10.0f;
    p.linearEnd   = 30.0f;

    CHECK(computeFogFactor(p,  0.0f) == doctest::Approx(0.0f));
    CHECK(computeFogFactor(p, 10.0f) == doctest::Approx(0.0f));
    CHECK(computeFogFactor(p, 20.0f) == doctest::Approx(0.5f));
    CHECK(computeFogFactor(p, 30.0f) == doctest::Approx(1.0f));
    CHECK(computeFogFactor(p, 50.0f) == doctest::Approx(1.0f));
}

TEST_CASE("Fog Linear: end == start devuelve 1 (configuracion degenerada)") {
    FogParams p{};
    p.mode = FogMode::Linear;
    p.linearStart = 5.0f;
    p.linearEnd   = 5.0f;
    CHECK(computeFogFactor(p, 4.0f) == doctest::Approx(1.0f));
    CHECK(computeFogFactor(p, 6.0f) == doctest::Approx(1.0f));
}

TEST_CASE("Fog Exp: 0 en distancia 0, monotono creciente, asintota a 1") {
    FogParams p{};
    p.mode = FogMode::Exp;
    p.density = 0.05f;

    CHECK(computeFogFactor(p, 0.0f)   == doctest::Approx(0.0f));
    const f32 mid = computeFogFactor(p, 30.0f);
    const f32 far = computeFogFactor(p, 100.0f);
    CHECK(mid > 0.0f);
    CHECK(mid < 1.0f);
    CHECK(far > mid);
    CHECK(far < 1.0f);
    // En distancia "muy grande" se acerca a 1.
    CHECK(computeFogFactor(p, 1000.0f) > 0.99f);
}

TEST_CASE("Fog Exp2: crece mas suave cerca y mas rapido lejos que Exp") {
    FogParams p{};
    p.density = 0.05f;
    p.mode = FogMode::Exp;
    const f32 expFar = computeFogFactor(p, 50.0f);
    p.mode = FogMode::Exp2;
    const f32 exp2Far = computeFogFactor(p, 50.0f);
    // Exp2 con density 0.05 a 50 m: 1 - exp(-(2.5)^2) ~= 0.998.
    // Exp con la misma density y distancia: 1 - exp(-2.5) ~= 0.918.
    // Asi que en este punto Exp2 esta mas alto.
    CHECK(exp2Far > expFar);
}

TEST_CASE("applyFog: mezcla lit con fog color segun el factor") {
    FogParams p{};
    p.mode = FogMode::Linear;
    p.color = glm::vec3(1.0f, 0.0f, 0.0f); // rojo puro
    p.linearStart = 0.0f;
    p.linearEnd = 10.0f;

    const glm::vec3 lit(0.0f, 1.0f, 0.0f); // verde puro

    // En distancia 0: factor 0, queda el lit.
    glm::vec3 r = applyFog(lit, p, 0.0f);
    CHECK(r.x == doctest::Approx(0.0f));
    CHECK(r.y == doctest::Approx(1.0f));
    CHECK(r.z == doctest::Approx(0.0f));

    // En distancia 10: factor 1, queda el fog color.
    r = applyFog(lit, p, 10.0f);
    CHECK(r.x == doctest::Approx(1.0f));
    CHECK(r.y == doctest::Approx(0.0f));

    // En distancia 5: factor 0.5, mezcla 50/50.
    r = applyFog(lit, p, 5.0f);
    CHECK(r.x == doctest::Approx(0.5f));
    CHECK(r.y == doctest::Approx(0.5f));
}
