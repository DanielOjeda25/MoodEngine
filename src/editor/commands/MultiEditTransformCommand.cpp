#include "editor/commands/MultiEditTransformCommand.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"

#include <cmath>
#include <utility>

namespace Mood {

namespace {

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b) {
    constexpr f32 eps = 1e-5f;
    return std::abs(a.x - b.x) < eps
        && std::abs(a.y - b.y) < eps
        && std::abs(a.z - b.z) < eps;
}

} // namespace

MultiEditTransformCommand::MultiEditTransformCommand(
    EditTransformCommand::Field field,
    std::vector<Entry> entries)
    : m_field(field), m_entries(std::move(entries)) {}

bool MultiEditTransformCommand::isNoOp() const {
    for (const auto& e : m_entries) {
        if (!nearlyEqual(e.before, e.after)) return false;
    }
    return true;
}

void MultiEditTransformCommand::execute() {
    for (const auto& e : m_entries) {
        applyValue(e, e.after);
    }
}

void MultiEditTransformCommand::undo() {
    for (const auto& e : m_entries) {
        applyValue(e, e.before);
    }
}

void MultiEditTransformCommand::onEntityRemap(entt::entity oldH,
                                                  entt::entity newH) {
    for (auto& e : m_entries) {
        if (e.entity.handle() == oldH) {
            e.entity = Entity(newH, e.entity.scene());
        }
    }
}

std::string MultiEditTransformCommand::name() const {
    const char* fieldName = "";
    switch (m_field) {
        case EditTransformCommand::Field::Position: fieldName = "Mover"; break;
        case EditTransformCommand::Field::Rotation: fieldName = "Rotar"; break;
        case EditTransformCommand::Field::Scale:    fieldName = "Escalar"; break;
    }
    return std::string(fieldName) + " " + std::to_string(m_entries.size())
           + " entidad(es)";
}

void MultiEditTransformCommand::applyValue(const Entry& e,
                                              const glm::vec3& v) {
    Scene* scene = e.entity.scene();
    if (scene == nullptr || !scene->registry().valid(e.entity.handle())) {
        // Skip silenciosamente: la entidad fue destruida tras el push.
        return;
    }
    if (!e.entity.hasComponent<TransformComponent>()) return;
    Entity ent = e.entity;
    auto& tf = ent.getComponent<TransformComponent>();
    switch (m_field) {
        case EditTransformCommand::Field::Position: tf.position      = v; break;
        case EditTransformCommand::Field::Rotation: tf.rotationEuler = v; break;
        case EditTransformCommand::Field::Scale:    tf.scale         = v; break;
    }
}

} // namespace Mood
