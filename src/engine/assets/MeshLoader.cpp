#include "engine/assets/MeshLoader.h"

#include "core/Log.h"
#include "engine/animation/AnimationClip.h"
#include "engine/animation/Skeleton.h"
#include "engine/render/IMesh.h"
#include "engine/render/MeshAsset.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Mood {

namespace {

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

std::vector<VertexAttribute> defaultAttributes() {
    return {
        {0, 3}, // position
        {1, 3}, // color
        {2, 2}, // uv
        {3, 3}, // normal
        {4, 4}, // boneIds (como floats; el shader los castea a int)
        {5, 4}, // boneWeights
    };
}

// ---------- Helpers de conversion assimp -> glm ----------

glm::mat4 toGlm(const aiMatrix4x4& m) {
    // aiMatrix4x4 es row-major; glm::mat4 es column-major. Hay que
    // transponer al copiar.
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,  // column 0
        m.a2, m.b2, m.c2, m.d2,  // column 1
        m.a3, m.b3, m.c3, m.d3,  // column 2
        m.a4, m.b4, m.c4, m.d4   // column 3
    );
}

glm::vec3 toGlm(const aiVector3D& v) { return glm::vec3(v.x, v.y, v.z); }
glm::quat toGlm(const aiQuaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }

// ---------- Esqueleto: construccion topologica ----------

struct BoneInfluence {
    int boneIndex = 0;
    float weight  = 0.0f;
};

void buildNodeMap(const aiNode* node,
                   std::unordered_map<std::string, const aiNode*>& out) {
    if (node == nullptr) return;
    out[node->mName.C_Str()] = node;
    for (u32 i = 0; i < node->mNumChildren; ++i) {
        buildNodeMap(node->mChildren[i], out);
    }
}

// Topo sort: padres antes que hijos. Asume que `parentMap[name]` apunta a
// otro bone del set (o "" si su padre no es un bone, => root del esqueleto).
std::vector<std::string> topoSortBones(
    const std::unordered_set<std::string>& boneNames,
    const std::unordered_map<std::string, std::string>& parentMap) {

    std::vector<std::string> ordered;
    ordered.reserve(boneNames.size());
    std::unordered_set<std::string> placed;
    placed.reserve(boneNames.size());

    // Iteramos hasta que se coloquen todos. En cada pasada agregamos los
    // que tienen padre ya colocado (o sin padre dentro del esqueleto).
    bool progressed = true;
    while (placed.size() < boneNames.size() && progressed) {
        progressed = false;
        for (const std::string& name : boneNames) {
            if (placed.count(name) != 0) continue;
            auto it = parentMap.find(name);
            const std::string& parent = (it != parentMap.end()) ? it->second
                                                                : std::string();
            if (parent.empty() || placed.count(parent) != 0) {
                ordered.push_back(name);
                placed.insert(name);
                progressed = true;
            }
        }
    }
    // Si hay ciclo (improbable con assimp), volcar lo que falte sin orden:
    // mejor un esqueleto medio raro que crashear el editor.
    for (const std::string& name : boneNames) {
        if (placed.count(name) == 0) ordered.push_back(name);
    }
    return ordered;
}

