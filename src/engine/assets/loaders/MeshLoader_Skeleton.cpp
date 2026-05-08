// F2H24 Bloque C: helpers de esqueleto del MeshLoader (assimp ->
// engine Skeleton). buildNodeMap (recoleccion de jerarquia),
// topoSortBones (padres antes que hijos), buildSkeleton (construye el
// Skeleton final con inverseBind matrices), buildPerVertexInfluences
// (top-4 boneIds + weights normalizados por vertice).

#include "engine/assets/loaders/MeshLoader_Internal.h"

#include <algorithm>
#include <unordered_set>

namespace Mood::detail {

void buildNodeMap(const aiNode* node,
                   std::unordered_map<std::string, const aiNode*>& out) {
    if (node == nullptr) return;
    out[node->mName.C_Str()] = node;
    for (u32 i = 0; i < node->mNumChildren; ++i) {
        buildNodeMap(node->mChildren[i], out);
    }
}

namespace {

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

} // namespace

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

} // namespace Mood::detail
