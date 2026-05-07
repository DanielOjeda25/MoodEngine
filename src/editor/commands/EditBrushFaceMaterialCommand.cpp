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
    u32 faceIndex,
    std::vector<MaterialAssetId> oldMaterials,
    u32 oldFaceMatIndex,
    std::vector<MaterialAssetId> newMaterials,
    u32 newFaceMatIndex,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_faceIndex(faceIndex),
      m_oldMaterials(std::move(oldMaterials)),
      m_oldFaceMatIndex(oldFaceMatIndex),
      m_newMaterials(std::move(newMaterials)),
      m_newFaceMatIndex(newFaceMatIndex),
      m_label(std::move(label)) {}

void EditBrushFaceMaterialCommand::execute() {
    apply(m_newMaterials, m_newFaceMatIndex);
}

void EditBrushFaceMaterialCommand::undo() {
    apply(m_oldMaterials, m_oldFaceMatIndex);
}

void EditBrushFaceMaterialCommand::apply(
    const std::vector<MaterialAssetId>& materials,
    u32 faceMatIndex) {
    if (m_scene == nullptr) return;
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
    if (m_faceIndex >= bc.brush.faces.size()) {
        Log::editor()->warn(
            "EditBrushFaceMaterialCommand: faceIndex {} fuera de rango "
            "(brush '{}' tiene {} caras)",
            m_faceIndex, m_entityTag, bc.brush.faces.size());
        return;
    }
    bc.materials = materials;
    bc.brush.faces[m_faceIndex].materialIndex = faceMatIndex;
    bc.dirty = true;
}

} // namespace Mood
