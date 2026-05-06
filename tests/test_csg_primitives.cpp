// Tests de las primitivas CSG (F2H14): cylinder, prism, sphere,
// pyramid, wedge. Cubren:
//   - Conteos de caras canonicos.
//   - AABB local consistente (~ unit cube tras makeXBrush(identity)).
//   - Normales unitarias y apuntando hacia afuera.
//   - isBrushValid devuelve true para cada primitiva default.
//   - buildBrushMesh produce mesh no-degenerada.
//   - Transformaciones (translate / rotate / scale) preservan
//     consistencia (AABB se mueve, normales se rotan).
//   - Operaciones booleanas con primitivas distintas funcionan
//     (ej. cylinder en una box → agujero cilindrico).

#include <doctest/doctest.h>

#include "core/Types.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushMesh.h"
#include "engine/world/csg/BrushOps.h"
#include "engine/world/csg/Primitives.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <cmath>

using namespace Mood;
using namespace Mood::Csg;

namespace {
constexpr f32 k_eps = 1e-3f;
}

// ============================================================
// Cylinder
// ============================================================

TEST_CASE("makeCylinderBrush default: 18 caras (16 lat + 2 caps)") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f));
    CHECK(b.faces.size() == 18);
}

TEST_CASE("makeCylinderBrush con segments=4: 6 caras (cubo rotado)") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 4);
    CHECK(b.faces.size() == 6);
    CHECK(isBrushValid(b));
}

TEST_CASE("makeCylinderBrush: AABB unitario centrado en origen") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 16);
    // El radio "real" del cilindro lateral (incirculo) es < 0.5
    // por la regularidad del prisma. AABB.x va al vertice del
    // poligono regular (toca la unidad). Verificamos +/- en Y
    // que es 0.5 exacto (los caps).
    CHECK(b.localAabb.min.y == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(0.5f).epsilon(k_eps));
    // En XZ el AABB se acerca a +/- 0.5 con 16 segments.
    CHECK(b.localAabb.max.x == doctest::Approx(0.5f).epsilon(0.02f));
    CHECK(b.localAabb.max.z == doctest::Approx(0.5f).epsilon(0.02f));
}

TEST_CASE("makeCylinderBrush: las normales laterales son unitarias") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 8);
    // Caras 0..7 son laterales; 8 = top, 9 = bottom.
    for (u32 i = 0; i < 8; ++i) {
        const glm::vec3 n = b.faces[i].plane.normal;
        const f32 len = glm::length(n);
        CHECK(len == doctest::Approx(1.0f).epsilon(k_eps));
        // Normal radial: y ≈ 0, sqrt(x^2 + z^2) ≈ 1.
        CHECK(std::fabs(n.y) < k_eps);
    }
    // Top: normal (0,1,0).
    CHECK(b.faces[8].plane.normal.y == doctest::Approx(1.0f).epsilon(k_eps));
    // Bottom: normal (0,-1,0).
    CHECK(b.faces[9].plane.normal.y == doctest::Approx(-1.0f).epsilon(k_eps));
}

TEST_CASE("makeCylinderBrush: isBrushValid + buildBrushMesh produce geometria") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 16);
    REQUIRE(isBrushValid(b));
    const BrushMeshData mesh = buildBrushMesh(b);
    CHECK(mesh.totalIndexCount() > 0);
    // Centroide debe estar cerca del origen (cilindro simetrico).
    glm::vec3 centroid(0.0f);
    for (const auto& v : mesh.allVertices()) centroid += v.position;
    centroid /= static_cast<f32>(mesh.allVertices().size());
    CHECK(centroid.x == doctest::Approx(0.0f).epsilon(0.05f));
    CHECK(centroid.y == doctest::Approx(0.0f).epsilon(0.05f));
    CHECK(centroid.z == doctest::Approx(0.0f).epsilon(0.05f));
}

