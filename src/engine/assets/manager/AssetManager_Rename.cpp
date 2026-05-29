// F3H19: AssetManager::renameLogicalPath — actualiza el mapeo id↔path
// interno de los registries del AssetManager cuando el dev renombra un
// asset desde el editor. El asset NO se recarga ni mueve — solo se
// reescribe el cache. El caller (RenameAssetCommand) mueve el archivo
// en disco antes via std::filesystem::rename.
//
// Detección de familia por extension del path. Para .fbx, distingue
// mesh vs anim clip por el prefijo "anim_" del filename (misma heurística
// que el AssetBrowser).

#include "engine/assets/manager/AssetManager.h"

#include "core/Log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace Mood {

namespace {

std::string toLowerExt(const std::string& path) {
    std::filesystem::path p(path);
    std::string e = p.extension().string();
    std::transform(e.begin(), e.end(), e.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return e;
}

bool stemStartsWithAnim(const std::string& path) {
    std::filesystem::path p(path);
    const std::string stem = p.stem().string();
    return stem.size() >= 5 && stem.compare(0, 5, "anim_") == 0;
}

}  // namespace

bool AssetManager::renameLogicalPath(const std::string& oldPath,
                                       const std::string& newPath) {
    if (oldPath.empty() || newPath.empty()) return false;
    if (oldPath == newPath) return false;

    const std::string ext = toLowerExt(oldPath);

    // Texturas: .png .jpg .jpeg .tga .hdr .bmp.
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga"
        || ext == ".hdr" || ext == ".bmp") {
        if (!m_textures.contains(oldPath)) return false;
        const TextureAssetId id = m_textures.findByPath(oldPath);
        if (id == 0) return false;  // cacheado como fallback
        return m_textures.rename(id, newPath);
    }

    // Audio: .wav .ogg .mp3 .flac.
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        if (!m_audioClips.contains(oldPath)) return false;
        const AudioAssetId id = m_audioClips.findByPath(oldPath);
        if (id == 0) return false;
        return m_audioClips.rename(id, newPath);
    }

    // .fbx: anim clip si el stem arranca con "anim_", sino mesh.
    if (ext == ".fbx") {
        if (stemStartsWithAnim(oldPath)) {
            if (!m_animationClips.contains(oldPath)) return false;
            const AnimationClipAssetId id = m_animationClips.findByPath(oldPath);
            if (id == 0) return false;
            return m_animationClips.rename(id, newPath);
        }
        if (!m_meshes.contains(oldPath)) return false;
        const MeshAssetId id = m_meshes.findByPath(oldPath);
        if (id == 0) return false;
        return m_meshes.rename(id, newPath);
    }

    // Mesh: .obj .gltf .glb.
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
        if (!m_meshes.contains(oldPath)) return false;
        const MeshAssetId id = m_meshes.findByPath(oldPath);
        if (id == 0) return false;
        return m_meshes.rename(id, newPath);
    }

    // Material.
    if (ext == ".material") {
        if (!m_materials.contains(oldPath)) return false;
        const MaterialAssetId id = m_materials.findByPath(oldPath);
        if (id == 0) return false;
        return m_materials.rename(id, newPath);
    }

    // Prefab.
    if (ext == ".moodprefab") {
        if (!m_prefabs.contains(oldPath)) return false;
        const PrefabAssetId id = m_prefabs.findByPath(oldPath);
        if (id == 0) return false;
        return m_prefabs.rename(id, newPath);
    }

    // Dialog.
    if (ext == ".mooddialog") {
        if (!m_dialogs.contains(oldPath)) return false;
        const DialogAssetId id = m_dialogs.findByPath(oldPath);
        if (id == 0) return false;
        return m_dialogs.rename(id, newPath);
    }

    // Item.
    if (ext == ".mooditem") {
        if (!m_items.contains(oldPath)) return false;
        const ItemAssetId id = m_items.findByPath(oldPath);
        if (id == 0) return false;
        return m_items.rename(id, newPath);
    }

    // Quest.
    if (ext == ".moodquest") {
        if (!m_quests.contains(oldPath)) return false;
        const QuestAssetId id = m_quests.findByPath(oldPath);
        if (id == 0) return false;
        return m_quests.rename(id, newPath);
    }

    // Vehicle config.
    if (ext == ".moodvehicle") {
        if (!m_vehicleConfigs.contains(oldPath)) return false;
        const VehicleConfigAssetId id = m_vehicleConfigs.findByPath(oldPath);
        if (id == 0) return false;
        return m_vehicleConfigs.rename(id, newPath);
    }

    // Weapon spec (F4H2).
    if (ext == ".moodweapon") {
        if (!m_weapons.contains(oldPath)) return false;
        const WeaponAssetId id = m_weapons.findByPath(oldPath);
        if (id == 0) return false;
        return m_weapons.rename(id, newPath);
    }

    // .lua: scripts NO se cachean en el AssetManager — viven solo como
    // string en ScriptComponent.path. RenameAssetCommand reescribe los
    // componentes directo; aquí no hay nada que hacer.
    if (ext == ".lua") return false;

    Log::assets()->warn(
        "renameLogicalPath: extension '{}' no reconocida (path '{}'), skip",
        ext, oldPath);
    return false;
}

}  // namespace Mood
