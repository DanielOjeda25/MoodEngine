// Tests del subsystem CSG (F2H11): tipos Brush + algoritmo
// brush implicito -> mesh triangulada. Cubren:
//   - makeBoxBrush con identity, traslacion, rotacion, escala.
//   - computeBrushAabb: encuentra los 8 vertices del box correctamente.
//   - buildBrushMesh: 24 vertices (4*6) + 36 indices (6*2 tris) para
//     box unitaria, normales correctas, UVs no degeneradas.
//   - Determinismo: misma entrada -> misma salida.
//   - Brush degenerado (< 4 caras): mesh vacia sin crash.
//   - Box transformada: la mesh sigue siendo cerrada / volumetrica.

#include <doctest/doctest.h>

#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushMesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <set>

using namespace Mood;
using namespace Mood::Csg;

namespace {
constexpr f32 k_eps = 1e-3f;  // ligeramente mayor que kPlaneEpsilon
                              // para acomodar acumulacion de errores.
}

// ============================================================
// makeBoxBrush
// ============================================================

TEST_CASE("makeBoxBrush identity: 6 caras con normales canonicas") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    REQUIRE(b.faces.size() == 6);

    // Las 6 normales esperadas (en algun orden).
    const glm::vec3 expected[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1},
    };

    // Cada normal canonica debe aparecer exactamente una vez.
    for (const auto& exp : expected) {
        int matches = 0;
        for (const auto& face : b.faces) {
            if (glm::length(face.plane.normal - exp) < k_eps) ++matches;
        }
        CHECK(matches == 1);
    }
}

TEST_CASE("makeBoxBrush identity: AABB local es [-0.5, +0.5]^3") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    CHECK(b.localAabb.min.x == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.y == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.z == doctest::Approx(-0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.x == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(0.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.z == doctest::Approx(0.5f).epsilon(k_eps));
}

TEST_CASE("makeBoxBrush trasladado: AABB se desplaza, no se deforma") {
    const glm::mat4 T = glm::translate(glm::mat4(1.0f),
                                        glm::vec3(10.0f, 5.0f, -3.0f));
    const Brush b = makeBoxBrush(T);
    CHECK(b.localAabb.min.x == doctest::Approx(9.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.x == doctest::Approx(10.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.y == doctest::Approx(4.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(5.5f).epsilon(k_eps));
    CHECK(b.localAabb.min.z == doctest::Approx(-3.5f).epsilon(k_eps));
    CHECK(b.localAabb.max.z == doctest::Approx(-2.5f).epsilon(k_eps));
}

TEST_CASE("makeBoxBrush escalado uniforme: AABB escala") {
    const glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    const Brush b = makeBoxBrush(S);
    // Box local de lado 1 escalada x2 -> lado 2, AABB de [-1, +1].
    CHECK(b.localAabb.min.x == doctest::Approx(-1.0f).epsilon(k_eps));
    CHECK(b.localAabb.max.x == doctest::Approx(1.0f).epsilon(k_eps));
    CHECK(b.localAabb.size().x == doctest::Approx(2.0f).epsilon(k_eps));
}

TEST_CASE("makeBoxBrush rotado 45deg en Y: AABB crece (cubo rotado)") {
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f),
                                     glm::vec3(0, 1, 0));
    const Brush b = makeBoxBrush(R);
    // Cubo unitario rotado 45deg: AABB en X y Z crece a sqrt(2)/2 ~ 0.707
    const f32 expected_extent = std::sqrt(2.0f) * 0.5f;
    CHECK(b.localAabb.max.x == doctest::Approx(expected_extent).epsilon(k_eps));
    CHECK(b.localAabb.max.z == doctest::Approx(expected_extent).epsilon(k_eps));
    CHECK(b.localAabb.max.y == doctest::Approx(0.5f).epsilon(k_eps));
}

TEST_CASE("makeBoxBrush: las normales apuntan hacia afuera (centro queda dentro)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    // El centro de la box (origen) debe estar del lado interior de
    // todas las caras: signedDistance < 0.
    for (const auto& face : b.faces) {
        CHECK(signedDistance(face.plane, glm::vec3(0.0f)) < -k_eps);
    }
}

TEST_CASE("makeBoxBrush: punto fuera del cubo da signedDistance > 0 en al menos una cara") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const glm::vec3 outside(10.0f, 0.0f, 0.0f);
    bool exterior = false;
    for (const auto& face : b.faces) {
        if (signedDistance(face.plane, outside) > k_eps) {
            exterior = true;
            break;
        }
    }
    CHECK(exterior);
}

// ============================================================
// computeBrushAabb
// ============================================================

TEST_CASE("computeBrushAabb: brush vacio devuelve AABB no-valida") {
    Brush b{};  // 0 faces
    const AABB aabb = computeBrushAabb(b);
    CHECK(aabb.size().x == doctest::Approx(0.0f));
    CHECK(aabb.size().y == doctest::Approx(0.0f));
    CHECK(aabb.size().z == doctest::Approx(0.0f));
}

TEST_CASE("computeBrushAabb: < 4 faces es degenerado") {
    Brush b;
    b.faces.push_back({Plane{glm::vec3(1, 0, 0), 0.0f}, 0});
    b.faces.push_back({Plane{glm::vec3(0, 1, 0), 0.0f}, 0});
    b.faces.push_back({Plane{glm::vec3(0, 0, 1), 0.0f}, 0});
    const AABB aabb = computeBrushAabb(b);
    CHECK(!aabb.isValid());
}

TEST_CASE("computeBrushAabb: box trasladado consistente con makeBoxBrush") {
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(7.0f));
    const Brush b = makeBoxBrush(T);
    const AABB aabb = computeBrushAabb(b);
    CHECK(aabb.min.x == doctest::Approx(b.localAabb.min.x).epsilon(k_eps));
    CHECK(aabb.max.x == doctest::Approx(b.localAabb.max.x).epsilon(k_eps));
}

