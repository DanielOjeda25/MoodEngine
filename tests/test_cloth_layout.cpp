// F2H75 Bloque A: tests del layout de tela puro (sin Jolt). Verifican que
// la grilla procedural produce particulas, resortes y triangulos coherentes,
// y que los anclajes marcan las particulas correctas.

#include <doctest/doctest.h>

#include "engine/physics/cloth/ClothLayout.h"

#include <cmath>
#include <set>

using namespace Mood;
using namespace Mood::cloth;

namespace {
// Cuenta cuantas particulas quedaron ancladas.
int countPinned(const ClothLayout& l) {
    int n = 0;
    for (const auto& p : l.particles) if (p.pinned) ++n;
    return n;
}
} // namespace

TEST_CASE("ClothLayout F2H75 A: grilla 2x2 minima") {
    auto l = buildGridCloth(1.0f, 1.0f, 2, 2, AnchorEdge::None);
    CHECK(l.resX == 2);
    CHECK(l.resY == 2);
    CHECK(l.vertexCount() == 4);
    CHECK_FALSE(l.empty());
    // 2 triangulos = 6 indices (1 sola celda).
    CHECK(l.triangleIndices.size() == 6);
    // structural: 2 horizontales (1 por fila) + 2 verticales (1 por col) = 4.
    // shear: 2 diagonales de la unica celda. bend: 0 (no hay vecinos a 2).
    // total = 6.
    CHECK(l.edges.size() == 6);
}

TEST_CASE("ClothLayout F2H75 A: clamps defensivos (res < 2, dim <= 0)") {
    auto l = buildGridCloth(-5.0f, 0.0f, 1, 0, AnchorEdge::None);
    CHECK(l.resX == 2);   // clamp a 2
    CHECK(l.resY == 2);   // clamp a 2
    CHECK(l.vertexCount() == 4);
    // las dims se clampean a > 0: no debe haber NaN ni posiciones colapsadas
    // a un solo punto (las 4 esquinas son distintas).
    std::set<std::tuple<f32, f32, f32>> uniq;
    for (const auto& p : l.particles) {
        uniq.insert({p.localPos.x, p.localPos.y, p.localPos.z});
    }
    CHECK(uniq.size() == 4);
}

TEST_CASE("ClothLayout F2H75 A: indices de triangulos en rango") {
    auto l = buildGridCloth(2.0f, 3.0f, 5, 7, AnchorEdge::TopEdge);
    const int vc = l.vertexCount();
    CHECK(vc == 35);
    // (resX-1)*(resY-1) celdas * 2 triangulos * 3 indices.
    CHECK(l.triangleIndices.size() == static_cast<usize>(4 * 6 * 6));
    for (u32 i : l.triangleIndices) {
        CHECK(i < static_cast<u32>(vc));
    }
    // ningun triangulo degenerado (3 indices distintos).
    for (usize t = 0; t < l.triangleIndices.size(); t += 3) {
        const u32 a = l.triangleIndices[t];
        const u32 b = l.triangleIndices[t + 1];
        const u32 c = l.triangleIndices[t + 2];
        CHECK(a != b);
        CHECK(b != c);
        CHECK(a != c);
    }
}

TEST_CASE("ClothLayout F2H75 A: edges referencian indices validos + restLength>0") {
    auto l = buildGridCloth(2.0f, 2.0f, 4, 4, AnchorEdge::None);
    const int vc = l.vertexCount();
    for (const auto& e : l.edges) {
        CHECK(e.a >= 0);
        CHECK(e.a < vc);
        CHECK(e.b >= 0);
        CHECK(e.b < vc);
        CHECK(e.a != e.b);
        CHECK(e.restLength > 0.0f);
    }
    // debe haber de las 3 familias.
    bool hasStruct = false, hasShear = false, hasBend = false;
    for (const auto& e : l.edges) {
        if (e.kind == EdgeKind::Structural) hasStruct = true;
        if (e.kind == EdgeKind::Shear)      hasShear = true;
        if (e.kind == EdgeKind::Bend)       hasBend = true;
    }
    CHECK(hasStruct);
    CHECK(hasShear);
    CHECK(hasBend);
}

TEST_CASE("ClothLayout F2H75 A: anclaje TopEdge fija la fila superior") {
    const int rx = 6, ry = 4;
    auto l = buildGridCloth(2.0f, 2.0f, rx, ry, AnchorEdge::TopEdge);
    CHECK(countPinned(l) == rx);  // toda la fila 0
    for (int x = 0; x < rx; ++x) {
        CHECK(l.particles[ClothLayout::idx(x, 0, rx)].pinned);
    }
    // la fila de abajo NO esta anclada.
    for (int x = 0; x < rx; ++x) {
        CHECK_FALSE(l.particles[ClothLayout::idx(x, ry - 1, rx)].pinned);
    }
}

TEST_CASE("ClothLayout F2H75 A: anclaje TopCorners fija solo 2 particulas") {
    const int rx = 5, ry = 5;
    auto l = buildGridCloth(2.0f, 2.0f, rx, ry, AnchorEdge::TopCorners);
    CHECK(countPinned(l) == 2);
    CHECK(l.particles[ClothLayout::idx(0, 0, rx)].pinned);
    CHECK(l.particles[ClothLayout::idx(rx - 1, 0, rx)].pinned);
}

TEST_CASE("ClothLayout F2H75 A: anclaje LeftEdge fija la columna izquierda") {
    const int rx = 5, ry = 6;
    auto l = buildGridCloth(2.0f, 2.0f, rx, ry, AnchorEdge::LeftEdge);
    CHECK(countPinned(l) == ry);
    for (int y = 0; y < ry; ++y) {
        CHECK(l.particles[ClothLayout::idx(0, y, rx)].pinned);
    }
}

TEST_CASE("ClothLayout F2H75 A: anclaje None deja todo libre") {
    auto l = buildGridCloth(2.0f, 2.0f, 4, 4, AnchorEdge::None);
    CHECK(countPinned(l) == 0);
}

TEST_CASE("ClothLayout F2H75 A: fila 0 es el borde superior (mayor Y local)") {
    const int rx = 3, ry = 3;
    auto l = buildGridCloth(2.0f, 2.0f, rx, ry, AnchorEdge::None);
    const f32 topY = l.particles[ClothLayout::idx(0, 0, rx)].localPos.y;
    const f32 botY = l.particles[ClothLayout::idx(0, ry - 1, rx)].localPos.y;
    CHECK(topY > botY);
    // centrado en el origen local: top ~ +h/2, bottom ~ -h/2.
    CHECK(topY == doctest::Approx(1.0f));
    CHECK(botY == doctest::Approx(-1.0f));
}