// Construye el Skeleton del MeshAsset agregando todos los aiBone de todos
// los aiMesh y resolviendo parentesco via la jerarquia de aiNode.
std::optional<Skeleton> buildSkeleton(const aiScene& scene,
                                       const std::unordered_map<std::string, const aiNode*>& nodeMap) {
    // 1. Coleccion de bones unicos por nombre + sus inverseBind.
    std::unordered_map<std::string, glm::mat4> inverseBindByName;
    for (u32 mi = 0; mi < scene.mNumMeshes; ++mi) {
        const aiMesh* m = scene.mMeshes[mi];
        if (m == nullptr) continue;
        for (u32 bi = 0; bi < m->mNumBones; ++bi) {
            const aiBone* b = m->mBones[bi];
            if (b == nullptr) continue;
            const std::string name = b->mName.C_Str();
            if (inverseBindByName.find(name) == inverseBindByName.end()) {
                inverseBindByName[name] = toGlm(b->mOffsetMatrix);
            }
        }
    }
    if (inverseBindByName.empty()) return std::nullopt;

    // 2. Set de nombres de bones para resolver parents.
    std::unordered_set<std::string> boneNames;
    boneNames.reserve(inverseBindByName.size());
    for (const auto& kv : inverseBindByName) boneNames.insert(kv.first);

    // 3. Para cada bone: parent = nombre del aiNode->mParent si tambien es
    //    bone; sino "" (root del skeleton).
    std::unordered_map<std::string, std::string> parentMap;
    parentMap.reserve(boneNames.size());
    for (const std::string& name : boneNames) {
        auto it = nodeMap.find(name);
        if (it == nodeMap.end()) {
            parentMap[name] = "";
            continue;
        }
        const aiNode* node = it->second;
        const aiNode* parent = node->mParent;
        // Subir por la cadena hasta encontrar un bone o la raiz.
        std::string parentBone;
        while (parent != nullptr) {
            const std::string pname = parent->mName.C_Str();
            if (boneNames.count(pname) != 0) {
                parentBone = pname;
                break;
            }
            parent = parent->mParent;
        }
        parentMap[name] = parentBone;
    }

    // 4. Topo sort para que padres tengan indice menor que sus hijos.
    const std::vector<std::string> ordered = topoSortBones(boneNames, parentMap);

    // 5. Mapear nombre -> indice ordenado.
    std::unordered_map<std::string, int> indexByName;
    indexByName.reserve(ordered.size());
    for (usize i = 0; i < ordered.size(); ++i) {
        indexByName[ordered[i]] = static_cast<int>(i);
    }

    // 6. Construir el Skeleton final.
    Skeleton skel;
    skel.bones.reserve(ordered.size());
    for (const std::string& name : ordered) {
        Bone bone;
        bone.name = name;
        bone.inverseBind = inverseBindByName[name];
        const std::string& pname = parentMap[name];
        bone.parent = pname.empty() ? -1 : indexByName[pname];
        // localBindTransform = transformacion local del aiNode (relativa al padre).
        auto it = nodeMap.find(name);
        if (it != nodeMap.end()) {
            bone.localBindTransform = toGlm(it->second->mTransformation);
        }
        skel.bones.push_back(std::move(bone));
    }
    return skel;
}

// Por cada vertice del aiMesh construye una lista de hasta
// k_maxInfluencesPerVertex (boneIdx, weight), sorted desc por peso y
// renormalizada. Vertices sin influencias quedan con boneIdx=0, weight=0.
std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>>
buildPerVertexInfluences(const aiMesh& mesh,
                          const std::unordered_map<std::string, int>& indexByName) {
    std::vector<std::vector<BoneInfluence>> raw(mesh.mNumVertices);
    for (u32 bi = 0; bi < mesh.mNumBones; ++bi) {
        const aiBone* b = mesh.mBones[bi];
        if (b == nullptr) continue;
        auto it = indexByName.find(b->mName.C_Str());
        if (it == indexByName.end()) continue;
        const int boneIndex = it->second;
        for (u32 wi = 0; wi < b->mNumWeights; ++wi) {
            const aiVertexWeight& w = b->mWeights[wi];
            if (w.mVertexId >= mesh.mNumVertices) continue;
            if (w.mWeight <= 0.0f) continue;
            raw[w.mVertexId].push_back({boneIndex, w.mWeight});
        }
    }

    std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>> out(mesh.mNumVertices);
    for (u32 v = 0; v < mesh.mNumVertices; ++v) {
        auto& rs = raw[v];
        // Sort por peso desc y truncar a top-4.
        std::sort(rs.begin(), rs.end(),
                   [](const BoneInfluence& a, const BoneInfluence& b) {
                       return a.weight > b.weight;
                   });
        if (rs.size() > k_maxInfluencesPerVertex) {
            rs.resize(k_maxInfluencesPerVertex);
        }
        // Renormalizar suma a 1.0 (importante: descartamos pesos al
        // truncar y queremos preservar la magnitud relativa).
        float sum = 0.0f;
        for (const auto& inf : rs) sum += inf.weight;
        if (sum > 1e-6f) {
            for (auto& inf : rs) inf.weight /= sum;
        }
        // Volcar a la slot fija de tamano 4. Sobrantes quedan en (0, 0).
        for (usize k = 0; k < rs.size(); ++k) {
            out[v][k] = rs[k];
        }
    }
    return out;
}

