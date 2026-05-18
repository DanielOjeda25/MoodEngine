// Tests headless de core/math/Ray.h (AUDIT-3 Bloque C). Verifica el
// unprojection NDC->world tanto para perspective como para ortho, y los
// edge cases que el helper protege (clip.w=0, dir cero).

#include <doctest/doctest.h>

#include "core/math/Ray.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace Mood;

namespace {

/// View matrix sencilla: camara en (0, 0, 5) mirando -Z, up = +Y.
glm::mat4 viewLookingDownNegZ() {
    return glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                       glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 invVP(const glm::mat4& proj, const glm::mat4& view) {
    return glm::inverse(proj * view);
}

} // namespace

TEST_CASE("Ray: pickRayFromNdc en centro NDC con perspective dispara hacia -Z") {
    const auto proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const auto view = viewLookingDownNegZ();
    const auto r = pickRayFromNdc(invVP(proj, view), 0.0f, 0.0f);
    REQUIRE(r.has_value());

    // Centro NDC -> origin debe estar al borde del near plane sobre el eje
    // optico (x=y=0). Z queda en (5 - 0.1) = 4.9 (camera_z - near).
    CHECK(r->origin.x == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(r->origin.y == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(r->origin.z == doctest::Approx(4.9f).epsilon(1e-2));

    // Direction normalizada apuntando a -Z.
    CHECK(r->direction.x == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(r->direction.y == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(r->direction.z == doctest::Approx(-1.0f).epsilon(1e-3));
}

TEST_CASE("Ray: pickRayFromNdc en esquina superior-derecha apunta arriba-derecha") {
    const auto proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    const auto view = viewLookingDownNegZ();
    // (ndc.x = +1, ndc.y = +1) — esquina superior derecha del viewport.
    const auto r = pickRayFromNdc(invVP(proj, view), 1.0f, 1.0f);
    REQUIRE(r.has_value());
    // Con FOV 90 + aspect 1, el rayo en la esquina tiene componentes x,y
    // positivas y z negativa. Solo verificamos signos + normalizacion.
    CHECK(r->direction.x > 0.0f);
    CHECK(r->direction.y > 0.0f);
    CHECK(r->direction.z < 0.0f);
    CHECK(glm::length(r->direction) == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("Ray: pickRayFromNdc bajo ortho preserva direccion paralela") {
    // Ortho con frustum [-1,1]^2 y near/far estandar. La direccion del rayo
    // es la misma para todo punto NDC: -Z.
    const auto proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    const auto view = viewLookingDownNegZ();
    const auto invvp = invVP(proj, view);

    const auto rCenter = pickRayFromNdc(invvp, 0.0f, 0.0f);
    const auto rCorner = pickRayFromNdc(invvp, 0.5f, 0.5f);
    REQUIRE(rCenter.has_value());
    REQUIRE(rCorner.has_value());

    // Ambas direcciones deben ser iguales (paralelas en ortho).
    CHECK(rCenter->direction.x == doctest::Approx(rCorner->direction.x).epsilon(1e-4));
    CHECK(rCenter->direction.y == doctest::Approx(rCorner->direction.y).epsilon(1e-4));
    CHECK(rCenter->direction.z == doctest::Approx(rCorner->direction.z).epsilon(1e-4));
    CHECK(rCenter->direction.z == doctest::Approx(-1.0f).epsilon(1e-4));

    // Origenes diferentes (los rayos no comparten origin en ortho).
    CHECK(rCenter->origin.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(rCorner->origin.x == doctest::Approx(0.5f).epsilon(1e-4));
    CHECK(rCorner->origin.y == doctest::Approx(0.5f).epsilon(1e-4));
}

TEST_CASE("Ray: unprojectNearFar devuelve near/far sin normalizar") {
    const auto proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const auto view = viewLookingDownNegZ();
    const auto nf = unprojectNearFar(invVP(proj, view), 0.0f, 0.0f);
    REQUIRE(nf.has_value());

    // near point esta en z = camera_z - near = 4.9; far en z = camera_z - far = -95.
    CHECK(nf->nearW.z == doctest::Approx(4.9f).epsilon(1e-2));
    CHECK(nf->farW.z  == doctest::Approx(-95.0f).epsilon(1e-1));
    // x, y centradas en eje optico.
    CHECK(nf->nearW.x == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(nf->farW.x  == doctest::Approx(0.0f).epsilon(1e-3));

    // dir = far - near apunta a -Z (no normalizada -> magnitud ~ 99.9).
    const glm::vec3 dir = nf->farW - nf->nearW;
    CHECK(dir.z < 0.0f);
    CHECK(glm::length(dir) > 50.0f);
}

TEST_CASE("Ray: pickRayFromNdc devuelve nullopt para invVP con W cero") {
    // Construimos una invVP degenerada: la fila W es cero -> nearH.w = 0.
    glm::mat4 invVP_bad(1.0f);
    invVP_bad[0][3] = 0.0f;
    invVP_bad[1][3] = 0.0f;
    invVP_bad[2][3] = 0.0f;
    invVP_bad[3][3] = 0.0f; // homogeneous w siempre 0

    const auto r = pickRayFromNdc(invVP_bad, 0.0f, 0.0f);
    CHECK_FALSE(r.has_value());

    const auto nf = unprojectNearFar(invVP_bad, 0.0f, 0.0f);
    CHECK_FALSE(nf.has_value());
}

TEST_CASE("Ray: pickRayFromNdc preserva longitud unitaria para NDCs arbitrarios") {
    const auto proj = glm::perspective(glm::radians(75.0f), 16.0f / 9.0f, 0.5f, 200.0f);
    const auto view = glm::lookAt(glm::vec3(3.0f, 4.0f, 5.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    const auto invvp = invVP(proj, view);

    const float samples[][2] = {
        {0.0f, 0.0f}, {-1.0f, -1.0f}, {1.0f, 1.0f},
        {0.3f, -0.7f}, {-0.9f, 0.2f},
    };
    for (auto s : samples) {
        const auto r = pickRayFromNdc(invvp, s[0], s[1]);
        REQUIRE(r.has_value());
        CHECK(glm::length(r->direction) == doctest::Approx(1.0f).epsilon(1e-4));
    }
}
