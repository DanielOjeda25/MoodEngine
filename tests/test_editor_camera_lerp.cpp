// F3H21: tests headless del lerp animator de EditorCamera (numpad smooth view).
// Cubre setPose / beginLerpTo / tick — no toca GL ni ImGui. El driver real
// vive en el caller del editor (EditorApplication tick).

#include <doctest/doctest.h>

#include "engine/scene/core/EditorCamera.h"

#include <glm/geometric.hpp>

using namespace Mood;

TEST_CASE("F3H21 EditorCamera: setPose mueve los 4 fields directo") {
    EditorCamera cam;
    cam.setPose(90.0f, 30.0f, 20.0f, glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(cam.yawDeg()   == doctest::Approx(90.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(30.0f));
    CHECK(cam.radius()   == doctest::Approx(20.0f));
    CHECK(cam.target().x == doctest::Approx(1.0f));
    CHECK(cam.target().y == doctest::Approx(2.0f));
    CHECK(cam.target().z == doctest::Approx(3.0f));
    CHECK_FALSE(cam.isLerping());
}

TEST_CASE("F3H21 EditorCamera: setPose clampea pitch a [-89, 89]") {
    EditorCamera cam;
    cam.setPose(0.0f, 120.0f, 10.0f, glm::vec3(0.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(89.0f));
    cam.setPose(0.0f, -120.0f, 10.0f, glm::vec3(0.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(-89.0f));
}

TEST_CASE("F3H21 EditorCamera: beginLerpTo con duration=0 es teleport") {
    EditorCamera cam(45.0f, 30.0f, 30.0f);
    cam.beginLerpTo(0.0f, 0.0f, 10.0f, glm::vec3(5.0f, 0.0f, 0.0f), 0);
    CHECK_FALSE(cam.isLerping());
    CHECK(cam.yawDeg() == doctest::Approx(0.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(0.0f));
    CHECK(cam.radius() == doctest::Approx(10.0f));
    CHECK(cam.target().x == doctest::Approx(5.0f));
}

TEST_CASE("F3H21 EditorCamera: beginLerpTo activa isLerping con duration>0") {
    EditorCamera cam(45.0f, 30.0f, 30.0f);
    cam.beginLerpTo(0.0f, 0.0f, 10.0f, glm::vec3(0.0f), 200);
    CHECK(cam.isLerping());
    // pose actual no cambio todavia (t=0)
    CHECK(cam.yawDeg() == doctest::Approx(45.0f));
}

TEST_CASE("F3H21 EditorCamera: tick avanza el lerp hasta llegar") {
    EditorCamera cam(0.0f, 0.0f, 20.0f);
    cam.beginLerpTo(90.0f, 30.0f, 10.0f, glm::vec3(5.0f), 200);

    // En la mitad del lerp (100ms), smoothstep(0.5) = 0.5 — debe estar a la mitad
    cam.tick(0.1f);  // dt = 0.1s
    CHECK(cam.isLerping());
    CHECK(cam.yawDeg() == doctest::Approx(45.0f).epsilon(0.01));
    CHECK(cam.pitchDeg() == doctest::Approx(15.0f).epsilon(0.01));
    CHECK(cam.radius() == doctest::Approx(15.0f).epsilon(0.01));

    // Completar el lerp (110ms restante > 100ms remaining)
    cam.tick(0.11f);
    CHECK_FALSE(cam.isLerping());
    // Pose final exacta (sin error de easing)
    CHECK(cam.yawDeg() == doctest::Approx(90.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(30.0f));
    CHECK(cam.radius() == doctest::Approx(10.0f));
    CHECK(cam.target().x == doctest::Approx(5.0f));
}

TEST_CASE("F3H21 EditorCamera: tick es no-op si no hay lerp activo") {
    EditorCamera cam(45.0f, 30.0f, 30.0f);
    cam.tick(0.1f);  // sin beginLerpTo previo
    CHECK(cam.yawDeg() == doctest::Approx(45.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(30.0f));
    CHECK(cam.radius() == doctest::Approx(30.0f));
}

TEST_CASE("F3H21 EditorCamera: lerp yaw shortest-path 350->10 grados") {
    // Sin shortest-path, ir de 350 a 10 daria delta=-340 (camino largo).
    // Con shortest-path, delta=+20 (camino corto via 0).
    EditorCamera cam(350.0f, 0.0f, 10.0f);
    cam.beginLerpTo(10.0f, 0.0f, 10.0f, glm::vec3(0.0f), 200);

    cam.tick(0.1f);  // mitad: smoothstep(0.5)=0.5
    // Yaw deberia estar cerca de 360 (== 0). En representacion lineal:
    // start=350, end ajustado a 350+20=370. Mitad = 360.
    CHECK(cam.yawDeg() == doctest::Approx(360.0f).epsilon(0.01));
}

TEST_CASE("F3H21 EditorCamera: setPose cancela un lerp en curso") {
    EditorCamera cam(0.0f, 0.0f, 20.0f);
    cam.beginLerpTo(90.0f, 30.0f, 10.0f, glm::vec3(0.0f), 500);
    CHECK(cam.isLerping());

    cam.setPose(180.0f, 45.0f, 25.0f, glm::vec3(0.0f));
    CHECK_FALSE(cam.isLerping());
    CHECK(cam.yawDeg() == doctest::Approx(180.0f));
}

TEST_CASE("F3H21 EditorCamera: tick con dt mayor que remaining termina exacto") {
    EditorCamera cam(0.0f, 0.0f, 10.0f);
    cam.beginLerpTo(45.0f, 20.0f, 15.0f, glm::vec3(0.0f), 100);

    // dt=1s con solo 100ms restantes: debe completar y settear el end exacto.
    cam.tick(1.0f);
    CHECK_FALSE(cam.isLerping());
    CHECK(cam.yawDeg() == doctest::Approx(45.0f));
    CHECK(cam.pitchDeg() == doctest::Approx(20.0f));
    CHECK(cam.radius() == doctest::Approx(15.0f));
}
