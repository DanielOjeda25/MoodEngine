#include "editor/commands/EditTransformCommand.h"

#include "core/Log.h"
#include "engine/scene/Components.h"
#include "engine/scene/Scene.h"

#include <cmath>

namespace Mood {

namespace {

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b) {
    constexpr f32 eps = 1e-5f;
    return std::abs(a.x - b.x) < eps
        && std::abs(a.y - b.y) < eps
        && std::abs(a.z - b.z) < eps;
}

} // namespace

EditTransformCommand::EditTransformCommand(Entity entity, Field field,
                                              glm::vec3 before, glm::vec3 after)
    : m_entity(entity), m_field(field), m_before(before), m_after(after) {}

bool EditTransformCommand::isNoOp() const {
    return nearlyEqual(m_before, m_after);
}

void EditTransformCommand::execute() {
    applyValue(m_after);
}

void EditTransformCommand::undo() {
    applyValue(m_before);
}

std::string EditTransformCommand::name() const {
    const char* fieldName = "";
    switch (m_field) {
        case Field::Position: fieldName = "Mover"; break;
        case Field::Rotation: fieldName = "Rotar"; break;
        case Field::Scale:    fieldName = "Escalar"; break;
    }
    if (m_entity.scene() != nullptr
        && m_entity.scene()->registry().valid(m_entity.handle())
        && m_entity.hasComponent<TagComponent>()) {
        return std::string(fieldName) + " '" +
               m_entity.getComponent<TagComponent>().name + "'";
    }
    return std::string(fieldName) + " entidad";
}

void EditTransformCommand::applyValue(const glm::vec3& v) {
    Scene* scene = m_entity.scene();
    if (scene == nullptr || !scene->registry().valid(m_entity.handle())) {
        Log::editor()->warn(
            "EditTransformCommand: entidad ya no existe (probablemente "
            "destruida tras el push). Skipping undo/redo.");
        return;
    }
    if (!m_entity.hasComponent<TransformComponent>()) {
        Log::editor()->warn(
            "EditTransformCommand: entidad sin TransformComponent. Skipping.");
        return;
    }
    auto& tf = m_entity.getComponent<TransformComponent>();
    switch (m_field) {
        case Field::Position: tf.position      = v; break;
        case Field::Rotation: tf.rotationEuler = v; break;
        case Field::Scale:    tf.scale         = v; break;
    }
}

} // namespace Mood