// Expande un aiMesh a un vertex buffer plano stride 19. Acumula AABB en
// `outMin`/`outMax`.
std::vector<f32> flattenAiMesh(
    const aiMesh& m,
    const std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>>& influences,
    glm::vec3& outMin, glm::vec3& outMax) {
    std::vector<f32> out;
    out.reserve(static_cast<usize>(m.mNumFaces) * 3 * k_strideFloats);

    const bool hasColor   = (m.mColors[0] != nullptr);
    const bool hasUV      = (m.mTextureCoords[0] != nullptr);
    const bool hasNormals = (m.mNormals != nullptr);
    const bool hasInfl    = !influences.empty();

    for (u32 f = 0; f < m.mNumFaces; ++f) {
        const aiFace& face = m.mFaces[f];
        if (face.mNumIndices != 3) continue;
        for (u32 k = 0; k < 3; ++k) {
            const u32 idx = face.mIndices[k];
            const aiVector3D& p = m.mVertices[idx];
            outMin.x = std::min(outMin.x, p.x);
            outMin.y = std::min(outMin.y, p.y);
            outMin.z = std::min(outMin.z, p.z);
            outMax.x = std::max(outMax.x, p.x);
            outMax.y = std::max(outMax.y, p.y);
            outMax.z = std::max(outMax.z, p.z);

            out.push_back(p.x);
            out.push_back(p.y);
            out.push_back(p.z);

            if (hasColor) {
                const aiColor4D& c = m.mColors[0][idx];
                out.push_back(c.r); out.push_back(c.g); out.push_back(c.b);
            } else {
                out.push_back(1.0f); out.push_back(1.0f); out.push_back(1.0f);
            }

            if (hasUV) {
                const aiVector3D& uv = m.mTextureCoords[0][idx];
                out.push_back(uv.x); out.push_back(uv.y);
            } else {
                out.push_back(0.0f); out.push_back(0.0f);
            }

            if (hasNormals) {
                const aiVector3D& n = m.mNormals[idx];
                out.push_back(n.x); out.push_back(n.y); out.push_back(n.z);
            } else {
                out.push_back(0.0f); out.push_back(1.0f); out.push_back(0.0f);
            }

            // boneIds + boneWeights (8 floats). Si el mesh no trae bones
            // o el vertice quedo sin influencias, todo en 0 — el shader
            // detecta sum(weights) == 0 y cae al pipeline no-skinneado.
            if (hasInfl && idx < influences.size()) {
                const auto& inf = influences[idx];
                for (u32 j = 0; j < k_maxInfluencesPerVertex; ++j) {
                    out.push_back(static_cast<f32>(inf[j].boneIndex));
                }
                for (u32 j = 0; j < k_maxInfluencesPerVertex; ++j) {
                    out.push_back(inf[j].weight);
                }
            } else {
                for (u32 j = 0; j < k_maxInfluencesPerVertex * 2; ++j) {
                    out.push_back(0.0f);
                }
            }
        }
    }
    return out;
}

// ---------- Animaciones ----------

void parseAnimations(const aiScene& scene,
                      const std::unordered_map<std::string, int>& indexByName,
                      std::vector<AnimationClip>& outClips) {
    for (u32 ai = 0; ai < scene.mNumAnimations; ++ai) {
        const aiAnimation* anim = scene.mAnimations[ai];
        if (anim == nullptr) continue;

        // Convencion: si mTicksPerSecond es 0 (algunos exporters), asumir 25.
        const float tps = (anim->mTicksPerSecond > 0.0)
                           ? static_cast<float>(anim->mTicksPerSecond)
                           : 25.0f;

        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        if (clip.name.empty()) clip.name = "anim_" + std::to_string(ai);
        clip.duration = static_cast<float>(anim->mDuration) / tps;
        clip.tracks.reserve(anim->mNumChannels);

        for (u32 ci = 0; ci < anim->mNumChannels; ++ci) {
            const aiNodeAnim* ch = anim->mChannels[ci];
            if (ch == nullptr) continue;
            auto it = indexByName.find(ch->mNodeName.C_Str());
            if (it == indexByName.end()) continue; // canal de un nodo no-bone

            BoneTrack tr;
            tr.boneIndex = it->second;
            tr.positionKeys.reserve(ch->mNumPositionKeys);
            tr.rotationKeys.reserve(ch->mNumRotationKeys);
            tr.scaleKeys.reserve(ch->mNumScalingKeys);

            for (u32 k = 0; k < ch->mNumPositionKeys; ++k) {
                const auto& key = ch->mPositionKeys[k];
                VectorKey v;
                v.time  = static_cast<float>(key.mTime) / tps;
                v.value = toGlm(key.mValue);
                tr.positionKeys.push_back(v);
            }
            for (u32 k = 0; k < ch->mNumRotationKeys; ++k) {
                const auto& key = ch->mRotationKeys[k];
                QuatKey q;
                q.time  = static_cast<float>(key.mTime) / tps;
                q.value = toGlm(key.mValue);
                tr.rotationKeys.push_back(q);
            }
            for (u32 k = 0; k < ch->mNumScalingKeys; ++k) {
                const auto& key = ch->mScalingKeys[k];
                VectorKey v;
                v.time  = static_cast<float>(key.mTime) / tps;
                v.value = toGlm(key.mValue);
                tr.scaleKeys.push_back(v);
            }
            clip.tracks.push_back(std::move(tr));
        }

        outClips.push_back(std::move(clip));
    }
}

} // namespace

