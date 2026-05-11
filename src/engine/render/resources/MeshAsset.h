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
#include "engine/animation/clips/AnimationClip.h"
#include "engine/animation/skeleton/Skeleton.h"

#include <glm/vec2.hpp>
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

    /// F2H6: niveles LOD adicionales generados con meshoptimizer al
    /// cargar el mesh. Vacios = no hay LOD (fallback a `submeshes` que
    /// es LOD 0). Skinned meshes los dejan vacios — el re-mapping de
    /// bone weights es scope grande con risk de bugs visuales y se
    /// posterga a hito futuro.
    std::vector<SubMesh> lod1Submeshes;
    std::vector<SubMesh> lod2Submeshes;

    /// F2H6: distancias camera→entity para transicionar entre LODs.
    /// `.x` = LOD 0 → 1, `.y` = LOD 1 → 2. Defaults sensatos para
    /// escenas tipicas; override per-mesh si una medicion futura lo
    /// justifica (UI editor para cambiarlo es hito futuro).
    glm::vec2 lodDistances{30.0f, 80.0f};

    /// AABB del bind pose en mesh-space. Si los submeshes no aportaron
    /// vertices (caso degenerado), queda en (-0.5, +0.5) — cubo unitario
    /// de seguridad para que el outline del Hito 16 no implosione.
    glm::vec3 aabbMin{-0.5f};
    glm::vec3 aabbMax{ 0.5f};

    /// Hito 23: rotacion del rootNode de assimp en grados (orden YXZ
    /// igual que TransformComponent). Para glTF Y-up nativos (Fox.glb)
    /// queda en 0; para modelos Z-up convertidos por glTF (CesiumMan)
    /// trae -90° en X. Los spawn paths la copian a la entidad para
    /// que CualquierMesh.glb spawnee con la orientacion correcta sin
    /// hardcodear por personaje.
    glm::vec3 importRotationEuler{0.0f};

    /// Esqueleto (Hito 19). Vacio para meshes estaticos.
    std::optional<Skeleton> skeleton;

    /// Animaciones empaquetadas en el archivo (Hito 19). Vacio para meshes
    /// estaticos. El primer clip es el "default" cuando el `AnimatorComponent`
    /// no especifica uno.
    std::vector<AnimationClip> animations;

    /// Hito 26 E: TextureAssetId por cada material referenciado en el archivo
    /// (size = scene->mNumMaterials). 0 = no se pudo extraer (material sin
    /// diffuse, o textura corrupta). Los spawn paths leen
    /// `materialAlbedoTextures[sub.materialIndex]` para crear materiales con
    /// la textura correcta del archivo. Cubre embedded (`*0` en glb) y
    /// external (path relativo a la carpeta del archivo).
    using TextureAssetId = u32;
    std::vector<TextureAssetId> materialAlbedoTextures;

    /// F2H49.1: diffuse/base color por cada material cuando NO hay texture map.
    /// Tipico caso de los rigs template de Mixamo (X Bot, Y Bot) — sus
    /// materiales tienen `aiColor4D` directo (gris/blanco/negro) y 0 texturas.
    /// `nullopt` = material sin color real → `createMaterialsForMesh` cae al
    /// missing-texture magenta como warning visual. Algun valor → el material
    /// se crea con `albedoTint = *color` + `useAlbedoMap = false`.
    /// Paralelo a `materialAlbedoTextures` (size = scene->mNumMaterials).
    std::vector<std::optional<glm::vec3>> materialAlbedoColors;

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

    /// F2H6: devuelve la lista de submeshes para el LOD pedido.
    /// Fallback automatico a LOD 0 si el nivel pedido esta vacio
    /// (mesh sin LODs generados, o skinned). Garantiza que el caller
    /// SIEMPRE recibe una lista valida — no hay que chequear empty.
    const std::vector<SubMesh>& submeshesForLod(u32 lod) const {
        if (lod == 1 && !lod1Submeshes.empty()) return lod1Submeshes;
        if (lod == 2 && !lod2Submeshes.empty()) return lod2Submeshes;
        return submeshes;
    }
};

/// @brief Selecciona el LOD apropiado para una distancia camera→entidad
///        dada los thresholds de un MeshAsset. PURO: no toca state
///        global, no llama a meshoptimizer. Util para tests + para que
///        SceneRenderer y groupByBatch usen la misma logica.
///
///        Convencion:
///          distance < thresholds.x          -> LOD 0 (full detail)
///          thresholds.x <= d < thresholds.y -> LOD 1 (~50%)
///          thresholds.y <= d                -> LOD 2 (~15%)
inline u32 selectLod(f32 distance, const glm::vec2& thresholds) {
    if (distance < thresholds.x) return 0;
    if (distance < thresholds.y) return 1;
    return 2;
}

} // namespace Mood
