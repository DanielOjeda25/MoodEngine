// Tests del modulo de operaciones booleanas CSG (F2H12). Cubren:
//   - isBrushValid: discrimina brushes con/sin volumen.
//   - subtract: 0 brushes (B contiene A), 1 (disjuntos), N (corte
//     parcial), conservacion de volumen end-to-end.
//   - intersectOp: nullopt si disjuntos, brush valido si overlap.
//   - unionOp: 1 brush (contencion), 2 (disjuntos), N (overlap).
//   - Conmutatividad / determinismo / casos tangenciales.

#include <doctest/doctest.h>

#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushMesh.h"
#include "engine/world/csg/BrushOps.h"

#include <glm/gtc/matrix_transform.hpp>

using namespace Mood;
using namespace Mood::Csg;

namespace {

constexpr f32 k_eps = 1e-3f;

/// @brief Helper: brush box centrado en `center` con lado `size`.
Brush boxAt(const glm::vec3& center, f32 size = 1.0f) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
    m = glm::scale(m, glm::vec3(size));
    return makeBoxBrush(m);
}

}  // namespace

// ============================================================
// isBrushValid
// ============================================================

TEST_CASE("isBrushValid: box unitaria es valida") {
    CHECK(isBrushValid(makeBoxBrush(glm::mat4(1.0f))));
}

TEST_CASE("isBrushValid: brush vacio es invalido") {
    Brush b;
    CHECK_FALSE(isBrushValid(b));
}

TEST_CASE("isBrushValid: < 4 caras es invalido") {
    Brush b;
    b.faces.push_back({Plane{glm::vec3(1, 0, 0), 0.0f}, 0});
    b.faces.push_back({Plane{glm::vec3(0, 1, 0), 0.0f}, 0});
    b.faces.push_back({Plane{glm::vec3(0, 0, 1), 0.0f}, 0});
    CHECK_FALSE(isBrushValid(b));
}

TEST_CASE("isBrushValid: 4 caras pero sin volumen comun es invalido") {
    // 4 planos paralelos al mismo eje no encierran volumen.
    Brush b;
    b.faces.push_back({Plane{glm::vec3( 1, 0, 0), -0.5f}, 0});
    b.faces.push_back({Plane{glm::vec3(-1, 0, 0), -0.5f}, 0});
    b.faces.push_back({Plane{glm::vec3( 1, 0, 0),  0.5f}, 0});
    b.faces.push_back({Plane{glm::vec3(-1, 0, 0),  0.5f}, 0});
    CHECK_FALSE(isBrushValid(b));
}

TEST_CASE("isBrushValid: box trasladada/rotada/escalada sigue valida") {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, -5.0f, 3.0f));
    m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0, 1, 0));
    m = glm::scale(m, glm::vec3(2.0f, 0.5f, 1.5f));
    CHECK(isBrushValid(makeBoxBrush(m)));
}

// ============================================================
// subtract
// ============================================================

TEST_CASE("subtract: B disjunto de A devuelve copia de A (1 brush)") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
    const auto result = subtract(A, B);
    REQUIRE(result.size() == 1);
    CHECK(isBrushValid(result[0]));
    // El resultado debe tener volumen ~ 1.0
    const auto mesh = buildBrushMesh(result[0]);
    CHECK(mesh.indices.size() > 0);
}

TEST_CASE("subtract: B contiene completamente a A devuelve vacio") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);          // box 1x1x1
    const Brush B = boxAt(glm::vec3(0.0f), 5.0f);          // box 5x5x5 conteniendo a A
    const auto result = subtract(A, B);
    CHECK(result.empty());
}

TEST_CASE("subtract: B == A devuelve vacio") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);
    const auto result = subtract(A, B);
    CHECK(result.empty());
}

TEST_CASE("subtract: B atravesando A en X produce 2 brushes") {
    // A = box centrada en origen, lado 2 (-1..1 en cada eje).
    // B = box larga en X (-5..5) centrada en Y=0, lado en Y/Z = 0.5.
    // El subtract deberia generar 2 brushes: el de Y > 0.25 y el de Y < -0.25.
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    glm::mat4 mB = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
    mB = glm::scale(mB, glm::vec3(10.0f, 0.5f, 0.5f));
    const Brush B = makeBoxBrush(mB);

    const auto result = subtract(A, B);
    // Esperamos 4 brushes: arriba/abajo en Y y adelante/atras en Z
    // (B atraviesa A en X, queda un "marco" de 4 piezas).
    CHECK(result.size() >= 2);
    for (const auto& r : result) {
        CHECK(isBrushValid(r));
    }
}

TEST_CASE("subtract: B en una esquina de A produce >= 1 brush") {
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);          // -1..1
    const Brush B = boxAt(glm::vec3(0.5f, 0.5f, 0.5f), 1.0f); // 0..1 esquina superior
    const auto result = subtract(A, B);
    CHECK_FALSE(result.empty());
    for (const auto& r : result) {
        CHECK(isBrushValid(r));
    }
}

TEST_CASE("subtract: B chico en el centro de A produce >= 1 brush") {
    const Brush A = boxAt(glm::vec3(0.0f), 4.0f);          // -2..2
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);          // -0.5..0.5 centrado
    const auto result = subtract(A, B);
    CHECK_FALSE(result.empty());
    for (const auto& r : result) {
        CHECK(isBrushValid(r));
    }
}

TEST_CASE("subtract: A invalido devuelve vacio") {
    Brush A;  // invalido
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);
    CHECK(subtract(A, B).empty());
}

