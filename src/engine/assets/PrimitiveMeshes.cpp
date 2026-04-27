#include "engine/assets/PrimitiveMeshes.h"

#include <cmath>

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

MeshData createSphereMesh(u32 segments) {
    // UV sphere: parametros (u, v) -> (longitud, latitud).
    //   x =  r * cos(2*pi*u) * sin(pi*v)
    //   y =  r * cos(pi*v)            (eje Y como pole)
    //   z =  r * sin(2*pi*u) * sin(pi*v)
    // Generamos triangulos sin EBO (matchea el layout del cubo). Color
    // blanco uniforme: el material setea albedoTint en el shader.
    const u32 longSegs = (segments < 4) ? 4u : segments;
    const u32 latSegs  = longSegs / 2u; // medio del meridiano
    const f32 radius   = 0.5f;
    const f32 pi       = 3.14159265358979f;

    auto pushVertex = [&](std::vector<f32>& out, f32 u, f32 v) {
        const f32 phi   = 2.0f * pi * u;
        const f32 theta = pi * v;
        const f32 sinT  = std::sin(theta);
        const f32 cosT  = std::cos(theta);
        const f32 nx = cosT * 0.0f + std::cos(phi) * sinT; // = cos(phi)*sin(theta)
        const f32 ny = cosT;
        const f32 nz = std::sin(phi) * sinT;
        const f32 px = radius * nx;
        const f32 py = radius * ny;
        const f32 pz = radius * nz;
        out.insert(out.end(), {
            px, py, pz,           // pos
            1.0f, 1.0f, 1.0f,     // color (blanco; albedoTint domina)
            u, 1.0f - v,          // uv (V flip estandar)
            nx, ny, nz,           // normal (= pos normalizada)
        });
    };

    std::vector<f32> vertices;
    vertices.reserve(longSegs * latSegs * 6 * 11);

    for (u32 j = 0; j < latSegs; ++j) {
        const f32 v0 = static_cast<f32>(j)     / static_cast<f32>(latSegs);
        const f32 v1 = static_cast<f32>(j + 1) / static_cast<f32>(latSegs);
        for (u32 i = 0; i < longSegs; ++i) {
            const f32 u0 = static_cast<f32>(i)     / static_cast<f32>(longSegs);
            const f32 u1 = static_cast<f32>(i + 1) / static_cast<f32>(longSegs);
            // Quad (u0,v0)-(u1,v0)-(u1,v1)-(u0,v1) -> 2 tris.
            // Orden CCW visto desde fuera: (u0,v0)->(u0,v1)->(u1,v0) y
            // (u1,v0)->(u0,v1)->(u1,v1) — convencion derecha-mano.
            pushVertex(vertices, u0, v0);
            pushVertex(vertices, u0, v1);
            pushVertex(vertices, u1, v0);
            pushVertex(vertices, u1, v0);
            pushVertex(vertices, u0, v1);
            pushVertex(vertices, u1, v1);
        }
    }

    const std::vector<VertexAttribute> attributes = {
        { 0, 3 },
        { 1, 3 },
        { 2, 2 },
        { 3, 3 },
    };
    return MeshData{ vertices, attributes };
}

} // namespace Mood