// ============================================================
// buildBrushMesh
// ============================================================

TEST_CASE("buildBrushMesh box unitaria: 24 vertices y 36 indices") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    // 6 caras x 4 vertices unicos por cara = 24 vertices distintos.
    // (Cada cara tiene su propio normal/uv aunque comparta posicion
    // en una arista; no se sueldan entre caras en F2H11.)
    CHECK(mesh.vertices.size() == 24);
    // 6 caras x 2 triangulos x 3 indices = 36.
    CHECK(mesh.indices.size() == 36);
}

TEST_CASE("buildBrushMesh box: cada vertice esta en alguna esquina de la box") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    // Las 8 esquinas esperadas de la box unitaria.
    for (const auto& v : mesh.vertices) {
        const f32 ax = std::fabs(v.position.x);
        const f32 ay = std::fabs(v.position.y);
        const f32 az = std::fabs(v.position.z);
        CHECK(ax == doctest::Approx(0.5f).epsilon(k_eps));
        CHECK(ay == doctest::Approx(0.5f).epsilon(k_eps));
        CHECK(az == doctest::Approx(0.5f).epsilon(k_eps));
    }
}

TEST_CASE("buildBrushMesh box: cada vertice tiene una normal canonica") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    for (const auto& v : mesh.vertices) {
        // La normal debe coincidir con uno de +/-X, +/-Y, +/-Z.
        const f32 ax = std::fabs(v.normal.x);
        const f32 ay = std::fabs(v.normal.y);
        const f32 az = std::fabs(v.normal.z);
        const bool isCanonical =
            (ax > 0.99f && ay < k_eps && az < k_eps) ||
            (ay > 0.99f && ax < k_eps && az < k_eps) ||
            (az > 0.99f && ax < k_eps && ay < k_eps);
        CHECK(isCanonical);
    }
}

TEST_CASE("buildBrushMesh box: existen 4 vertices con normal +X y todos en x=0.5") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    int count_pos_x = 0;
    for (const auto& v : mesh.vertices) {
        if (v.normal.x > 0.99f) {
            ++count_pos_x;
            CHECK(v.position.x == doctest::Approx(0.5f).epsilon(k_eps));
        }
    }
    CHECK(count_pos_x == 4);
}

TEST_CASE("buildBrushMesh: triangulos no degenerados (vertices distintos)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    REQUIRE(mesh.indices.size() % 3 == 0);
    for (usize i = 0; i < mesh.indices.size(); i += 3) {
        const u32 a = mesh.indices[i];
        const u32 ib = mesh.indices[i + 1];
        const u32 ic = mesh.indices[i + 2];
        CHECK(a != ib);
        CHECK(ib != ic);
        CHECK(a != ic);
    }
}

