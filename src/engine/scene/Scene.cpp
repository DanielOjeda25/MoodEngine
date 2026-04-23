#include "engine/scene/Scene.h"

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"

#include <string>

namespace Mood {

Scene::Scene() = default;
Scene::~Scene() = default;

Entity Scene::createEntity(std::string_view name) {
    const entt::entity handle = m_registry.create();
    m_registry.emplace<TagComponent>(handle, std::string{name.empty() ? "Entity" : name});
    m_registry.emplace<TransformComponent>(handle);
    return makeEntity(handle);
}

void Scene::destroyEntity(Entity entity) {
    if (!entity) return;
    if (m_registry.valid(entity.handle())) {
        m_registry.destroy(entity.handle());
    }
}

Entity Scene::makeEntity(entt::entity handle) {
    return Entity(handle, this);
}

} // namespace Mood
