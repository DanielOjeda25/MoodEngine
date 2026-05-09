#include "editor/commands/EditBrushFaceMaterialCommand.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

EditBrushFaceMaterialCommand::EditBrushFaceMaterialCommand(
    Scene* scene,
    std::string entityTag,
    std::vector<u32> faceIndices,
    std::vector<MaterialAssetId> oldMaterials,
    std::vector<u32> oldFaceMatIndices,
    std::vector<MaterialAssetId> newMaterials,
    std::vector<u32> newFaceMatIndices,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_faceIndices(std::move(faceIndices)),
      m_oldMaterials(std::move(oldMaterials)),
      m_oldFaceMatIndices(std::move(oldFaceMatIndices)),
      m_newMaterials(std::move(newMaterials)),
      m_newFaceMatIndices(std::move(newFaceMatIndices)),
      m_label(std::move(label)) {}

EditBrushFaceMaterialCommand::EditBrushFaceMaterialCommand(
    Scene* scene,
    std::string entityTag,
    u32 faceIndex,
    std::vector<MaterialAssetId> oldMaterials,
    u32 oldFaceMatIndex,
    std::vector<MaterialAssetId> newMaterials,
    u32 newFaceMatIndex,
    std::string label)
    : EditBrushFaceMaterialCommand(
        scene,
        std::move(entityTag),
        std::vector<u32>{faceIndex},
        std::move(oldMaterials),
        std::vector<u32>{oldFaceMatIndex},
        std::move(newMaterials),
        std::vector<u32>{newFaceMatIndex},
        std::move(label)) {}

void EditBrushFaceMaterialCommand::execute() {
    apply(m_newMaterials, m_newFaceMatIndices);
}

void EditBrushFaceMaterialCommand::undo() {
    apply(m_oldMaterials, m_oldFaceMatIndices);
}

void EditBrushFaceMaterialCommand::apply(
    const std::vector<MaterialAssetId>& materials,
    const std::vector<u32>& faceMatIndices) {
    if (m_scene == nullptr) return;
    if (m_faceIndices.size() != faceMatIndices.size()) {
        Log::editor()->warn(
            "EditBrushFaceMaterialCommand: tamano mismatch faceIndices={} vs faceMatIndices={}",
            m_faceIndices.size(), faceMatIndices.size());
        return;
    }
    Entity target;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(target) && tag.name == m_entityTag) {
            target = e;
        }
    });
    if (!static_cast<bool>(target)) {
        Log::editor()->warn(
            "EditBrushFaceMaterialCommand: no encontre entidad con tag '{}'",
            m_entityTag);
        return;
    }
    if (!target.hasComponent<BrushComponent>()) {
        Log::editor()->warn(
            "EditBrushFaceMaterialCommand: entidad '{}' sin BrushComponent",
            m_entityTag);
        return;
    }
    auto& bc = target.getComponent<BrushComponent>();
    // Validar TODOS los faceIndex antes de mutar (si alguno esta fuera de
    // rango, no aplicamos parcial — todo o nada).
    for (u32 fi : m_faceIndices) {
        if (fi >= bc.brush.faces.size()) {
            Log::editor()->warn(
                "EditBrushFaceMaterialCommand: faceIndex {} fuera de rango "
                "(brush '{}' tiene {} caras)",
                fi, m_entityTag, bc.brush.faces.size());
            return;
        }
    }
    bc.materials = materials;
    for (size_t i = 0; i < m_faceIndices.size(); ++i) {
        bc.brush.faces[m_faceIndices[i]].materialIndex = faceMatIndices[i];
    }
    bc.dirty = true;
}

} // namespace Mood