TEST_CASE("buildBrushMesh: triangulos con area > 0") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    for (usize i = 0; i < mesh.indices.size(); i += 3) {
        const glm::vec3& p0 = mesh.vertices[mesh.indices[i]].position;
        const glm::vec3& p1 = mesh.vertices[mesh.indices[i + 1]].position;
        const glm::vec3& p2 = mesh.vertices[mesh.indices[i + 2]].position;
        const glm::vec3 cross = glm::cross(p1 - p0, p2 - p0);
        const f32 area2 = glm::length(cross);
        CHECK(area2 > k_eps);
    }
}

TEST_CASE("buildBrushMesh: triangulos respetan la normal de la cara (CCW visto desde +n)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    for (usize i = 0; i < mesh.indices.size(); i += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[i]];
        const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
        const auto& v2 = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 cross = glm::cross(v1.position - v0.position,
                                            v2.position - v0.position);
        // El cross debe apuntar hacia la normal de la cara
        // (los 3 vertices comparten normal por construccion).
        CHECK(glm::dot(glm::normalize(cross), v0.normal) > 0.99f);
    }
}

TEST_CASE("buildBrushMesh: brush trasladado mueve la mesh consistentemente") {
    // Translacion solo en X (no usar glm::vec3(10.0f) que es (10,10,10)).
    const glm::mat4 T = glm::translate(glm::mat4(1.0f),
                                        glm::vec3(10.0f, 0.0f, 0.0f));
    const Brush b = makeBoxBrush(T);
    const BrushMeshData mesh = buildBrushMesh(b);
    REQUIRE(mesh.vertices.size() == 24);
    for (const auto& v : mesh.vertices) {
        // x entre 9.5 y 10.5 (box centrada en (10,0,0))
        CHECK(v.position.x >= 9.5f - k_eps);
        CHECK(v.position.x <= 10.5f + k_eps);
        CHECK(v.position.y >= -0.5f - k_eps);
        CHECK(v.position.y <= 0.5f + k_eps);
        CHECK(v.position.z >= -0.5f - k_eps);
        CHECK(v.position.z <= 0.5f + k_eps);
    }
}

TEST_CASE("buildBrushMesh: brush degenerado < 4 caras devuelve mesh vacia") {
    Brush b;
    b.faces.push_back({Plane{glm::vec3(1, 0, 0), 0.0f}, 0});
    b.faces.push_back({Plane{glm::vec3(-1, 0, 0), -1.0f}, 0});  // x = 1
    const BrushMeshData mesh = buildBrushMesh(b);
    CHECK(mesh.vertices.empty());
    CHECK(mesh.indices.empty());
}

TEST_CASE("buildBrushMesh: determinismo bit-a-bit") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData m1 = buildBrushMesh(b);
    const BrushMeshData m2 = buildBrushMesh(b);
    REQUIRE(m1.vertices.size() == m2.vertices.size());
    REQUIRE(m1.indices.size() == m2.indices.size());
    for (usize i = 0; i < m1.vertices.size(); ++i) {
        CHECK(m1.vertices[i].position.x == m2.vertices[i].position.x);
        CHECK(m1.vertices[i].position.y == m2.vertices[i].position.y);
        CHECK(m1.vertices[i].position.z == m2.vertices[i].position.z);
    }
    for (usize i = 0; i < m1.indices.size(); ++i) {
        CHECK(m1.indices[i] == m2.indices[i]);
    }
}

TEST_CASE("buildBrushMesh: materialIndex se propaga a los vertices") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f), 42);
    const BrushMeshData mesh = buildBrushMesh(b);
    for (const auto& v : mesh.vertices) {
        CHECK(v.materialIndex == 42);
    }
}

TEST_CASE("buildBrushMesh: las UV no son todas iguales (proyeccion planar funciona)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    // Comprobar que hay variacion en U y V.
    f32 uMin = mesh.vertices[0].uv.x, uMax = mesh.vertices[0].uv.x;
    f32 vMin = mesh.vertices[0].uv.y, vMax = mesh.vertices[0].uv.y;
    for (const auto& v : mesh.vertices) {
        uMin = std::min(uMin, v.uv.x);
        uMax = std::max(uMax, v.uv.x);
        vMin = std::min(vMin, v.uv.y);
        vMax = std::max(vMax, v.uv.y);
    }
    CHECK((uMax - uMin) > k_eps);
    CHECK((vMax - vMin) > k_eps);
}

