// Tests minimos de las camaras. No cubren todos los casos: solo fijan
// invariantes claros para que un futuro refactor no los rompa en silencio.

#include <doctest/doctest.h>

#include "engine/scene/EditorCamera.h"
#include "engine/scene/FpsCamera.h"

#include <glm/geometric.hpp>

#include <cmath>

using namespace Mood;

TEST_CASE("EditorCamera default: radio y yaw definen posicion esperada") {
    EditorCamera cam(/*yawDeg=*/0.0f, /*pitchDeg=*/0.0f, /*radius=*/5.0f);
    // Con yaw=0, pitch=0 y target en el origen, position = (0, 0, +5).
    const glm::vec3 p = cam.position();
    CHECK(p.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(p.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(p.z == doctest::Approx(5.0f).epsilon(1e-4));
}

TEST_CASE("EditorCamera: wheel zoom positivo acerca la camara") {
    EditorCamera cam(0.0f, 0.0f, 5.0f);
    const float r0 = glm::length(cam.position());
    cam.applyWheel(+1.0f);
    const float r1 = glm::length(cam.position());
    CHECK(r1 < r0);
}

TEST_CASE("EditorCamera: pitch se clampea a +-89 grados") {
    EditorCamera cam(0.0f, 0.0f, 5.0f);
    // Arrastre enorme para forzar el clamp. applyMouseDrag suma dy * k_rotateSpeed.
    cam.applyMouseDrag(0.0f, 100000.0f);
    // En pitch=+89 la Y no puede exceder el radio; sin clamp explotaria por NaN
    // al calcular lookAt. Test indirecto: viewMatrix no tiene NaNs.
    const glm::mat4 v = cam.viewMatrix();
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            CHECK_FALSE(std::isnan(v[c][r]));
        }
    }
}

TEST_CASE("FpsCamera default: forward apunta hacia -Z") {
    FpsCamera cam; // yaw=-90, pitch=0
    const glm::vec3 f = cam.forward();
    CHECK(f.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(f.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(f.z == doctest::Approx(-1.0f).epsilon(1e-4));
}

TEST_CASE("FpsCamera: W (dir.z=+1) traslada hacia adelante sobre XZ") {
    FpsCamera cam({0.0f, 0.0f, 0.0f}, /*yaw=*/-90.0f, /*pitch=*/0.0f);
    const glm::vec3 p0 = cam.position();
    cam.move(glm::vec3(0.0f, 0.0f, 1.0f), /*dt=*/1.0f);
    const glm::vec3 p1 = cam.position();
    // Tras 1 segundo con speed = 3, la distancia recorrida debe ser 3.
    CHECK(glm::length(p1 - p0) == doctest::Approx(3.0f).epsilon(1e-3));
    // Y la direccion debe ser la de forward (hacia -Z).
    CHECK(p1.z < p0.z);
    CHECK(p1.y == doctest::Approx(0.0f).epsilon(1e-4));
}

TEST_CASE("FpsCamera: combinar vectores ortogonales no rompe la velocidad") {
    FpsCamera cam({0.0f, 0.0f, 0.0f}, -90.0f, 0.0f);
    // Adelante + derecha a la vez: deberia normalizarse antes de escalar por
    // speed*dt, es decir, no ir mas rapido que recorrido en linea recta.
    cam.move(glm::vec3(1.0f, 0.0f, 1.0f), 1.0f);
    CHECK(glm::length(cam.position()) <= doctest::Approx(3.0f).epsilon(1e-3));
}
