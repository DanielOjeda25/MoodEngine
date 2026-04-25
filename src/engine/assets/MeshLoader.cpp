#include "engine/assets/MeshLoader.h"

#include "core/Log.h"
#include "engine/render/IMesh.h"
#include "engine/render/MeshAsset.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Mood {

namespace {

// Layout interleaved del mesh, igual al cubo primitivo:
// pos(3) + color(3) + uv(2) + normal(3) = stride 11 floats.
// Hito 11: agregamos normales para el iluminado Blinn-Phong.
constexpr u32 k_strideFloats = 11;

std::vector<VertexAttribute> defaultAttributes() {
    return {
        {0, 3}, // position
        {1, 3}, // color
        {2, 2}, // uv
        {3, 3}, // normal
    };
}

// Expande un aiMesh (con indices) a un vertex buffer plano listo para
// glDrawArrays. Triangulos desplegados, un vertice por corner.
std::vector<f32> flattenAiMesh(const aiMesh& m) {
    std::vector<f32> out;
    // Reserva pesimista: 3 vertices por cara * stride floats.
    out.reserve(static_cast<usize>(m.mNumFaces) * 3 * k_strideFloats);

    const bool hasColor   = (m.mColors[0] != nullptr);
    const bool hasUV      = (m.mTextureCoords[0] != nullptr);
    const bool hasNormals = (m.mNormals != nullptr);

    for (u32 f = 0; f < m.mNumFaces; ++f) {
        const aiFace& face = m.mFaces[f];
        // aiProcess_Triangulate garantiza 3 indices por cara. Si no, el
        // formato entro con algo raro y preferimos saltarlo a corromper.
        if (face.mNumIndices != 3) continue;
        for (u32 k = 0; k < 3; ++k) {
            const u32 idx = face.mIndices[k];
            const aiVector3D& p = m.mVertices[idx];
            out.push_back(p.x);
            out.push_back(p.y);
            out.push_back(p.z);

            if (hasColor) {
                const aiColor4D& c = m.mColors[0][idx];
                out.push_back(c.r);
                out.push_back(c.g);
                out.push_back(c.b);
            } else {
                // Blanco neutro: el shader samplea la textura y multiplica
                // por color; blanco = mostrar la textura tal cual.
                out.push_back(1.0f);
                out.push_back(1.0f);
                out.push_back(1.0f);
            }

            if (hasUV) {
                const aiVector3D& uv = m.mTextureCoords[0][idx];
                out.push_back(uv.x);
                out.push_back(uv.y);
            } else {
                out.push_back(0.0f);
                out.push_back(0.0f);
            }

            if (hasNormals) {
                const aiVector3D& n = m.mNormals[idx];
                out.push_back(n.x);
                out.push_back(n.y);
                out.push_back(n.z);
            } else {
                // Sin normales (importer raro o GenNormals fallo): up vector
                // nominal. El iluminado se vera plano pero no crashea.
                out.push_back(0.0f);
                out.push_back(1.0f);
                out.push_back(0.0f);
            }
        }
    }
    return out;
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
    // - GenNormals: genera normales planas si el modelo no las trae. Las
    //   consumira Hito 11 (iluminacion); hoy el shader no las usa.
    // - FlipUVs: assimp usa origin arriba-izq (como DirectX); OpenGL + stb
    //   (flip vertical al cargar la textura) esperan origin abajo-izq. El
    //   flip de UVs compensa.
    // - CalcTangentSpace: tangentes para normal mapping (Hito 11+).
    const u32 flags = aiProcess_Triangulate
                    | aiProcess_GenNormals
                    | aiProcess_FlipUVs
                    | aiProcess_CalcTangentSpace;

    const aiScene* scene = importer.ReadFile(filesystemPath, flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0
        || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
        Log::assets()->warn("MeshLoader: fallo '{}' ({})", logicalPath,
                             importer.GetErrorString());
        return nullptr;
    }

    auto asset = std::make_unique<MeshAsset>();
    asset->logicalPath = logicalPath;
    asset->submeshes.reserve(scene->mNumMeshes);

    const auto attrs = defaultAttributes();
    for (u32 i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* m = scene->mMeshes[i];
        if (m == nullptr || m->mNumFaces == 0) continue;

        auto vertices = flattenAiMesh(*m);
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

    Log::assets()->info("MeshLoader: '{}' cargado ({} submeshes, {} vertices)",
                         logicalPath, asset->submeshes.size(), asset->totalVertexCount());
    return asset;
}

} // namespace Mood
