#include "engine/assets/PrimitiveMeshes.h"

namespace Mood {

MeshData createCubeMesh() {
    // 36 vertices: 6 caras * 6 vertices (2 triangulos sin EBO). Cada vertice
    // tiene pos(xyz), color(rgb), uv(st), normal(nx,ny,nz).
    // Normales por cara (no interpoladas), para que las paredes/piso se
    // sombreen con caras "duras" estilo Wolfenstein/AAA-2010.
    //
    // Orden de caras (derecha-mano): +X, -X, +Y, -Y, +Z, -Z.
    const std::vector<f32> vertices = {
        // +X (derecha) -- normal (1, 0, 0)
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f,

        // -X (izquierda) -- normal (-1, 0, 0)
        -0.5f, -0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.9f, 0.9f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.9f, 0.9f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f,

        // +Y (arriba) -- normal (0, 1, 0)
        -0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.95f, 1.0f, 0.95f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,

        // -Y (abajo) -- normal (0, -1, 0)
        -0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.9f, 0.9f, 0.95f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,

        // +Z (frente) -- normal (0, 0, 1)
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f,

        // -Z (detras) -- normal (0, 0, -1)
         0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.9f, 0.95f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
    };

    const std::vector<VertexAttribute> attributes = {
        { 0, 3 }, // aPos
        { 1, 3 }, // aColor
        { 2, 2 }, // aUv
        { 3, 3 }, // aNormal
    };

    return MeshData{ vertices, attributes };
}

} // namespace Mood
