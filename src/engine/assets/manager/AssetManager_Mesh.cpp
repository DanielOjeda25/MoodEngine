// F2H24 Bloque C: AssetManager — operaciones sobre meshes (Hito 10).
// loadMesh / getMesh / meshPathOf.
//
// break-B5: storage delegado a AssetRegistry<MeshAsset>. meshPathOf
// devuelve el path agregado al registry (en sync con MeshAsset.logicalPath
// que el loader rellena con el mismo string).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"
#include "engine/assets/loaders/MeshLoader.h"
#include "engine/render/rhi/IMesh.h"  // dtor de SubMesh::mesh (unique_ptr<IMesh>)
#include "engine/render/resources/MeshAsset.h"

#include <utility>

namespace Mood {

MeshAssetId AssetManager::loadMesh(std::string_view logicalPath) {
    if (m_meshes.contains(logicalPath)) {
        return m_meshes.findByPath(logicalPath);
    }

    const auto fs = m_vfs.resolve(logicalPath);
    if (fs.empty()) {
        Log::assets()->warn(
            "AssetManager: mesh path '{}' rechazado por VFS. Fallback a missing (cubo).",
            logicalPath);
        m_meshes.cacheAsFallback(logicalPath);
        return missingMeshId();
    }

    const std::string key{logicalPath};
    auto asset = loadMeshWithAssimp(key, fs.generic_string(), m_meshFactory, this);
    if (asset == nullptr) {
        // Loggeo ya emitido por loadMeshWithAssimp. Cacheamos el fallback
        // para no reintentar cada frame.
        m_meshes.cacheAsFallback(logicalPath);
        return missingMeshId();
    }

    const MeshAssetId id = m_meshes.add(key, std::move(asset));
    Log::assets()->info("AssetManager: cargado mesh {} -> id {}", logicalPath, id);
    return id;
}

MeshAsset* AssetManager::getMesh(MeshAssetId id) const {
    // break-B5: misma loophole que getAnimationClip / getMaterial.
    const auto& v = m_meshes.all();
    if (v.empty()) return nullptr;
    if (id >= v.size()) return v[0].get();
    return v[id].get();
}

std::string AssetManager::meshPathOf(MeshAssetId id) const {
    // Pre-break leia `m_meshes[id]->logicalPath`; el registry mantiene el
    // string en paralelo, mismo valor (el caller del add pasa la misma
    // cadena que se asigna a MeshAsset.logicalPath en el loader).
    return m_meshes.pathOf(id);
}

} // namespace Mood
