// F2H24 Bloque C: nucleo del MeshLoader (assimp -> MeshAsset). Helpers
// repartidos en archivos parciales:
//   _Skeleton.cpp  — buildNodeMap + buildSkeleton + buildPerVertexInfluences
//   _Geometry.cpp  — defaultAttributes + flattenAiMesh + extractAlbedo +
//                    generateLodFlatVertices
//   _Animation.cpp — parseAnimations
//
// Aca solo vive el orquestador `loadMeshWithAssimp` y el scope para
// el toGlm del rootNode (importRotationEuler).

#include "engine/assets/loaders/MeshLoader.h"
#include "engine/assets/loaders/MeshLoader_Internal.h"

#include "core/Log.h"
#include "engine/assets/cache/LodCache.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/resources/MeshAsset.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/trigonometric.hpp>

#include <filesystem>
#include <limits>

namespace Mood {

std::unique_ptr<MeshAsset> loadMeshWithAssimp(const std::string& logicalPath,
                                                const std::string& filesystemPath,
                                                const MeshFactory& meshFactory,
                                                AssetManager* assetManager) {
    if (!meshFactory) {
        Log::assets()->warn("MeshLoader: no se paso MeshFactory para '{}'", logicalPath);
        return nullptr;
    }

    Assimp::Importer importer;

    // F2H49: configuracion especifica para FBX (especialmente Mixamo).
    // - PRESERVE_PIVOTS=false: sin esto, assimp inserta nodos intermedios
    //   `$AssimpFbx$_Translation/_Rotation/_Scaling` por cada pivote del FBX.
    //   Eso rompe la topologia del esqueleto Mixamo (los bones quedan a 4
    //   niveles de profundidad cada uno) y desincroniza los tracks de
    //   animacion. Apagandolo, assimp colapsa todo al nodo de bone real.
    // - READ_ALL_GEOMETRY_LAYERS=false: Mixamo a veces deja layers UV
    //   extra vacios; nos quedamos con el primario solamente.
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, false);
    // Mixamo exporta en cm (×100). aiProcess_GlobalScale lee el
    // mUnitScaleFactor del header FBX y normaliza a 1.0 unidad = 1 metro.
    // glTF ya viene en metros: el flag es no-op cuando UnitScale es 1.0.
    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);

    // Flags:
    // - Triangulate: garantiza 3 indices por cara (convierte quads/n-gons).
    // - GenNormals: genera normales planas si el modelo no las trae.
    // - CalcTangentSpace: tangentes para normal mapping (Hito 17+).
    // - LimitBoneWeights: assimp recorta a 4 huesos por vertice (matchea
    //   nuestro k_maxInfluencesPerVertex). Igual mantenemos nuestro
    //   trim+normalize defensivo arriba.
    // - GlobalScale (F2H49): aplica AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY * unit
    //   scale del archivo. Mixamo viene en cm → divide entre 100; glTF/m → no-op.
    //
    // Hito 26: REMOVIDO `aiProcess_FlipUVs`. Antes lo usabamos para
    // compensar el origin top-left de glTF/DirectX vs el bottom-left de
    // OpenGL. Pero `OpenGLTexture` YA hace `stbi_set_flip_vertically_on_load(true)`
    // al cargar el PNG: el image flip + el UV flip cancelaban en texturas
    // simetricas (grid, brick) pero rompian palette swatches como el
    // `colormap.png` de Kenney (cada UV apuntaba a un pixel ~negro).
    const u32 flags = aiProcess_Triangulate
                    | aiProcess_GenNormals
                    | aiProcess_CalcTangentSpace
                    | aiProcess_LimitBoneWeights
                    | aiProcess_GlobalScale;

    const aiScene* scene = importer.ReadFile(filesystemPath, flags);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0
        || scene->mRootNode == nullptr) {
        Log::assets()->warn("MeshLoader: fallo '{}' ({})", logicalPath,
                             importer.GetErrorString());
        return nullptr;
    }
    // F2H49: rechazamos archivos que no traen NI mesh NI animacion (ej. un
    // FBX vacio). Si tiene aunque sea animacion sola, seguimos: caso anim-only
    // de Mixamo. Lo expone `loadAnimationClipWithAssimp` (Bloque B) — la ruta
    // de loadMesh sigue retornando nullptr cuando no hay submeshes, pero al
    // menos no aborta y deja al caller de animation-only obtener los clips.
    if (scene->mNumMeshes == 0 && scene->mNumAnimations == 0) {
        Log::assets()->warn("MeshLoader: '{}' no tiene meshes ni animaciones",
                             logicalPath);
        return nullptr;
    }

    // Mapa nodeName -> aiNode* (toda la jerarquia, no solo bones).
    std::unordered_map<std::string, const aiNode*> nodeMap;
    detail::buildNodeMap(scene->mRootNode, nodeMap);

    // Esqueleto compartido por todos los submeshes (convencion glTF/FBX).
    std::optional<Skeleton> skeleton = detail::buildSkeleton(*scene, nodeMap);

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

    // F2H6: guardamos los floats LOD 0 en paralelo para alimentar a
    // meshoptimizer despues del loop. Si el mesh es skinned, este vector
    // se limpia y no se generan LODs.
    std::vector<std::vector<f32>> submeshVerticesLod0;
    submeshVerticesLod0.reserve(scene->mNumMeshes);

    const auto attrs = detail::defaultAttributes();
    for (u32 i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* m = scene->mMeshes[i];
        if (m == nullptr || m->mNumFaces == 0) continue;

        // Influencias por vertice (vacio si el mesh no tiene bones).
        std::vector<std::array<detail::BoneInfluence, detail::k_maxInfluencesPerVertex>> influences;
        if (skeleton.has_value() && m->mNumBones > 0) {
            influences = detail::buildPerVertexInfluences(*m, indexByName);
        }

        auto vertices = detail::flattenAiMesh(*m, influences, aabbMin, aabbMax);
        if (vertices.empty()) continue;

        SubMesh sm{};
        sm.materialIndex = m->mMaterialIndex;
        sm.vertexCount = static_cast<u32>(vertices.size() / detail::k_strideFloats);
        sm.mesh = meshFactory(vertices, attrs);
        if (sm.mesh == nullptr) {
            Log::assets()->warn("MeshLoader: MeshFactory devolvio null para submesh {} de '{}'",
                                 i, logicalPath);
            continue;
        }
        asset->submeshes.push_back(std::move(sm));
        submeshVerticesLod0.push_back(std::move(vertices));
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
        detail::parseAnimations(*scene, indexByName, asset->animations);
    }

    // Hito 26 E: extrae albedo de cada material referenciado. Si el caller
    // no paso AssetManager (tests headless sin GL), saltamos — los meshes
    // quedan sin texturas y los spawn caen al material default.
    if (assetManager != nullptr) {
        asset->materialAlbedoTextures.resize(scene->mNumMaterials, 0);
        asset->materialAlbedoColors.resize(scene->mNumMaterials, std::nullopt);
        for (u32 mi = 0; mi < scene->mNumMaterials; ++mi) {
            const aiMaterial* mat = scene->mMaterials[mi];
            if (mat == nullptr) continue;
            asset->materialAlbedoTextures[mi] =
                detail::extractAlbedo(*scene, *mat, logicalPath, *assetManager);
            // F2H49.1: si el material no trae texture map, intentamos un
            // diffuse color plano (caso template Mixamo X/Y Bot).
            if (asset->materialAlbedoTextures[mi] == 0) {
                asset->materialAlbedoColors[mi] = detail::extractDiffuseColor(*mat);
            }
        }
    }

    // F2H6: generar LODs (1 y 2) para meshes estaticos. Skinned saltea
    // — re-mapear bone weights en mesh simplificado es scope grande con
    // risk de bugs visuales (postergado a hito futuro).
    //
    // Flujo: lookup en cache de disco -> miss -> generar con
    // meshoptimizer -> save cache. El cache vive en
    // assets/.cache/lods/<hash>.moodlod, lateral al formato .moodmap.
    if (!asset->hasSkeleton() && !submeshVerticesLod0.empty()) {
        // mtime + size del source para invalidacion de cache.
        std::error_code ec;
        const auto fsPath = std::filesystem::path(filesystemPath);
        const u64 sourceSize = static_cast<u64>(
            std::filesystem::file_size(fsPath, ec));
        const auto mtime = std::filesystem::last_write_time(fsPath, ec);
        const u64 sourceMtimeNs = static_cast<u64>(
            mtime.time_since_epoch().count());

        const auto cachePath = LodCache::pathFor(logicalPath);
        LodCache::LodCacheEntry cached;
        bool cacheHit = (!ec) && LodCache::tryLoad(
            cachePath, sourceMtimeNs, sourceSize, cached);

        // Si miss, generar. Resultado se persiste para el proximo arranque.
        LodCache::LodCacheEntry generated;
        if (!cacheHit) {
            generated.lod1.reserve(submeshVerticesLod0.size());
            generated.lod2.reserve(submeshVerticesLod0.size());
            for (usize i = 0; i < submeshVerticesLod0.size(); ++i) {
                const auto& srcVerts = submeshVerticesLod0[i];
                const u32 matIndex = asset->submeshes[i].materialIndex;

                LodCache::LodSubmeshData sm1;
                sm1.materialIndex = matIndex;
                sm1.vertices = detail::generateLodFlatVertices(
                    srcVerts, /*ratio=*/0.5f, /*error=*/0.05f);
                sm1.vertexCount = static_cast<u32>(
                    sm1.vertices.size() / detail::k_strideFloats);
                generated.lod1.push_back(std::move(sm1));

                LodCache::LodSubmeshData sm2;
                sm2.materialIndex = matIndex;
                sm2.vertices = detail::generateLodFlatVertices(
                    srcVerts, /*ratio=*/0.15f, /*error=*/0.10f);
                sm2.vertexCount = static_cast<u32>(
                    sm2.vertices.size() / detail::k_strideFloats);
                generated.lod2.push_back(std::move(sm2));
            }
            // Save (best-effort: fallar al escribir no rompe el load).
            if (!ec) {
                LodCache::save(cachePath, sourceMtimeNs, sourceSize, generated);
            }
        }

        const auto& lodSource = cacheHit ? cached : generated;

        // Reconstruir IMesh para LOD 1 y LOD 2. Vertices vacios (mesh
        // muy chico para simplificar) saltean — el submeshesForLod()
        // helper hara fallback a LOD 0 cuando se consulte.
        for (const auto& sm : lodSource.lod1) {
            SubMesh out{};
            out.materialIndex = sm.materialIndex;
            out.vertexCount = sm.vertexCount;
            if (sm.vertexCount > 0) {
                out.mesh = meshFactory(sm.vertices, attrs);
            }
            asset->lod1Submeshes.push_back(std::move(out));
        }
        for (const auto& sm : lodSource.lod2) {
            SubMesh out{};
            out.materialIndex = sm.materialIndex;
            out.vertexCount = sm.vertexCount;
            if (sm.vertexCount > 0) {
                out.mesh = meshFactory(sm.vertices, attrs);
            }
            asset->lod2Submeshes.push_back(std::move(out));
        }

        // Reporte de tris por LOD (solo si hubo generacion util).
        u32 totalLod1 = 0, totalLod2 = 0;
        for (const auto& s : asset->lod1Submeshes) totalLod1 += s.vertexCount / 3;
        for (const auto& s : asset->lod2Submeshes) totalLod2 += s.vertexCount / 3;
        if (totalLod1 > 0 || totalLod2 > 0) {
            Log::assets()->info(
                "[lod] '{}' LOD0={} tris LOD1={} tris LOD2={} tris (cache={})",
                logicalPath, asset->totalVertexCount() / 3, totalLod1, totalLod2,
                cacheHit ? "hit" : "miss");
        }
    }

    // Hito 23: rotacion del rootNode de assimp como Euler XYZ (en
    // grados, con orden YXZ igual que TransformComponent::worldMatrix).
    // glTF Y-up nativos (Fox) traen rootNode = identidad -> 0; modelos
    // Z-up convertidos por glTF (CesiumMan) traen -90° en X. Los spawn
    // paths copian este Euler al entity Transform para orientacion
    // correcta sin hardcodear por modelo.
    const glm::mat4 rootTransform = detail::toGlm(scene->mRootNode->mTransformation);
    {
        float yawRad = 0.0f, pitchRad = 0.0f, rollRad = 0.0f;
        glm::extractEulerAngleYXZ(rootTransform, yawRad, pitchRad, rollRad);
        asset->importRotationEuler = glm::vec3(
            glm::degrees(pitchRad),
            glm::degrees(yawRad),
            glm::degrees(rollRad));
    }

    Log::assets()->info(
        "MeshLoader: '{}' cargado ({} submeshes, {} vertices, {} bones, {} clips, "
        "import rot=({:.1f},{:.1f},{:.1f}))",
        logicalPath, asset->submeshes.size(), asset->totalVertexCount(),
        asset->hasSkeleton() ? asset->skeleton->bones.size() : usize{0},
        asset->animations.size(),
        asset->importRotationEuler.x,
        asset->importRotationEuler.y,
        asset->importRotationEuler.z);
    Log::assets()->debug(
        "MeshLoader: '{}' AABB min=({:.3f},{:.3f},{:.3f}) max=({:.3f},{:.3f},{:.3f})",
        logicalPath,
        asset->aabbMin.x, asset->aabbMin.y, asset->aabbMin.z,
        asset->aabbMax.x, asset->aabbMax.y, asset->aabbMax.z);
    return asset;
}

} // namespace Mood