TEST_CASE("makeCylinderBrush trasladado: AABB se desplaza") {
    const glm::mat4 T = glm::translate(glm::mat4(1.0f),
                                        glm::vec3(5.0f, 2.0f, -3.0f));
    const Brush b = makeCylinderBrush(T, 8);
    CHECK(b.localAabb.center().x == doctest::Approx(5.0f).epsilon(k_eps));
    CHECK(b.localAabb.center().y == doctest::Approx(2.0f).epsilon(k_eps));
    CHECK(b.localAabb.center().z == doctest::Approx(-3.0f).epsilon(k_eps));
}

TEST_CASE("makeCylinderBrush sin segments minimos: brush vacio") {
    const Brush b = makeCylinderBrush(glm::mat4(1.0f), 2);  // < 3
    CHECK(b.faces.empty());
    CHECK_FALSE(isBrushValid(b));
}

// ============================================================
// Prism
// ============================================================

TEST_CASE("makePrismBrush(3): 5 caras (3 lat + 2 caps)") {
    const Brush b = makePrismBrush(glm::mat4(1.0f), 3);
    CHECK(b.faces.size() == 5);
    CHECK(isBrushValid(b));
}

TEST_CASE("makePrismBrush(6): 8 caras (6 lat + 2 caps)") {
    const Brush b = makePrismBrush(glm::mat4(1.0f), 6);
    CHECK(b.faces.size() == 8);
    CHECK(isBrushValid(b));
}

TEST_CASE("makePrismBrush(N) == makeCylinderBrush(N) (mismo conteo)") {
    const Brush prism = makePrismBrush(glm::mat4(1.0f), 5);
    const Brush cyl = makeCylinderBrush(glm::mat4(1.0f), 5);
    CHECK(prism.faces.size() == cyl.faces.size());
    // Validar tambien que las normales coinciden (mismo orden).
    for (usize i = 0; i < prism.faces.size(); ++i) {
        CHECK(glm::length(prism.faces[i].plane.normal -
                            cyl.faces[i].plane.normal) < k_eps);
    }
}

// ============================================================
// Sphere
// ============================================================

TEST_CASE("makeSphereBrush: 12 caras") {
    const Brush b = makeSphereBrush(glm::mat4(1.0f));
    CHECK(b.faces.size() == 12);
}

TEST_CASE("makeSphereBrush: isBrushValid + AABB no-degenerado") {
    const Brush b = makeSphereBrush(glm::mat4(1.0f));
    REQUIRE(isBrushValid(b));
    CHECK(b.localAabb.size().x > k_eps);
    CHECK(b.localAabb.size().y > k_eps);
    CHECK(b.localAabb.size().z > k_eps);
}

TEST_CASE("makeSphereBrush: las 12 normales son unitarias") {
    const Brush b = makeSphereBrush(glm::mat4(1.0f));
    for (const auto& f : b.faces) {
        CHECK(glm::length(f.plane.normal) == doctest::Approx(1.0f).epsilon(k_eps));
    }
}

TEST_CASE("makeSphereBrush: buildBrushMesh produce geometria simetrica") {
    const Brush b = makeSphereBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    CHECK(mesh.totalIndexCount() > 0);
    // Centroide cerca del origen (esfera centrada).
    glm::vec3 centroid(0.0f);
    for (const auto& v : mesh.allVertices()) centroid += v.position;
    centroid /= static_cast<f32>(mesh.allVertices().size());
    CHECK(glm::length(centroid) < 0.05f);
}

TEST_CASE("makeSphereBrush trasladada: AABB se mueve") {
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
    const Brush b = makeSphereBrush(T);
    CHECK(b.localAabb.center().x == doctest::Approx(10.0f).epsilon(0.1f));
}

// ============================================================
// Pyramid
// ============================================================

TEST_CASE("makePyramidBrush: 5 caras (4 lat + base)") {
    const Brush b = makePyramidBrush(glm::mat4(1.0f));
    CHECK(b.faces.size() == 5);
    CHECK(isBrushValid(b));
}