std::unique_ptr<MeshAsset> loadMeshWithAssimp(const std::string& logicalPath,
                                                const std::string& filesystemPath,
                                                const MeshFactory& meshFactory) {
    if (!meshFactory) {
        Log::assets()->warn("MeshLoader: no se paso MeshFactory para '{}'", logicalPath);
        return nullptr;
    }

    Assimp::Importer importer;
    // Flags:
    // - Triangulate: garantiza 3 indices por cara (convierte quads/n-gons).
    // - GenNormals: genera normales planas si el modelo no las trae.
    // - FlipUVs: assimp usa origin arriba-izq (como DirectX); OpenGL + stb
    //   esperan origin abajo-izq.
    // - CalcTangentSpace: tangentes para normal mapping (Hito 17+).
    // - LimitBoneWeights: assimp recorta a 4 huesos por vertice (matchea
    //   nuestro k_maxInfluencesPerVertex). Igual mantenemos nuestro
    //   trim+normalize defensivo arriba.
    const u32 flags = aiProcess_Triangulate
                    | aiProcess_GenNormals
                    | aiProcess_FlipUVs
                    | aiProcess_CalcTangentSpace
                    | aiProcess_LimitBoneWeights;

    const aiScene* scene = importer.ReadFile(filesystemPath, flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0
        || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
        Log::assets()->warn("MeshLoader: fallo '{}' ({})", logicalPath,
                             importer.GetErrorString());
        return nullptr;
    }

    // Mapa nodeName -> aiNode* (toda la jerarquia, no solo bones).
    std::unordered_map<std::string, const aiNode*> nodeMap;
    buildNodeMap(scene->mRootNode, nodeMap);

    // Esqueleto compartido por todos los submeshes (convencion glTF/FBX).
    std::optional<Skeleton> skeleton = buildSkeleton(*scene, nodeMap);

    // Mapa para resolver bone indices al construir influencias y tracks.
    std::unordered_map<std::string, int> indexByName;
    if (skeleton.has_value()) {
        indexByName.reserve(skeleton->bones.size());
        for (usize i = 0; i < skeleton->bones.size(); ++i) {
            indexByName[skeleton->bones[i].name] = static_cast<int>(i);
        }
    }

    auto asset = std::make_unique<MeshAsset>();
    asset->logicalPath = logicalPath;
    asset->submeshes.reserve(scene->mNumMeshes);

    glm::vec3 aabbMin( std::numeric_limits<float>::max());
    glm::vec3 aabbMax(-std::numeric_limits<float>::max());

    const auto attrs = defaultAttributes();
    for (u32 i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* m = scene->mMeshes[i];
        if (m == nullptr || m->mNumFaces == 0) continue;

        // Influencias por vertice (vacio si el mesh no tiene bones).
        std::vector<std::array<BoneInfluence, k_maxInfluencesPerVertex>> influences;
        if (skeleton.has_value() && m->mNumBones > 0) {
            influences = buildPerVertexInfluences(*m, indexByName);
        }

        auto vertices = flattenAiMesh(*m, influences, aabbMin, aabbMax);
        if (vertices.empty()) continue;

        SubMesh sm{};
        sm.materialIndex = m->mMaterialIndex;
        sm.vertexCount = static_cast<u32>(vertices.size() / k_strideFloats);
        sm.mesh = meshFactory(vertices, attrs);
        if (sm.mesh == nullptr) {
            Log::assets()->warn("MeshLoader: MeshFactory devolvio null para submesh {} de '{}'",
                                 i, logicalPath);
            continue;
        }
        asset->submeshes.push_back(std::move(sm));
    }

    if (asset->submeshes.empty()) {
        Log::assets()->warn("MeshLoader: '{}' no tiene submeshes utiles tras flattening",
                             logicalPath);
        return nullptr;
    }

    // AABB final. Si nunca se acumulo (caso degenerado), mantener default.
    if (aabbMin.x <= aabbMax.x && aabbMin.y <= aabbMax.y && aabbMin.z <= aabbMax.z) {
        asset->aabbMin = aabbMin;
        asset->aabbMax = aabbMax;
    }

    // Esqueleto + animaciones (si los hay).
    if (skeleton.has_value()) {
        asset->skeleton = std::move(skeleton);
        parseAnimations(*scene, indexByName, asset->animations);
    }

    Log::assets()->info("MeshLoader: '{}' cargado ({} submeshes, {} vertices, {} bones, {} clips)",
                         logicalPath, asset->submeshes.size(), asset->totalVertexCount(),
                         asset->hasSkeleton() ? asset->skeleton->bones.size() : usize{0},
                         asset->animations.size());
    return asset;
}

} // namespace Mood
