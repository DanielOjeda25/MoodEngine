#pragma once

// Frustum culling primitivas (F2H3). Header-only, header dependency-light:
// solo glm + el AABB del motor. La extraccion de planos sigue Gribb-Hartmann
// "Fast Extraction of Viewing Frustum Planes from the World-View-Projection
// Matrix" (2001) — los 6 planos salen directos de la matriz combinada.
//
// El test AABB-vs-frustum usa el truco "p-vertex": para cada plano, el
// vertice del AABB mas extremo en la direccion +normal se calcula con 3
// `select` (nx>0 ? max.x : min.x, ...). Si ese vertice esta del lado
// negativo del plano, los 8 vertices lo estan -> AABB fuera.
// Cuesta 1 dot product por plano en lugar de 8.
//
// Convencion: planos en forma `dot(normal, p) + distance >= 0` para
// "p del lado positivo (visible)". Test conservador: si NO esta
// completamente del lado negativo de un plano, se considera visible —
// esto incluye casos de interseccion (parcialmente dentro).

#include "core/Types.h"
#include "core/math/AABB.h"

#include <glm/geometric.hpp>  // glm::dot, glm::length
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>

namespace Mood {

struct Plane {
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    f32 distance{0.0f};
};

struct Frustum {
    /// @brief Orden: left, right, bottom, top, near, far.
    std::array<Plane, 6> planes{};
};

/// @brief Extrae los 6 planos del frustum a partir de la matriz combinada
///        view * projection (NO al reves — proyeccion-aplicada-primero en
///        notacion column-major de glm: `viewProj = projection * view`).
///        Convencion OpenGL: clip space en Z es [-1, 1], por eso el plano
///        near se obtiene como row3 + row2. Para clip space [0,1] (D3D/Vk)
///        habria que usar row2 directo.
inline Frustum frustumFromViewProj(const glm::mat4& viewProj) {
    // glm es column-major: viewProj[col][row]. La fila i vista como vec4
    // se construye tomando el indice [row][col] al reves.
    auto row = [&](int i) {
        return glm::vec4(viewProj[0][i], viewProj[1][i],
                           viewProj[2][i], viewProj[3][i]);
    };
    const glm::vec4 r0 = row(0);
    const glm::vec4 r1 = row(1);
    const glm::vec4 r2 = row(2);
    const glm::vec4 r3 = row(3);

    auto makePlane = [](const glm::vec4& v) {
        const glm::vec3 n(v.x, v.y, v.z);
        const f32 invLen = 1.0f / glm::length(n);
        return Plane{n * invLen, v.w * invLen};
    };

    Frustum f;
    f.planes[0] = makePlane(r3 + r0); // left
    f.planes[1] = makePlane(r3 - r0); // right
    f.planes[2] = makePlane(r3 + r1); // bottom
    f.planes[3] = makePlane(r3 - r1); // top
    f.planes[4] = makePlane(r3 + r2); // near
    f.planes[5] = makePlane(r3 - r2); // far
    return f;
}

/// @brief True si el AABB en world-space esta visible en el frustum
///        (dentro o intersectandolo). False solo si el AABB esta
///        completamente del lado negativo de algun plano. Conservador:
///        prefiere falsos positivos (dibujar de mas) sobre falsos
///        negativos (mesh que se nota faltar).
inline bool aabbVisible(const AABB& aabbWorld, const Frustum& frustum) {
    for (const Plane& p : frustum.planes) {
        // p-vertex: el vertice del AABB mas en la direccion de la normal.
        // Si ese esta del lado negativo, los otros 7 tambien.
        const glm::vec3 pv(
            p.normal.x > 0.0f ? aabbWorld.max.x : aabbWorld.min.x,
            p.normal.y > 0.0f ? aabbWorld.max.y : aabbWorld.min.y,
            p.normal.z > 0.0f ? aabbWorld.max.z : aabbWorld.min.z);
        if (glm::dot(p.normal, pv) + p.distance < 0.0f) {
            return false;
        }
    }
    return true;
}

/// @brief Transforma un AABB local-space al world-space rotando + escalando
///        + trasladando los 8 vertices y tomando el envolvente eje-alineado.
///        Costo: ~30 ns por entidad. Para 836 entidades del baseline F2H2
///        son ~25 us totales — negligible vs los 42 ms del PBR::staticPass.
///
///        Aviso: el AABB resultante puede ser MAS GRANDE que el local
///        rotado (bounding eje-alineado de un cubo rotado 45° crece). Es
///        el trade-off correcto para culling conservador — preferimos
///        falsos positivos a falsos negativos.
inline AABB worldAabb(const AABB& localAabb, const glm::mat4& worldMatrix) {
    const glm::vec3 corners[8] = {
        {localAabb.min.x, localAabb.min.y, localAabb.min.z},
        {localAabb.max.x, localAabb.min.y, localAabb.min.z},
        {localAabb.min.x, localAabb.max.y, localAabb.min.z},
        {localAabb.max.x, localAabb.max.y, localAabb.min.z},
        {localAabb.min.x, localAabb.min.y, localAabb.max.z},
        {localAabb.max.x, localAabb.min.y, localAabb.max.z},
        {localAabb.min.x, localAabb.max.y, localAabb.max.z},
        {localAabb.max.x, localAabb.max.y, localAabb.max.z},
    };

    const glm::vec3 first = glm::vec3(worldMatrix * glm::vec4(corners[0], 1.0f));
    AABB out{first, first};
    for (int i = 1; i < 8; ++i) {
        const glm::vec3 wp = glm::vec3(worldMatrix * glm::vec4(corners[i], 1.0f));
        out.min = glm::min(out.min, wp);
        out.max = glm::max(out.max, wp);
    }
    return out;
}

} // namespace Mood