// ============================================================
// brushMeshDataToInterleaved
// ============================================================

TEST_CASE("brushMeshDataToInterleaved: box -> 36 vertices * 11 floats = 396 floats") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    REQUIRE(mesh.indices.size() == 36);
    const std::vector<f32> interleaved = brushMeshDataToInterleaved(mesh);
    CHECK(interleaved.size() == 36 * 11);
}

TEST_CASE("brushMeshDataToInterleaved: layout pos(3)+color(3)+uv(2)+normal(3)") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh = buildBrushMesh(b);
    const std::vector<f32> flat = brushMeshDataToInterleaved(mesh);
    REQUIRE(flat.size() >= 11);

    const u32 firstIdx = mesh.indices[0];
    const auto& v0 = mesh.vertices[firstIdx];
    // Posicion (offset 0-2)
    CHECK(flat[0] == doctest::Approx(v0.position.x));
    CHECK(flat[1] == doctest::Approx(v0.position.y));
    CHECK(flat[2] == doctest::Approx(v0.position.z));
    // Color (offset 3-5) = blanco
    CHECK(flat[3] == doctest::Approx(1.0f));
    CHECK(flat[4] == doctest::Approx(1.0f));
    CHECK(flat[5] == doctest::Approx(1.0f));
    // UV (offset 6-7)
    CHECK(flat[6] == doctest::Approx(v0.uv.x));
    CHECK(flat[7] == doctest::Approx(v0.uv.y));
    // Normal (offset 8-10)
    CHECK(flat[8] == doctest::Approx(v0.normal.x));
    CHECK(flat[9] == doctest::Approx(v0.normal.y));
    CHECK(flat[10] == doctest::Approx(v0.normal.z));
}

TEST_CASE("brushMeshDataToInterleaved: mesh vacia produce vector vacio") {
    BrushMeshData empty;
    const std::vector<f32> flat = brushMeshDataToInterleaved(empty);
    CHECK(flat.empty());
}

// ============================================================
// F2H15: defaultTangentBasis + UV params + lockToWorld
// ============================================================

TEST_CASE("defaultTangentBasis: ejes ortogonales unitarios y ortogonales a la normal") {
    const glm::vec3 normals[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1},
    };
    for (const auto& n : normals) {
        glm::vec3 u, v;
        defaultTangentBasis(n, u, v);
        CHECK(glm::length(u) == doctest::Approx(1.0f).epsilon(k_eps));
        CHECK(glm::length(v) == doctest::Approx(1.0f).epsilon(k_eps));
        CHECK(std::fabs(glm::dot(u, n)) < k_eps);
        CHECK(std::fabs(glm::dot(v, n)) < k_eps);
        CHECK(std::fabs(glm::dot(u, v)) < k_eps);
    }
}

TEST_CASE("defaultTangentBasis: estable (misma normal -> mismos ejes)") {
    glm::vec3 u1, v1, u2, v2;
    defaultTangentBasis(glm::normalize(glm::vec3(0.5f, 0.7f, 0.3f)), u1, v1);
    defaultTangentBasis(glm::normalize(glm::vec3(0.5f, 0.7f, 0.3f)), u2, v2);
    CHECK(glm::length(u1 - u2) < k_eps);
    CHECK(glm::length(v1 - v2) < k_eps);
}

TEST_CASE("makeBoxBrush: cada cara tiene uAxis/vAxis no-cero") {
    const Brush b = makeBoxBrush(glm::mat4(1.0f));
    for (const auto& face : b.faces) {
        CHECK(glm::length(face.uAxis) == doctest::Approx(1.0f).epsilon(k_eps));
        CHECK(glm::length(face.vAxis) == doctest::Approx(1.0f).epsilon(k_eps));
    }
}

TEST_CASE("buildBrushMesh: uvScale=2 duplica las UVs vs scale=1") {
    Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh1 = buildBrushMesh(b);

    for (auto& face : b.faces) face.uvScale = glm::vec2(2.0f);
    const BrushMeshData mesh2 = buildBrushMesh(b);

    REQUIRE(mesh1.vertices.size() == mesh2.vertices.size());
    // Para cada vertex, mesh2.uv = mesh1.uv * 2.
    for (usize i = 0; i < mesh1.vertices.size(); ++i) {
        CHECK(mesh2.vertices[i].uv.x ==
              doctest::Approx(mesh1.vertices[i].uv.x * 2.0f).epsilon(k_eps));
        CHECK(mesh2.vertices[i].uv.y ==
              doctest::Approx(mesh1.vertices[i].uv.y * 2.0f).epsilon(k_eps));
    }
}

