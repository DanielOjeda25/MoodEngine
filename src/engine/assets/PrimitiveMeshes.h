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

/// @brief Cubo unitario [-0.5, +0.5]^3 con color por vertice + UVs por cara.
///        36 vertices (6 caras * 2 triangulos * 3 vertices, sin EBO).
///        Orden de atributos: pos(3) + color(3) + uv(2) -> stride 8 floats.
MeshData createCubeMesh();

} // namespace Mood