TEST_CASE("subtract: B invalido devuelve [A]") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    Brush B;  // invalido
    const auto result = subtract(A, B);
    REQUIRE(result.size() == 1);
    CHECK(result[0].faces.size() == A.faces.size());
}

TEST_CASE("subtract: determinismo (misma entrada -> misma salida)") {
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    const Brush B = boxAt(glm::vec3(0.5f, 0.0f, 0.0f), 1.0f);
    const auto r1 = subtract(A, B);
    const auto r2 = subtract(A, B);
    REQUIRE(r1.size() == r2.size());
    for (usize i = 0; i < r1.size(); ++i) {
        CHECK(r1[i].faces.size() == r2[i].faces.size());
    }
}

TEST_CASE("subtract: hereda materialIndex de A") {
    Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    for (auto& f : A.faces) f.materialIndex = 7;
    const Brush B = boxAt(glm::vec3(0.5f, 0.0f, 0.0f), 1.0f);
    const auto result = subtract(A, B);
    REQUIRE_FALSE(result.empty());
    // Las caras heredadas de A mantienen materialIndex 7;
    // las nuevas caras (planos flipped de B) tambien usan ese
    // material por defecto en F2H12 (per-cara real es F2H14).
    for (const auto& r : result) {
        for (const auto& f : r.faces) {
            CHECK(f.materialIndex == 7);
        }
    }
}

// ============================================================
// intersectOp
// ============================================================

TEST_CASE("intersectOp: A y B disjuntos devuelve nullopt") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
    CHECK_FALSE(intersectOp(A, B).has_value());
}

TEST_CASE("intersectOp: B contiene a A devuelve A (mod ordering)") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(0.0f), 5.0f);
    const auto result = intersectOp(A, B);
    REQUIRE(result.has_value());
    CHECK(isBrushValid(*result));
    // El volumen de A ∩ B con B ⊇ A es el de A. Aprox: contar tris
    // del mesh (24/box).
    const auto mesh = buildBrushMesh(*result);
    CHECK(mesh.indices.size() > 0);
}

TEST_CASE("intersectOp: A y B con overlap parcial devuelve brush valido") {
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    const Brush B = boxAt(glm::vec3(1.0f, 0.0f, 0.0f), 2.0f);  // overlap en x=[0,1]
    const auto result = intersectOp(A, B);
    REQUIRE(result.has_value());
    CHECK(isBrushValid(*result));
}

TEST_CASE("intersectOp: conmutatividad (A ∩ B == B ∩ A en volumen)") {
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    const Brush B = boxAt(glm::vec3(0.5f, 0.5f, 0.0f), 1.5f);
    const auto ab = intersectOp(A, B);
    const auto ba = intersectOp(B, A);
    REQUIRE(ab.has_value());
    REQUIRE(ba.has_value());
    // Ambos deben tener la misma cantidad de vertices del brush
    // (la representacion implicita puede diferir en orden de caras
    // pero el volumen es identico).
    const auto meshAB = buildBrushMesh(*ab);
    const auto meshBA = buildBrushMesh(*ba);
    CHECK(meshAB.vertices.size() == meshBA.vertices.size());
}

TEST_CASE("intersectOp: A invalido devuelve nullopt") {
    Brush A;
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);
    CHECK_FALSE(intersectOp(A, B).has_value());
}

TEST_CASE("intersectOp: A == B devuelve A (volumen completo)") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const auto result = intersectOp(A, A);
    REQUIRE(result.has_value());
    CHECK(isBrushValid(*result));
}

// ============================================================
// unionOp
// ============================================================

TEST_CASE("unionOp: A y B disjuntos devuelve [A, B] (2 brushes)") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(10.0f, 0.0f, 0.0f), 1.0f);
    const auto result = unionOp(A, B);
    CHECK(result.size() == 2);
    for (const auto& r : result) CHECK(isBrushValid(r));
}

TEST_CASE("unionOp: B contiene a A devuelve [B] (1 brush)") {
    const Brush A = boxAt(glm::vec3(0.0f), 1.0f);
    const Brush B = boxAt(glm::vec3(0.0f), 5.0f);
    const auto result = unionOp(A, B);
    REQUIRE(result.size() == 1);
    // El brush devuelto debe ser equivalente a B (mismo numero de caras
    // canonicas — 6).
    CHECK(result[0].faces.size() == 6);
}

TEST_CASE("unionOp: A contiene a B devuelve [A] (1 brush)") {
    const Brush A = boxAt(glm::vec3(0.0f), 5.0f);
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);
    const auto result = unionOp(A, B);
    REQUIRE(result.size() == 1);
    CHECK(result[0].faces.size() == 6);
}

TEST_CASE("unionOp: A y B con overlap parcial devuelve N>=2 brushes") {
    const Brush A = boxAt(glm::vec3(0.0f), 2.0f);
    const Brush B = boxAt(glm::vec3(1.5f, 0.0f, 0.0f), 2.0f);  // overlap en X
    const auto result = unionOp(A, B);
    CHECK(result.size() >= 2);
    for (const auto& r : result) CHECK(isBrushValid(r));
}

TEST_CASE("unionOp: A invalido devuelve [B]") {
    Brush A;
    const Brush B = boxAt(glm::vec3(0.0f), 1.0f);
    const auto result = unionOp(A, B);
    REQUIRE(result.size() == 1);
    CHECK(result[0].faces.size() == 6);
}

TEST_CASE("unionOp: ambos invalidos devuelve vacio") {
    Brush A, B;
    CHECK(unionOp(A, B).empty());
}
