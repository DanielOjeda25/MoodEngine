// F3H31: tests del CPU helper Hosek-Wilkie + sun direction.
//
// El shader GLSL no se puede testear directo sin un contexto GL — los
// tests se enfocan en el helper CPU (compute_coefficients) y en
// sunDirectionFromTimeOfDay.

#include <doctest/doctest.h>

#include "engine/render/sky/HosekWilkie.h"

#include <cmath>

using Mood::Sky::HosekCoefficients;
using Mood::Sky::computeCoefficients;
using Mood::Sky::sunDirectionFromTimeOfDay;

namespace {

constexpr float k_eps = 1e-5f;

bool isFiniteVec3(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool allFinite(const HosekCoefficients& k) {
    return isFiniteVec3(k.A) && isFiniteVec3(k.B) && isFiniteVec3(k.C)
         && isFiniteVec3(k.D) && isFiniteVec3(k.E) && isFiniteVec3(k.F)
         && isFiniteVec3(k.G) && isFiniteVec3(k.H) && isFiniteVec3(k.I)
         && isFiniteVec3(k.Z);
}

} // namespace

TEST_CASE("Hosek-Wilkie: sun en zenit (mediodia clear sky) produce coefs finitos") {
    const glm::vec3 sunZenith(0.0f, 1.0f, 0.0f);
    const float turbidity = 2.5f;
    const glm::vec3 groundAlbedo(0.3f);

    const auto k = computeCoefficients(sunZenith, turbidity, groundAlbedo);

    CHECK(allFinite(k));
    // Z (radiance scale) en zenit con clear sky: valores positivos finitos.
    CHECK(k.Z.r > 0.0f);
    CHECK(k.Z.g > 0.0f);
    CHECK(k.Z.b > 0.0f);
}

TEST_CASE("Hosek-Wilkie: sun horizonte (sunset) finito + positivo") {
    const glm::vec3 sunHorizon = glm::normalize(glm::vec3(1.0f, 0.05f, 0.0f));
    const float turbidity = 4.0f;
    const glm::vec3 groundAlbedo(0.3f);

    const auto k = computeCoefficients(sunHorizon, turbidity, groundAlbedo);

    CHECK(allFinite(k));
    CHECK(k.Z.r > 0.0f);
    CHECK(k.Z.g > 0.0f);
    CHECK(k.Z.b > 0.0f);
}

TEST_CASE("Hosek-Wilkie: sun bajo horizonte (noche) -> radiance cero") {
    const glm::vec3 sunNight(0.0f, -0.5f, 0.0f);
    const float turbidity = 2.5f;
    const glm::vec3 groundAlbedo(0.3f);

    const auto k = computeCoefficients(sunNight, turbidity, groundAlbedo);

    CHECK(allFinite(k));
    // Y<-0.1 fade aplica: radiance cae a ~0 en los 3 canales.
    CHECK(k.Z.r == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(k.Z.g == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(k.Z.b == doctest::Approx(0.0f).epsilon(1e-3));
}

TEST_CASE("Hosek-Wilkie: turbidity high no rompe el modelo") {
    const glm::vec3 sunDir(0.0f, 0.8f, 0.2f);
    const glm::vec3 ground(0.3f);

    const auto clear = computeCoefficients(sunDir, 1.5f, ground);
    const auto hazy  = computeCoefficients(sunDir, 8.0f, ground);

    CHECK(allFinite(clear));
    CHECK(allFinite(hazy));
    CHECK(hazy.Z.r > 0.0f);
    CHECK(hazy.Z.g > 0.0f);
    CHECK(hazy.Z.b > 0.0f);
}

TEST_CASE("sunDirectionFromTimeOfDay: keypoints conocidos") {
    // Mediodia: zenit (Y=1).
    const auto noon = sunDirectionFromTimeOfDay(12.0f);
    CHECK(noon.x == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(noon.y == doctest::Approx(1.0f).epsilon(k_eps));
    CHECK(noon.z == doctest::Approx(0.0f).epsilon(k_eps));

    // Amanecer 06:00: horizonte este (X+, Y=0).
    const auto dawn = sunDirectionFromTimeOfDay(6.0f);
    CHECK(dawn.x == doctest::Approx(1.0f).epsilon(k_eps));
    CHECK(dawn.y == doctest::Approx(0.0f).epsilon(k_eps));

    // Atardecer 18:00: horizonte oeste (X-, Y=0).
    const auto dusk = sunDirectionFromTimeOfDay(18.0f);
    CHECK(dusk.x == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(dusk.y == doctest::Approx(0.0f).epsilon(k_eps));

    // Medianoche 00:00: nadir (Y=-1).
    const auto midnight = sunDirectionFromTimeOfDay(0.0f);
    CHECK(midnight.y == doctest::Approx(-1.0f).epsilon(k_eps));
}

TEST_CASE("sunDirectionFromTimeOfDay: continuidad alrededor del dia") {
    const auto t1 = sunDirectionFromTimeOfDay(11.99f);
    const auto t2 = sunDirectionFromTimeOfDay(12.01f);
    const float delta = glm::length(t2 - t1);
    CHECK(delta < 0.01f);
}
