#pragma once

// Generadores de mallas primitivas para debug / desarrollo temprano.
// Los datos se devuelven como CPU-side; el backend decide como subirlos a GPU.

#include "engine/render/RendererTypes.h"
#include "core/Types.h"

#include <vector>

namespace Mood {

struct MeshData {
    /// Vertices interleaved segun `attributes`.
    std::vector<f32> vertices;
    /// Layout (suma de componentCount = stride por vertice).
    std::vector<VertexAttribute> attributes;
};

/// @brief Cubo unitario [-0.5, +0.5]^3 con color por vertice + UVs por cara
///        + normales planas por cara (Hito 11: el iluminado lo necesita).
///        36 vertices (6 caras * 2 triangulos * 3 vertices, sin EBO).
///        Orden de atributos: pos(3) + color(3) + uv(2) + normal(3) -> stride 11.
MeshData createCubeMesh();

/// @brief UV sphere de radio 0.5 centrada en el origen (Hito 17). Geometria
///        triangulada sin EBO para matchear el layout del cubo. Util para
///        debug visual del PBR (los esferos muestran el dot(N, L) en
///        toda la superficie a la vez).
/// @param segments Cantidad de divisiones longitudinales (alrededor del
///        eje Y). El paralelo es `segments / 2`.
MeshData createSphereMesh(u32 segments = 32);

} // namespace Mood
