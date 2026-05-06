#include "editor/commands/EditMeshRendererMaterialCommand.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

EditMeshRendererMaterialCommand::EditMeshRendererMaterialCommand(
    Scene* scene,
    std::string entityTag,
    u32 slotIndex,
    MaterialAssetId oldMat,
    MaterialAssetId newMat,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_slotIndex(slotIndex),
      m_oldMat(oldMat),
      m_newMat(newMat),
      m_label(std::move(label)) {}

void EditMeshRendererMaterialCommand::execute() {
    applyMaterial(m_newMat);
}

void EditMeshRendererMaterialCommand::undo() {
    applyMaterial(m_oldMat);
}

void EditMeshRendererMaterialCommand::applyMaterial(MaterialAssetId mat) {
    if (m_scene == nullptr) return;
    Entity target;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(target) && tag.name == m_entityTag) {
            target = e;
        }
    });
    if (!static_cast<bool>(target) ||
        !target.hasComponent<MeshRendererComponent>()) {
        Log::editor()->warn(
            "EditMeshRendererMaterialCommand: '{}' invalida o sin MeshRenderer",
            m_entityTag);
        return;
    }
    auto& mr = target.getComponent<MeshRendererComponent>();
    if (mr.materials.size() <= m_slotIndex) {
        // Asegurar que el slot exista. Si el slot 0 no existe lo
        // creamos (caso del drop sobre mesh recien spawneada que
        // arranca con materials vacio).
        mr.materials.resize(m_slotIndex + 1, 0);
    }
    mr.materials[m_slotIndex] = mat;
}

} // namespace Mood
