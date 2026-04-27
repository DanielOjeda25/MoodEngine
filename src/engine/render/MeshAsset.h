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
// - Layout de vertices: pos(3) + color(3) + uv(2) + normal(3) + boneIds(4) +
//   boneWeights(4) = stride 19 floats. Los meshes sin esqueleto guardan
//   boneIds=0 y boneWeights=0 — el shader skinneable los detecta por la
//   suma de pesos = 0 y cae al pipeline normal. Los imports expanden el
//   index buffer a triangulos planos (sin EBO) para matchear
//   `OpenGLRenderer::drawMesh`. Cuando llegue la optimizacion indexada,
//   migrar IMesh + este struct juntos.
// - `materialIndex` apunta al array de texturas del `MeshRendererComponent`
//   (1 textura por submesh). Fallback al slot 0 del AssetManager si el array
//   es mas corto que el numero de submeshes.
// - Hito 19: opcionalmente trae `Skeleton` + lista de `AnimationClip`. Los
//   meshes sin huesos (cubo, esfera, props OBJ) los dejan vacios.
// - `aabbMin`/`aabbMax` son del bind pose en mesh-space (espacio local del
//   modelo, antes del Transform de la entidad). Para meshes animados,
//   `AnimationSystem` aplica un margen porque la pose en runtime puede
//   exceder el bind.

#include "core/Types.h"
#include "engine/animation/AnimationClip.h"
#include "engine/animation/Skeleton.h"

#include <glm/vec3.hpp>

#include <memory>
#include <optional>
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

    /// AABB del bind pose en mesh-space. Si los submeshes no aportaron
    /// vertices (caso degenerado), queda en (-0.5, +0.5) — cubo unitario
    /// de seguridad para que el outline del Hito 16 no implosione.
    glm::vec3 aabbMin{-0.5f};
    glm::vec3 aabbMax{ 0.5f};

    /// Esqueleto (Hito 19). Vacio para meshes estaticos.
    std::optional<Skeleton> skeleton;

    /// Animaciones empaquetadas en el archivo (Hito 19). Vacio para meshes
    /// estaticos. El primer clip es el "default" cuando el `AnimatorComponent`
    /// no especifica uno.
    std::vector<AnimationClip> animations;

    /// Suma de vertices de todos los submeshes. Uso: logs, Inspector.
    u32 totalVertexCount() const {
        u32 n = 0;
        for (const auto& s : submeshes) n += s.vertexCount;
        return n;
    }

    /// true si tiene huesos parseados (no implica que tenga animaciones).
    bool hasSkeleton() const {
        return skeleton.has_value() && !skeleton->empty();
    }
};

} // namespace Mood
