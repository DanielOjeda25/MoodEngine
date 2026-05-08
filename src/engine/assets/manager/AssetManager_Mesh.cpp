// F2H24 Bloque C: AssetManager — operaciones sobre meshes (Hito 10).
// loadMesh / getMesh / meshPathOf.

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/assets/loaders/MeshLoader.h"
#include "engine/render/rhi/IMesh.h"  // dtor de SubMesh::mesh (unique_ptr<IMesh>)
#include "engine/render/resources/MeshAsset.h"

#include <utility>

namespace Mood {

MeshAssetId AssetManager::loadMesh(std::string_view logicalPath) {
    const std::string key(logicalPath);
    if (auto it = m_meshCache.find(key); it != m_meshCache.end()) {
        return it->second;
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: mesh path '{}' rechazado por VFS. Fallback a missing (cubo).",
            logicalPath);
        m_meshCache.emplace(key, missingMeshId());
        return missingMeshId();
    }

    auto asset = loadMeshWithAssimp(key, fs.generic_string(), m_meshFactory, this);
    if (asset == nullptr) {
        // Loggeo ya emitido por loadMeshWithAssimp. Cacheamos el fallback
        // para no reintentar cada frame.
        m_meshCache.emplace(key, missingMeshId());
        return missingMeshId();
    }

    const MeshAssetId id = static_cast<MeshAssetId>(m_meshes.size());
    m_meshes.push_back(std::move(asset));
    m_meshCache.emplace(key, id);
    Log::assets()->info("AssetManager: cargado mesh {} -> id {}", logicalPath, id);
    return id;
}

MeshAsset* AssetManager::getMesh(MeshAssetId id) const {
    if (id >= m_meshes.size()) {
        return m_meshes[missingMeshId()].get();
    }
    return m_meshes[id].get();
}

std::string AssetManager::meshPathOf(MeshAssetId id) const {
    if (id >= m_meshes.size()) {
        return m_meshes[missingMeshId()]->logicalPath;
    }
    return m_meshes[id]->logicalPath;
}

} // namespace Mood
