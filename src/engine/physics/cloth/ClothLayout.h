#pragma once

// F2H75 Bloque A: layout de una tela (cloth) generada proceduralmente como
// grilla NxM de particulas unidas por resortes. Header puro (sin Jolt) —
// usable headless en tests.
//
// Concepto: una tela mass-spring estandar. Cada particula es un punto de
// masa; los resortes la unen a sus vecinas en 3 familias:
//   - structural: vecinos directos (horizontal/vertical) — da la forma base.
//   - shear:      diagonales — resiste el corte (que la celda colapse en
//                 rombo).
//   - bend:       vecinos a 2 de distancia — resiste el doblez (que la tela
//                 se pliegue sobre si misma).
//
// El layout es DECLARATIVO (geometria + topologia + flags de anclaje, sin
// estado runtime). El PhysicsWorld lo consume al crear el soft body para
// armar los `JPH::SoftBodySharedSettings` reales en Jolt; el ClothSystem
// usa `triangleIndices` para armar el mesh dinamico que se renderiza.
//
// Convencion de espacio LOCAL:
//   - Plano XY (Z = 0). La tela "mira" hacia +Z/-Z (se renderiza doble cara).
//   - X en [-width/2, +width/2] (ancho).
//   - Y en [+height/2, -height/2]: la fila 0 es el BORDE SUPERIOR. Asi la
//     gravedad (-Y) la cuelga natural si se ancla arriba.
//   - Indexado row-major: idx = y*resX + x.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <vector>

namespace Mood::cloth {

/// @brief Que borde/esquinas de la tela quedan ancladas (fijas en el aire).
///        Las particulas ancladas tienen masa inversa 0 en Jolt (no caen).
enum class AnchorEdge : u8 {
    None       = 0,  ///< Nada anclado: la tela cae libre (paracaidas, trapo).
    TopEdge    = 1,  ///< Borde superior completo (cortina, bandera colgante).
    TopCorners = 2,  ///< Solo las 2 esquinas de arriba (guirnalda, tendedero).
    LeftEdge   = 3,  ///< Borde izquierdo completo (bandera en asta vertical).
};

/// @brief Familia de un resorte. Jolt usa una compliance global por edge,
///        pero guardamos el tipo por si en el futuro queremos rigidez
///        distinta por familia (structural rigido, bend blando, etc.).
enum class EdgeKind : u8 {
    Structural = 0,
    Shear      = 1,
    Bend       = 2,
};

/// @brief Una particula de la grilla.
struct ClothParticle {
    glm::vec3 localPos{0.0f};  ///< Posicion en espacio local del cloth.
    bool      pinned = false;  ///< true = anclada (invMass 0 en Jolt).
};

/// @brief Un resorte entre dos particulas (indices en `particles`).
struct ClothEdge {
    int      a = 0;
    int      b = 0;
    f32      restLength = 0.0f;  ///< Distancia de reposo (longitud inicial).
    EdgeKind kind = EdgeKind::Structural;
};

/// @brief Layout completo de la tela. `particles` en orden row-major
///        (y*resX + x). `triangleIndices` tiene 3 indices por triangulo,
///        2 triangulos por celda de la grilla.
struct ClothLayout {
    int resX = 0;  ///< Particulas por lado horizontal (>= 2).
    int resY = 0;  ///< Particulas por lado vertical (>= 2).

    std::vector<ClothParticle> particles;     ///< resX*resY particulas.
    std::vector<ClothEdge>     edges;         ///< structural + shear + bend.
    std::vector<u32>           triangleIndices;  ///< 3 por triangulo.

    int  vertexCount() const { return static_cast<int>(particles.size()); }
    bool empty() const { return particles.empty(); }

    /// @brief Indice row-major de la particula (x, y) en una grilla de `resX`.
    static int idx(int x, int y, int resX) { return y * resX + x; }
};

/// @brief Construye una tela rectangular como grilla NxM de particulas.
///
/// @param width   Ancho en metros (eje X local). Clampeado a > 0.
/// @param height  Alto en metros (eje Y local). Clampeado a > 0.
/// @param resX    Particulas a lo ancho. Clampeado a >= 2.
/// @param resY    Particulas a lo alto. Clampeado a >= 2.
/// @param anchor  Que parte queda fija (ver `AnchorEdge`).
///
/// @return Layout con particulas + resortes (structural/shear/bend) +
///         indices de triangulos. Las particulas ancladas segun `anchor`
///         quedan con `pinned = true`.
ClothLayout buildGridCloth(f32 width, f32 height, int resX, int resY,
                            AnchorEdge anchor);

} // namespace Mood::cloth
