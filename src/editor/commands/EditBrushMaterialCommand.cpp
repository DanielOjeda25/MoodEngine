#include "editor/commands/EditBrushMaterialCommand.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

EditBrushMaterialCommand::EditBrushMaterialCommand(
    Scene* scene,
    std::string entityTag,
    MaterialAssetId oldMat,
    MaterialAssetId newMat,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_oldMat(oldMat),
      m_newMat(newMat),
      m_label(std::move(label)) {}

void EditBrushMaterialCommand::execute() {
    applyMaterial(m_newMat);
}

void EditBrushMaterialCommand::undo() {
    applyMaterial(m_oldMat);
}

void EditBrushMaterialCommand::applyMaterial(MaterialAssetId mat) {
    if (m_scene == nullptr) return;
    Entity target;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(target) && tag.name == m_entityTag) {
            target = e;
        }
    });
    if (!static_cast<bool>(target)) {
        Log::editor()->warn(
            "EditBrushMaterialCommand: no encontre entidad con tag '{}'",
            m_entityTag);
        return;
    }
    if (!target.hasComponent<BrushComponent>()) {
        Log::editor()->warn(
            "EditBrushMaterialCommand: entidad '{}' sin BrushComponent",
            m_entityTag);
        return;
    }
    auto& bc = target.getComponent<BrushComponent>();
    bc.material = mat;
    bc.dirty = true;
}

} // namespace Mood