TEST_CASE("makePyramidBrush: base cuadrada en y=-0.5") {
    const Brush b = makePyramidBrush(glm::mat4(1.0f));
    // La cara base es la ultima.
    const auto& base = b.faces.back();
    CHECK(base.plane.normal.y == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(base.plane.distance == doctest::Approx(-0.5f).epsilon(k_eps));
}

TEST_CASE("makePyramidBrush: AABB es unit cube") {
    const Brush b = makePyramidBrush(glm::mat4(1.0f));
    CHECK(b.localAabb.min.x == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.x == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.y == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(0.5f).epsilon(k_eps));
}

TEST_CASE("makePyramidBrush: cima en (0, 0.5, 0)") {
    const Brush b = makePyramidBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    // Debe haber un vertice (la cima) cerca de (0, 0.5, 0).
    bool foundApex = false;
    for (const auto& v : mesh.allVertices()) {
        if (std::fabs(v.position.x) < k_eps &&
            std::fabs(v.position.y - 0.5f) < k_eps &&
            std::fabs(v.position.z) < k_eps) {
            foundApex = true;
            break;
        }
    }
    CHECK(foundApex);
}

TEST_CASE("makePyramidBrush: caras laterales con componente +Y (apuntan arriba)") {
    const Brush b = makePyramidBrush(glm::mat4(1.0f));
    // Las primeras 4 caras son laterales; sus normales apuntan
    // hacia afuera-arriba (componente +Y > 0).
    for (u32 i = 0; i < 4; ++i) {
        CHECK(b.faces[i].plane.normal.y > 0.0f);
    }
}

// ============================================================
// Wedge
// ============================================================

TEST_CASE("makeWedgeBrush: 5 caras") {
    const Brush b = makeWedgeBrush(glm::mat4(1.0f));
    CHECK(b.faces.size() == 5);
    CHECK(isBrushValid(b));
}

TEST_CASE("makeWedgeBrush: plano inclinado con normal (0, +y, +z)") {
    const Brush b = makeWedgeBrush(glm::mat4(1.0f));
    // El ultimo plano definido en Primitives.cpp es el inclinado.
    const auto& slope = b.faces.back();
    CHECK(slope.plane.normal.x == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(slope.plane.normal.y > 0.0f);
    CHECK(slope.plane.normal.z > 0.0f);
}

TEST_CASE("makeWedgeBrush: AABB es unit cube") {
    const Brush b = makeWedgeBrush(glm::mat4(1.0f));
    CHECK(b.localAabb.min.x == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.x == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.y == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.z == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.z == doctest::Approx(0.5f).epsilon(k_eps));
}

// ============================================================
// Boolean ops con primitivas distintas
// ============================================================

TEST_CASE("subtract(Box, Cylinder): box con agujero cilindrico no es vacio") {
    // Box grande conteniendo un cylinder pequeno en el centro.
    const Brush A = makeBoxBrush(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)));
    const Brush B = makeCylinderBrush(glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)));
    const auto result = subtract(A, B);
    CHECK_FALSE(result.empty());
    for (const auto& r : result) {
        CHECK(isBrushValid(r));
    }
}

TEST_CASE("intersect(Cylinder, Sphere): se tocan -> brush valido") {
    const Brush cyl = makeCylinderBrush(glm::mat4(1.0f), 8);
    const Brush sph = makeSphereBrush(glm::mat4(1.0f));
    const auto result = intersectOp(cyl, sph);
    REQUIRE(result.has_value());
    CHECK(isBrushValid(*result));
}

TEST_CASE("subtract(Wedge, Box pequena): wedge intacto si box afuera") {
    const Brush wedge = makeWedgeBrush(glm::mat4(1.0f));
    const Brush smallBox = makeBoxBrush(
        glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)));
    const auto result = subtract(wedge, smallBox);
    REQUIRE(result.size() == 1);
    CHECK(isBrushValid(result[0]));
}

TEST_CASE("union(Pyramid, Box) disjuntos -> 2 brushes") {
    const Brush pyr = makePyramidBrush(glm::mat4(1.0f));
    const Brush box = makeBoxBrush(
        glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)));
    const auto result = unionOp(pyr, box);
    CHECK(result.size() == 2);
}
