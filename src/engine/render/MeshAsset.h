#pragma once

// MeshAsset (Hito 10): representacion de un modelo 3D importado.
//
// Un modelo puede tener varios submeshes: un coche tiene carroceria + ruedas +
// vidrios, cada uno con su material distinto. assimp los separa automatico
// cuando importa un archivo (uno por `aiMesh` en la escena).
//
// Ownership: cada SubMesh posee su propio `IMesh` (VAO/VBO). El MeshAsset los
// agrupa bajo un solo logicalPath para servir al serializer.
//
// Convenciones:
// - Layout de vertices igual al del cubo del Hito 3: pos(3) + color(3) + uv(2),
//   stride 8 floats. Los imports expanden el index buffer a triangulos planos
//   (sin EBO) para matchear lo que espera `OpenGLRenderer::drawMesh`. Cuando
//   llegue la optimizacion indexada, migrar IMesh + este struct juntos.
// - `materialIndex` apunta al array de texturas del `MeshRendererComponent`
//   (1 textura por submesh). Fallback al slot 0 del AssetManager si el array
//   es mas corto que el numero de submeshes.

#include "core/Types.h"

#include <memory>
#include <string>
#include <vector>

namespace Mood {

class IMesh;

struct SubMesh {
    std::unique_ptr<IMesh> mesh;
    u32 materialIndex = 0;
    u32 vertexCount = 0;
};

struct MeshAsset {
    /// Path logico usado por el serializer (p.ej. "meshes/suzanne.obj").
    std::string logicalPath;
    std::vector<SubMesh> submeshes;

    /// Suma de vertices de todos los submeshes. Uso: logs, Inspector.
    u32 totalVertexCount() const {
        u32 n = 0;
        for (const auto& s : submeshes) n += s.vertexCount;
        return n;
    }
};

} // namespace Mood
