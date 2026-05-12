// Tests del PrimitiveMeshes (sanidad del winding).
//
// Investigando feedback del dev (2026-05-12): los cubos primitivos
// (slot 0 de AssetManager, usado por tile walls + spawns sin model_path +
// gizmos) podrian tener winding inconsistente con sus normales declaradas.
// El setup global es:
//   - glEnable(GL_CULL_FACE)
//   - glCullFace(GL_BACK)
//   - glFrontFace(GL_CCW)
//
// Una triangulo con vertices CCW desde la perspectiva del viewer es
// FRONT-facing. El "viewer" desde afuera de una cara coincide con la
// direccion de la normal. Por lo tanto: si el cross product
// (v1-v0) x (v2-v0) apunta en el mismo sentido que la normal declarada,
// la cara es CCW desde afuera = FRONT desde afuera = visible al
// renderear. Si apunta opuesto, la cara queda culled cuando se ve desde
// afuera (= bug).

#include <doctest/doctest.h>

#include "engine/assets/primitives/PrimitiveMeshes.h"

#include <glm/glm.hpp>

using namespace Mood;

namespace {

constexpr int k_stride = 11;  // pos(3) + color(3) + uv(2) + normal(3)

/// @brief Lee el vertice i-esimo del buffer interleaved.
/// @returns pair { pos, normal }.
std::pair<glm::vec3, glm::vec3> readVertex(const std::vector<f32>& v, int i) {
    const int base = i * k_stride;
    return {
        glm::vec3(v[base + 0], v[base + 1], v[base + 2]),
        glm::vec3(v[base + 8], v[base + 9], v[base + 10]),
    };
}

} // namespace

TEST_CASE("PrimitiveMeshes::createCubeMesh: cada cara tiene winding CCW "
          "desde afuera (consistente con su normal declarada)") {
    auto data = createCubeMesh();
    REQUIRE(data.vertices.size() == 36 * k_stride);  // 6 caras * 6 verts

    // 6 caras * 2 triangulos = 12 triangulos. Itero de 3 en 3.
    for (int tri = 0; tri < 12; ++tri) {
        const int v0i = tri * 3 + 0;
        const int v1i = tri * 3 + 1;
        const int v2i = tri * 3 + 2;
        const auto [p0, n0] = readVertex(data.vertices, v0i);
        const auto [p1, n1] = readVertex(data.vertices, v1i);
        const auto [p2, n2] = readVertex(data.vertices, v2i);

        // Sanidad: los 3 vertices del mismo triangulo comparten normal
        // declarada (caras duras).
        CHECK(glm::all(glm::equal(n0, n1)));
        CHECK(glm::all(glm::equal(n0, n2)));

        // Cross product del triangulo en object space. Si todo esta
        // bien, apunta en la misma direccion que la normal.
        const glm::vec3 edge1 = p1 - p0;
        const glm::vec3 edge2 = p2 - p0;
        const glm::vec3 cross = glm::cross(edge1, edge2);
        // Normalizamos ambos para comparar direccion (sin importar mag).
        const glm::vec3 crossN = glm::normalize(cross);
        const float dotWithNormal = glm::dot(crossN, n0);

        // dot ~ +1 = mismo sentido (winding CCW desde afuera, OK)
        // dot ~ -1 = opuesto (winding CW desde afuera, queda culled)
        INFO("Triangulo ", tri, " p0=", p0.x, ",", p0.y, ",", p0.z,
             " normal=", n0.x, ",", n0.y, ",", n0.z,
             " cross_norm=", crossN.x, ",", crossN.y, ",", crossN.z,
             " dot=", dotWithNormal);
        CHECK(dotWithNormal > 0.99f);
    }
}
