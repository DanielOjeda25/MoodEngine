#include "engine/physics/cloth/ClothLayout.h"

#include <glm/geometric.hpp>  // glm::dot

#include <algorithm>
#include <cmath>

namespace Mood::cloth {

namespace {

// Agrega un edge entre (xa,ya) y (xb,yb) calculando su restLength desde las
// posiciones locales ya generadas.
void addEdge(ClothLayout& layout, int xa, int ya, int xb, int yb,
             EdgeKind kind) {
    const int a = ClothLayout::idx(xa, ya, layout.resX);
    const int b = ClothLayout::idx(xb, yb, layout.resX);
    const glm::vec3 d =
        layout.particles[a].localPos - layout.particles[b].localPos;
    ClothEdge e;
    e.a = a;
    e.b = b;
    e.restLength = std::sqrt(glm::dot(d, d));
    e.kind = kind;
    layout.edges.push_back(e);
}

} // namespace

ClothLayout buildGridCloth(f32 width, f32 height, int resX, int resY,
                            AnchorEdge anchor) {
    ClothLayout layout;

    // Clamps defensivos: una tela necesita al menos 2x2 particulas (1 celda)
    // y dimensiones positivas. Sin esto, divisiones por (res-1) explotan.
    layout.resX = std::max(resX, 2);
    layout.resY = std::max(resY, 2);
    const f32 w = std::max(width, 0.001f);
    const f32 h = std::max(height, 0.001f);

    const int rx = layout.resX;
    const int ry = layout.resY;

    // --- Particulas (row-major, fila 0 = borde superior) ---
    layout.particles.reserve(static_cast<usize>(rx) * ry);
    for (int y = 0; y < ry; ++y) {
        // ty in [0,1]: 0 = arriba (local +h/2), 1 = abajo (local -h/2).
        const f32 ty = static_cast<f32>(y) / static_cast<f32>(ry - 1);
        const f32 localY = (0.5f - ty) * h;
        for (int x = 0; x < rx; ++x) {
            const f32 tx = static_cast<f32>(x) / static_cast<f32>(rx - 1);
            const f32 localX = (tx - 0.5f) * w;
            ClothParticle p;
            p.localPos = glm::vec3(localX, localY, 0.0f);
            p.pinned = false;
            layout.particles.push_back(p);
        }
    }

    // --- Anclajes ---
    auto pin = [&](int x, int y) {
        layout.particles[ClothLayout::idx(x, y, rx)].pinned = true;
    };
    switch (anchor) {
        case AnchorEdge::None:
            break;
        case AnchorEdge::TopEdge:
            for (int x = 0; x < rx; ++x) pin(x, 0);
            break;
        case AnchorEdge::TopCorners:
            pin(0, 0);
            pin(rx - 1, 0);
            break;
        case AnchorEdge::LeftEdge:
            for (int y = 0; y < ry; ++y) pin(0, y);
            break;
    }

    // --- Resortes ---
    // structural: vecinos directos (derecha + abajo, para no duplicar).
    for (int y = 0; y < ry; ++y) {
        for (int x = 0; x < rx; ++x) {
            if (x + 1 < rx) addEdge(layout, x, y, x + 1, y, EdgeKind::Structural);
            if (y + 1 < ry) addEdge(layout, x, y, x, y + 1, EdgeKind::Structural);
        }
    }
    // shear: ambas diagonales de cada celda.
    for (int y = 0; y + 1 < ry; ++y) {
        for (int x = 0; x + 1 < rx; ++x) {
            addEdge(layout, x, y, x + 1, y + 1, EdgeKind::Shear);
            addEdge(layout, x + 1, y, x, y + 1, EdgeKind::Shear);
        }
    }
    // bend: vecinos a 2 de distancia (derecha + abajo).
    for (int y = 0; y < ry; ++y) {
        for (int x = 0; x < rx; ++x) {
            if (x + 2 < rx) addEdge(layout, x, y, x + 2, y, EdgeKind::Bend);
            if (y + 2 < ry) addEdge(layout, x, y, x, y + 2, EdgeKind::Bend);
        }
    }

    // --- Triangulos (2 por celda) ---
    // Winding CCW visto desde +Z. Como la tela se renderiza doble cara, el
    // sentido exacto no afecta el culling, pero lo mantenemos consistente
    // para que el recalculo de normales por triangulo no se cancele entre
    // celdas vecinas.
    layout.triangleIndices.reserve(
        static_cast<usize>(rx - 1) * (ry - 1) * 6);
    for (int y = 0; y + 1 < ry; ++y) {
        for (int x = 0; x + 1 < rx; ++x) {
            const u32 v00 = static_cast<u32>(ClothLayout::idx(x, y, rx));
            const u32 v10 = static_cast<u32>(ClothLayout::idx(x + 1, y, rx));
            const u32 v01 = static_cast<u32>(ClothLayout::idx(x, y + 1, rx));
            const u32 v11 = static_cast<u32>(ClothLayout::idx(x + 1, y + 1, rx));
            // Triangulo 1: v00, v01, v10. Triangulo 2: v10, v01, v11.
            layout.triangleIndices.push_back(v00);
            layout.triangleIndices.push_back(v01);
            layout.triangleIndices.push_back(v10);
            layout.triangleIndices.push_back(v10);
            layout.triangleIndices.push_back(v01);
            layout.triangleIndices.push_back(v11);
        }
    }

    return layout;
}

} // namespace Mood::cloth
