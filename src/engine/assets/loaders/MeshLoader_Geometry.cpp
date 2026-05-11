// F2H24 Bloque C: helpers de geometria del MeshLoader.
// flattenAiMesh — expande aiMesh a vertex buffer plano stride 19.
// extractAlbedo — resuelve la textura del slot diffuse / albedo de un
//   aiMaterial (embedded o external).
// generateLodFlatVertices — simplifica un buffer flat usando
//   meshoptimizer (dedup -> simplify -> re-expandir).

#include "engine/assets/loaders/MeshLoader_Internal.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"

#include <meshoptimizer.h>

#include <algorithm>
#include <filesystem>

namespace Mood::detail {

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

TextureAssetId extractAlbedo(const aiScene& scene,
                              const aiMaterial& mat,
                              const std::string& meshLogicalPath,
                              AssetManager& am) {
    aiString aiPath;
    // Probamos primero BASE_COLOR (PBR/glTF), despues DIFFUSE (legacy/OBJ).
    aiReturn ok = mat.GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
    if (ok != AI_SUCCESS) {
        ok = mat.GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
    }
    if (ok != AI_SUCCESS || aiPath.length == 0) return 0;

    const std::string path = aiPath.C_Str();

    // Caso embedded: assimp marca con prefijo "*<idx>". También cubre
    // glb donde la textura embedded tiene un nombre tipo "*0".
    if (!path.empty() && path[0] == '*') {
        const aiTexture* embedded = scene.GetEmbeddedTexture(path.c_str());
        if (embedded == nullptr) {
            Log::assets()->warn(
                "MeshLoader: ref embedded '{}' no encontrada en '{}'",
                path, meshLogicalPath);
            return 0;
        }
        // mHeight==0 => textura comprimida (PNG/JPG), pcData de tamano mWidth.
        // mHeight>0 => raw RGBA pixels (no soportado por simplicidad).
        if (embedded->mHeight != 0) {
            Log::assets()->warn(
                "MeshLoader: textura embedded '{}' en '{}' es raw (mHeight={}); "
                "solo soportamos comprimidas (PNG/JPG).",
                path, meshLogicalPath, embedded->mHeight);
            return 0;
        }
        std::vector<u8> bytes(
            reinterpret_cast<const u8*>(embedded->pcData),
            reinterpret_cast<const u8*>(embedded->pcData) + embedded->mWidth);
        const std::string cacheKey = meshLogicalPath + "#" + path;
        return am.loadEmbeddedTexture(cacheKey, bytes);
    }

    // Caso external: resolver relativo a la carpeta del mesh.
    // meshLogicalPath ej "meshes/kenney_survival/barrel.glb"
    // path ej "Textures/colormap.png"
    // -> "meshes/kenney_survival/Textures/colormap.png"
    //
    // Hito 35 C: lexically_normal() colapsa segmentos `..`. Ej OBJ con
    // `.mtl` + `map_Kd ../textures/brick.png` produce "meshes/../textures/
    // brick.png" que el VFS rechaza por leak. Normalizar lo deja en
    // "textures/brick.png" — el VFS lo acepta como path lógico válido.
    //
    // F2H49: FBXs (especialmente de Mixamo) a veces guardan el path
    // absoluto del filesystem del autor original: "C:\Users\Mixamo\textures\
    // ch11_body.png" o "/Users/joe/Desktop/textures/...". Esos paths no
    // existen en nuestra VFS — si detectamos que es absoluto, caemos al
    // basename y buscamos relativo a la carpeta del mesh.
    std::filesystem::path texPath(path);
    if (texPath.is_absolute()) {
        texPath = texPath.filename();  // "C:\foo\bar\diff.png" -> "diff.png"
    }
    const auto baseDir = std::filesystem::path(meshLogicalPath).parent_path();
    const auto resolved = (baseDir / texPath).lexically_normal().generic_string();
    return am.loadTexture(resolved);
}

std::vector<f32> generateLodFlatVertices(const std::vector<f32>& sourceFlat,
                                            f32 reductionRatio,
                                            f32 errorRatio) {
    constexpr usize kStride = k_strideFloats;
    const usize sourceVertexCount = sourceFlat.size() / kStride;
    if (sourceVertexCount < 300) {
        return {};  // mesh muy chico — sin LOD
    }

    // Indices implicitos 0..N (mesh flat = un index por vertice).
    std::vector<u32> sourceIndices(sourceVertexCount);
    for (usize i = 0; i < sourceVertexCount; ++i) {
        sourceIndices[i] = static_cast<u32>(i);
    }

    // Dedup: meshoptimizer trabaja sobre (indexed mesh) + un index buffer.
    std::vector<u32> remap(sourceVertexCount);
    const usize uniqueCount = meshopt_generateVertexRemap(
        remap.data(), sourceIndices.data(), sourceIndices.size(),
        sourceFlat.data(), sourceVertexCount, kStride * sizeof(f32));

    std::vector<f32> dedupedVertices(uniqueCount * kStride);
    std::vector<u32> dedupedIndices(sourceIndices.size());
    meshopt_remapVertexBuffer(dedupedVertices.data(), sourceFlat.data(),
                                sourceVertexCount, kStride * sizeof(f32),
                                remap.data());
    meshopt_remapIndexBuffer(dedupedIndices.data(), sourceIndices.data(),
                               sourceIndices.size(), remap.data());

    // Simplify. meshopt_simplify trabaja con position en los primeros
    // 3 floats del vertice (offset 0) — nuestro layout matchea.
    const usize targetIndexCount = static_cast<usize>(
        static_cast<f32>(dedupedIndices.size()) * reductionRatio);
    std::vector<u32> simplifiedIndices(dedupedIndices.size());
    f32 resultError = 0.0f;
    const usize newIndexCount = meshopt_simplify(
        simplifiedIndices.data(), dedupedIndices.data(), dedupedIndices.size(),
        dedupedVertices.data(), uniqueCount, kStride * sizeof(f32),
        targetIndexCount, errorRatio, /*options=*/0u, &resultError);
    simplifiedIndices.resize(newIndexCount);

    // Si el simplify no logro reducir significativamente (<10%), no vale
    // la pena agregar el LOD. Devuelve vacio = "sin LOD".
    if (newIndexCount > dedupedIndices.size() * 9 / 10) {
        return {};
    }

    // Re-expandir a flat (un vertice por indice consecutivo).
    std::vector<f32> result;
    result.reserve(newIndexCount * kStride);
    for (u32 idx : simplifiedIndices) {
        const f32* src = &dedupedVertices[idx * kStride];
        result.insert(result.end(), src, src + kStride);
    }
    return result;
}

} // namespace Mood::detail
