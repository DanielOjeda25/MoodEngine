#include "engine/assets/PrimitiveMeshes.h"

namespace Mood {

MeshData createCubeMesh() {
    // 36 vertices: 6 caras * 6 vertices (2 triangulos sin EBO). Cada vertice
    // tiene pos(xyz), color(rgb), uv(st). Las caras usan un tinte sutil para
    // que se distingan al rotar; la textura hace el trabajo visual real.
    //
    // Orden de caras (derecha-mano): +X, -X, +Y, -Y, +Z, -Z.
    const std::vector<f32> vertices = {
        // +X (derecha) -- tinte blanco puro
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,

        // -X (izquierda)
        -0.5f, -0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 1.0f,

        // +Y (arriba)
        -0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 1.0f,

        // -Y (abajo)
        -0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 1.0f,

        // +Z (frente)
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,

        // -Z (detras)
         0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 1.0f,
    };

    const std::vector<VertexAttribute> attributes = {
        { 0, 3 }, // aPos
        { 1, 3 }, // aColor
        { 2, 2 }, // aUv
    };

    return MeshData{ vertices, attributes };
}

} // namespace Mood
