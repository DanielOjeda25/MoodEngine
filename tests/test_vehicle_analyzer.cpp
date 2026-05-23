// F2H82 Bloque A: tests del parser de nombres + clasificación por posición del
// VehicleMeshAnalyzer (funciones puras, headless — sin assimp ni archivo).
// Cubre la convención de docs/conventions/vehiculos.md.

#include <doctest/doctest.h>

#include "engine/physics/vehicle/VehicleMeshAnalyzer.h"

using namespace Mood::vehicle;

TEST_CASE("VehicleMeshAnalyzer F2H82 A: nombres canónicos wheel_FL/FR/RL/RR") {
    CHECK(wheelRoleFromName("wheel_FL") == WheelRole::FL);
    CHECK(wheelRoleFromName("wheel_FR") == WheelRole::FR);
    CHECK(wheelRoleFromName("wheel_RL") == WheelRole::RL);
    CHECK(wheelRoleFromName("wheel_RR") == WheelRole::RR);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: alias Rockstar (lf/rf/lr/rr)") {
    CHECK(wheelRoleFromName("wheel_lf") == WheelRole::FL);
    CHECK(wheelRoleFromName("wheel_rf") == WheelRole::FR);
    CHECK(wheelRoleFromName("wheel_lr") == WheelRole::RL);
    CHECK(wheelRoleFromName("wheel_rr") == WheelRole::RR);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: alias Unreal (BL/BR = rear)") {
    CHECK(wheelRoleFromName("Wheel_BL") == WheelRole::RL);
    CHECK(wheelRoleFromName("Wheel_BR") == WheelRole::RR);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: español (tesla RUEDRA_*)") {
    CHECK(wheelRoleFromName("RUEDRA_DELANTERA_IZQUIERDA") == WheelRole::FL);
    CHECK(wheelRoleFromName("RUEDRA_DELANTERA_DERECHA") == WheelRole::FR);
    CHECK(wheelRoleFromName("RUEDRA_TRASERA_IZQUIERDA") == WheelRole::RL);
    CHECK(wheelRoleFromName("RUEDRA_TRASERA_DERECHA") == WheelRole::RR);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: abreviatura tire (armor-car f_t_l/b_t_r)") {
    CHECK(wheelRoleFromName("f_t_l") == WheelRole::FL);
    CHECK(wheelRoleFromName("f_t_r") == WheelRole::FR);
    CHECK(wheelRoleFromName("b_t_l") == WheelRole::RL);
    CHECK(wheelRoleFromName("b_t_r") == WheelRole::RR);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: palabras completas + separadores varios") {
    CHECK(wheelRoleFromName("front_left_wheel") == WheelRole::FL);
    CHECK(wheelRoleFromName("rear-right-wheel") == WheelRole::RR);
    CHECK(wheelRoleFromName("Wheel.Front.Right") == WheelRole::FR);
    CHECK(wheelRoleFromName("back left tire") == WheelRole::RL);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: no-ruedas devuelven Unknown") {
    CHECK(wheelRoleFromName("chassis") == WheelRole::Unknown);
    CHECK(wheelRoleFromName("body") == WheelRole::Unknown);
    CHECK(wheelRoleFromName("Cube.003") == WheelRole::Unknown);
    CHECK(wheelRoleFromName("armoured_car_body") == WheelRole::Unknown);
    // "tier" != "tire" — no debe confundirse.
    CHECK(wheelRoleFromName("armoured_car_tier") == WheelRole::Unknown);
    // "wheel" sin dirección no se puede ubicar.
    CHECK(wheelRoleFromName("wheel") == WheelRole::Unknown);
}

TEST_CASE("VehicleMeshAnalyzer F2H82 A: clasificación por posición (+Z forward)") {
    const glm::vec3 center{0.0f, 0.0f, 0.0f};
    // Frente = +Z, izquierda = -X.
    CHECK(wheelRoleFromPosition({-1.0f, 0.0f,  2.0f}, center) == WheelRole::FL);
    CHECK(wheelRoleFromPosition({ 1.0f, 0.0f,  2.0f}, center) == WheelRole::FR);
    CHECK(wheelRoleFromPosition({-1.0f, 0.0f, -2.0f}, center) == WheelRole::RL);
    CHECK(wheelRoleFromPosition({ 1.0f, 0.0f, -2.0f}, center) == WheelRole::RR);
}