TEST_CASE("buildBrushMesh: uvOffset desplaza las UVs") {
    Brush b = makeBoxBrush(glm::mat4(1.0f));
    const BrushMeshData mesh1 = buildBrushMesh(b);

    const glm::vec2 off(0.5f, -0.25f);
    for (auto& face : b.faces) face.uvOffset = off;
    const BrushMeshData mesh2 = buildBrushMesh(b);

    REQUIRE(mesh1.vertices.size() == mesh2.vertices.size());
    for (usize i = 0; i < mesh1.vertices.size(); ++i) {
        CHECK(mesh2.vertices[i].uv.x ==
              doctest::Approx(mesh1.vertices[i].uv.x + off.x).epsilon(k_eps));
        CHECK(mesh2.vertices[i].uv.y ==
              doctest::Approx(mesh1.vertices[i].uv.y + off.y).epsilon(k_eps));
    }
}

TEST_CASE("buildBrushMesh: lockToWorld=true cambia UVs al transformar") {
    Brush b = makeBoxBrush(glm::mat4(1.0f));
    for (auto& face : b.faces) face.lockToWorld = true;

    const glm::mat4 identity(1.0f);
    const glm::mat4 translated = glm::translate(glm::mat4(1.0f),
                                                  glm::vec3(10.0f, 0.0f, 0.0f));

    const BrushMeshData mesh1 = buildBrushMesh(b, identity);
    const BrushMeshData mesh2 = buildBrushMesh(b, translated);

    REQUIRE(mesh1.vertices.size() == mesh2.vertices.size());
    REQUIRE(mesh1.vertices.size() > 0);

    // Con lockToWorld, las UVs cambian segun la posicion world.
    // No requerimos que CADA vertex difiera, pero al menos uno debe.
    bool anyDifferent = false;
    for (usize i = 0; i < mesh1.vertices.size(); ++i) {
        if (std::fabs(mesh1.vertices[i].uv.x - mesh2.vertices[i].uv.x) > k_eps ||
            std::fabs(mesh1.vertices[i].uv.y - mesh2.vertices[i].uv.y) > k_eps) {
            anyDifferent = true;
            break;
        }
    }
    CHECK(anyDifferent);
}

TEST_CASE("buildBrushMesh: lockToWorld=false ignora worldMatrix") {
    Brush b = makeBoxBrush(glm::mat4(1.0f));
    // Default lockToWorld=false. Las UVs deben ser identicas
    // independientemente del worldMatrix pasado.
    const BrushMeshData mesh1 = buildBrushMesh(b, glm::mat4(1.0f));
    const BrushMeshData mesh2 = buildBrushMesh(b,
        glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, -3.0f)));

    REQUIRE(mesh1.vertices.size() == mesh2.vertices.size());
    for (usize i = 0; i < mesh1.vertices.size(); ++i) {
        CHECK(mesh1.vertices[i].uv.x ==
              doctest::Approx(mesh2.vertices[i].uv.x).epsilon(k_eps));
        CHECK(mesh1.vertices[i].uv.y ==
              doctest::Approx(mesh2.vertices[i].uv.y).epsilon(k_eps));
    }
}

TEST_CASE("buildBrushMesh: box rotada produce mesh cerrada (centroide dentro de AABB)") {
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f),
                                     glm::vec3(0, 1, 0));
    const Brush b = makeBoxBrush(R);
    const BrushMeshData mesh = buildBrushMesh(b);
    REQUIRE(mesh.vertices.size() == 24);

    // El centroide de los 24 vertices debe estar cerca del origen
    // (la box es simetrica).
    glm::vec3 centroid(0.0f);
    for (const auto& v : mesh.vertices) centroid += v.position;
    centroid /= static_cast<f32>(mesh.vertices.size());
    CHECK(centroid.x == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(centroid.y == doctest::Approx(0.0f).epsilon(k_eps));
    CHECK(centroid.z == doctest::Approx(0.0f).epsilon(k_eps));
}
