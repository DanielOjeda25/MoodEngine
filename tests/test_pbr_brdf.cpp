// Tests numericos de los helpers PBR del Hito 17 (Bloque 5). El shader
// `pbr.frag` y `engine/render/PbrMath.h` comparten formula (Cook-Torrance
// con GGX/Smith/Schlick); estos tests verifican que la version C++
// devuelve valores conocidos para que un cambio futuro al shader que
// no replique aca aparezca como CI red.

#include <doctest/doctest.h>

#include "engine/render/PbrMath.h"

#include <glm/geometric.hpp>

using namespace Mood;

TEST_CASE("fresnelF0: dieléctrico estandar 0.04, metálico = albedo") {
    const glm::vec3 albedo(0.95f, 0.93f, 0.88f); // oro

    // Plástico (metallic=0): F0 fijo en 0.04 sin importar el albedo.
    const glm::vec3 dielectric = fresnelF0(albedo, 0.0f);
    CHECK(dielectric.x == doctest::Approx(0.04f));
    CHECK(dielectric.y == doctest::Approx(0.04f));
    CHECK(dielectric.z == doctest::Approx(0.04f));

    // Metal puro (metallic=1): F0 = albedo (los metales toman su color
    // del especular).
    const glm::vec3 metal = fresnelF0(albedo, 1.0f);
    CHECK(metal.x == doctest::Approx(0.95f));
    CHECK(metal.y == doctest::Approx(0.93f));
    CHECK(metal.z == doctest::Approx(0.88f));

    // Mid-metallic = lerp lineal.
    const glm::vec3 mid = fresnelF0(albedo, 0.5f);
    CHECK(mid.x == doctest::Approx(0.5f * 0.04f + 0.5f * 0.95f));
    CHECK(mid.y == doctest::Approx(0.5f * 0.04f + 0.5f * 0.93f));
}

TEST_CASE("fresnelSchlick: cosTheta=1 devuelve F0 (sin Fresnel)") {
    // A incidencia normal (dot(H, V) = 1) el termino (1-cos)^5 = 0,
    // asi que F = F0 exacto.
    const glm::vec3 F0(0.04f, 0.04f, 0.04f);
    const glm::vec3 F = fresnelSchlick(1.0f, F0);
    CHECK(F.x == doctest::Approx(0.04f));
    CHECK(F.y == doctest::Approx(0.04f));
    CHECK(F.z == doctest::Approx(0.04f));
}

TEST_CASE("fresnelSchlick: cosTheta=0 devuelve 1.0 (grazing -> reflejo total)") {
    // Angulo de pastoreo (90 deg respecto de la normal): el especular
    // sube a 1 sin importar F0. Es el efecto Fresnel clásico (todo el
    // mundo se ve espejado al ras del agua).
    const glm::vec3 F0(0.04f, 0.04f, 0.04f);
    const glm::vec3 F = fresnelSchlick(0.0f, F0);
    CHECK(F.x == doctest::Approx(1.0f));
    CHECK(F.y == doctest::Approx(1.0f));
    CHECK(F.z == doctest::Approx(1.0f));
}

TEST_CASE("fresnelSchlick: monotono creciente entre F0 y 1") {
    // Asegura que la curva es estrictamente creciente al alejarse de
    // la incidencia normal (no hay accidente de polinomio que la
    // doble).
    const glm::vec3 F0(0.04f, 0.04f, 0.04f);
    const f32 prev = fresnelSchlick(0.9f, F0).x;
    const f32 mid  = fresnelSchlick(0.5f, F0).x;
    const f32 far  = fresnelSchlick(0.1f, F0).x;
    CHECK(mid > prev);
    CHECK(far > mid);
    CHECK(far < 1.0f);
}

TEST_CASE("distributionGGX: pico cuando H = N (cos = 1)") {
    // Con H alineado con la normal NdotH=1, asi que denom = (a^2-1)+1
    // = a^2. La distribucion alcanza su maximo: D = a^2 / (PI * a^4)
    // = 1 / (PI * a^2). Para r=0.5, a=0.25, a^2=0.0625, D ~= 5.093.
    const glm::vec3 N(0.0f, 1.0f, 0.0f);
    const glm::vec3 H = N; // alineado
    const f32 r = 0.5f;
    const f32 a  = r * r;
    const f32 a2 = a * a;
    constexpr f32 PI = 3.14159265358979f;
    const f32 expected = 1.0f / (PI * a2);
    CHECK(distributionGGX(N, H, r) == doctest::Approx(expected));
}

TEST_CASE("distributionGGX: roughness baja concentra el lobulo") {
    // Comparar D para H lejos de N entre roughness baja y alta:
    // roughness baja debe dar D mucho mas chico (lobulo concentrado),
    // roughness alta da D mas alto (lobulo amplio).
    const glm::vec3 N(0.0f, 1.0f, 0.0f);
    // H a ~30 grados de N (vector unitario en xy plane).
    const glm::vec3 H = glm::normalize(glm::vec3(0.5f, 0.866f, 0.0f));
    const f32 dRough = distributionGGX(N, H, 1.0f);
    const f32 dSharp = distributionGGX(N, H, 0.1f);
    CHECK(dRough > dSharp);
}

TEST_CASE("geometrySmith: cae a 0 cuando NdotL=0 (luz rasante perpendicular)") {
    // L perpendicular a N => NdotL=0 => g_L = 0 => producto = 0.
    const glm::vec3 N(0.0f, 1.0f, 0.0f);
    const glm::vec3 V(0.0f, 1.0f, 0.0f);
    const glm::vec3 L(1.0f, 0.0f, 0.0f);
    const f32 g = geometrySmith(N, V, L, 0.5f);
    CHECK(g == doctest::Approx(0.0f));
}

TEST_CASE("geometrySmith: en [0, 1]") {
    // Para cualquier configuracion valida G es un coeficiente en [0,1].
    const glm::vec3 N(0.0f, 1.0f, 0.0f);
    const glm::vec3 V = glm::normalize(glm::vec3(0.3f, 0.9f, 0.0f));
    const glm::vec3 L = glm::normalize(glm::vec3(-0.4f, 0.7f, 0.2f));
    const f32 roughs[] = {0.1f, 0.5f, 0.9f};
    for (f32 r : roughs) {
        const f32 g = geometrySmith(N, V, L, r);
        CHECK(g >= 0.0f);
        CHECK(g <= 1.0f);
    }
}
