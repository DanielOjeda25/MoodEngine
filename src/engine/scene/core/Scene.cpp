#include "engine/scene/core/Scene.h"

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"

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
    if (!m_registry.valid(entity.handle())) return;
    // F2H82: si es chasis de un vehiculo, llevarse tambien las 4 wheel-entities
    // que VehicleSystem auto-spawnea (no estan en el undo stack como entidades
    // independientes; Ctrl+Z borraba el chasis y las ruedas quedaban huerfanas
    // en la escena, sin chasis al cual referir). Hacemos el cleanup acá para
    // que sea invariante: cualquier delete del chasis = delete del set.
    if (m_registry.all_of<VehicleComponent>(entity.handle())) {
        const auto& veh = m_registry.get<VehicleComponent>(entity.handle());
        for (u32 wh : veh.wheelEntities) {
            if (wh == 0) continue;
            const auto we = static_cast<entt::entity>(wh);
            if (m_registry.valid(we)) m_registry.destroy(we);
        }
    }
    m_registry.destroy(entity.handle());
}

Entity Scene::makeEntity(entt::entity handle) {
    return Entity(handle, this);
}

Entity Scene::entityFromHandle(entt::entity handle) {
    if (handle == entt::null) return Entity{};
    if (!m_registry.valid(handle)) return Entity{};
    return makeEntity(handle);
}

// --- F2H33: VisGroups ---

VisGroup* Scene::findVisGroup(u64 id) {
    if (id == 0) return nullptr;
    for (auto& vg : m_visgroups) {
        if (vg.id == id) return &vg;
    }
    return nullptr;
}

const VisGroup* Scene::findVisGroup(u64 id) const {
    if (id == 0) return nullptr;
    for (const auto& vg : m_visgroups) {
        if (vg.id == id) return &vg;
    }
    return nullptr;
}

VisGroup& Scene::addVisGroup(std::string name, glm::vec3 color) {
    // Generar id libre (lineal — VisGroups raramente > 100).
    u64 nextId = 1;
    while (findVisGroup(nextId) != nullptr) ++nextId;

    VisGroup vg;
    vg.id = nextId;
    vg.name = std::move(name);
    vg.color = color;
    vg.hidden = false;
    m_visgroups.push_back(std::move(vg));
    return m_visgroups.back();
}

VisGroup& Scene::insertVisGroup(VisGroup vg) {
    if (vg.id == 0) {
        // id=0 es sentinel "sin grupo"; promocionar a 1+.
        u64 nextId = 1;
        while (findVisGroup(nextId) != nullptr) ++nextId;
        vg.id = nextId;
    } else if (auto* existing = findVisGroup(vg.id); existing != nullptr) {
        *existing = std::move(vg);
        return *existing;
    }
    m_visgroups.push_back(std::move(vg));
    return m_visgroups.back();
}

void Scene::removeVisGroup(u64 id) {
    if (id == 0) return;
    for (auto it = m_visgroups.begin(); it != m_visgroups.end(); ++it) {
        if (it->id == id) {
            m_visgroups.erase(it);
            return;
        }
    }
}

} // namespace Mood
