#include "editor/commands/EditBrushGeometryCommand.h"

#include "core/Log.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

#include <utility>

namespace Mood {

EditBrushGeometryCommand::EditBrushGeometryCommand(
    Scene* scene,
    std::string entityTag,
    std::vector<Plane> beforePlanes,
    std::vector<Plane> afterPlanes,
    glm::vec3 tfPositionBefore,
    glm::vec3 tfPositionAfter,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_before(std::move(beforePlanes)),
      m_after(std::move(afterPlanes)),
      m_tfPosBefore(tfPositionBefore),
      m_tfPosAfter(tfPositionAfter),
      m_label(std::move(label)) {}

void EditBrushGeometryCommand::execute() {
    applyState(m_after, m_tfPosAfter);
}

void EditBrushGeometryCommand::undo() {
    applyState(m_before, m_tfPosBefore);
}

bool EditBrushGeometryCommand::isNoOp() const {
    if (m_tfPosBefore != m_tfPosAfter) return false;
    if (m_before.size() != m_after.size()) return false;
    for (usize i = 0; i < m_before.size(); ++i) {
        if (m_before[i].normal != m_after[i].normal) return false;
        if (m_before[i].distance != m_after[i].distance) return false;
    }
    return true;
}

void EditBrushGeometryCommand::applyState(const std::vector<Plane>& planes,
                                           const glm::vec3& tfPos) {
    if (m_scene == nullptr) return;
    Entity target;
    m_scene->forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (!static_cast<bool>(target) && tag.name == m_entityTag) {
            target = e;
        }
    });
    if (!static_cast<bool>(target)) {
        Log::editor()->warn(
            "EditBrushGeometryCommand: no encontre entidad con tag '{}'",
            m_entityTag);
        return;
    }
    if (!target.hasComponent<BrushComponent>() ||
        !target.hasComponent<TransformComponent>()) {
        Log::editor()->warn(
            "EditBrushGeometryCommand: entidad '{}' sin BrushComponent o "
            "TransformComponent", m_entityTag);
        return;
    }
    auto& bc = target.getComponent<BrushComponent>();
    auto& tf = target.getComponent<TransformComponent>();
    if (planes.size() != bc.brush.faces.size()) {
        Log::editor()->warn(
            "EditBrushGeometryCommand: count mismatch — snapshot tiene "
            "{} planos, brush tiene {} caras (no aplico)",
            planes.size(), bc.brush.faces.size());
        return;
    }
    for (usize i = 0; i < planes.size(); ++i) {
        bc.brush.faces[i].plane = planes[i];
    }
    tf.position = tfPos;
    bc.brush.localAabb = Csg::computeBrushAabb(bc.brush);
    bc.dirty = true;
}

} // namespace Mood
