#pragma once

// F2H24 Bloque C: helpers compartidos por los archivos parciales del
// MeshLoader (MeshLoader.cpp + MeshLoader_Skeleton.cpp +
// MeshLoader_Geometry.cpp + MeshLoader_Animation.cpp). Header privado
// del modulo — no incluir desde otro modulo.

#include "core/Types.h"
#include "engine/animation/clips/AnimationClip.h"
#include "engine/animation/skeleton/Skeleton.h"
#include "engine/assets/manager/AssetManager.h"  // TextureAssetId alias
#include "engine/render/rhi/RendererTypes.h"

#include <assimp/scene.h>
#include <assimp/types.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mood {

namespace detail {

// Layout interleaved del mesh:
//   pos(3) + color(3) + uv(2) + normal(3) + boneIds(4) + boneWeights(4) = 19.
//
// Hito 19: agregamos boneIds + boneWeights a TODOS los meshes importados
// por assimp. Los meshes sin esqueleto los emiten en 0 (los 8 floats
// finales). El shader skinneable los consume; el `pbr.vert` normal los
// ignora (no declara las locations 4 y 5). El overhead es +32 bytes por
// vertice — despreciable para los volumenes que manejamos.
constexpr u32 k_strideFloats = 19;
constexpr u32 k_maxInfluencesPerVertex = 4;

struct BoneInfluence {
    int boneIndex = 0;
    float weight  = 0.0f;
};

inline glm::mat4 toGlm(const aiMatrix4x4& m) {
    // aiMatrix4x4 es row-major; glm::mat4 es column-major. Hay que
    // transponer al copiar.
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,  // column 0
        m.a2, m.b2, m.c2, m.d2,  // column 1
        m.a3, m.b3, m.c3, m.d3,  // column 2
        m.a4, m.b4, m.c4, m.d4   // column 3
    );
}

inline glm::vec3 toGlm(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
inline glm::quat toGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

std::vector<VertexAttribute> defaultAttributes();

// Skeleton (definidas en MeshLoader_Skeleton.cpp).
void buildNodeMap(const aiNode* node,
                   std::unordered_map<std::string, const aiNode*>& out);

std::optional<Skeleton> buildSkeleton(const aiScene& scene,
                                        const std::unordered_map<std::string, const aiNode*>& nodeMap);

std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>>
buildPerVertexInfluences(const aiMesh& mesh,
                           const std::unordered_map<std::string, int>& indexByName);

// Geometry (definidas en MeshLoader_Geometry.cpp).
std::vector<f32> flattenAiMesh(
    const aiMesh& m,
    const std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>>& influences,
    glm::vec3& outMin, glm::vec3& outMax);

TextureAssetId extractAlbedo(const aiScene& scene,
                              const aiMaterial& mat,
                              const std::string& meshLogicalPath,
                              AssetManager& am);

/// @brief F2H49.1: extrae el diffuse/base color del material cuando no hay
///        texture map. Devuelve nullopt si el material no expone color o
///        el color es blanco puro default (sentinel "sin color real").
std::optional<glm::vec3> extractDiffuseColor(const aiMaterial& mat);

std::vector<f32> generateLodFlatVertices(const std::vector<f32>& sourceFlat,
                                            f32 reductionRatio,
                                            f32 errorRatio);

// Animation (definidas en MeshLoader_Animation.cpp).

/// @brief Sanitiza nombres feos de clips ("Armature|mixamo.com" -> "mixamo_com").
///        Expuesto para reuso desde el loader standalone (F2H49).
std::string sanitizeClipName(std::string raw);

/// @brief Convierte un `aiNodeAnim` a `BoneTrack`. `boneName` se llena
///        siempre con el nombre del nodo. `boneIndex` queda en -1 — el
///        caller lo resuelve (parseAnimations contra el skeleton del mesh
///        embedded; el loader standalone lo deja en -1 para bind pass).
BoneTrack convertChannel(const aiNodeAnim& ch, float tps);

void parseAnimations(const aiScene& scene,
                      const std::unordered_map<std::string, int>& indexByName,
                      std::vector<AnimationClip>& outClips);

} // namespace detail

} // namespace Mood
