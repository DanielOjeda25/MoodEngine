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

// --- F3H27: helpers de jerarquia parent/child ---

glm::mat4 Scene::worldMatrixOf(entt::entity handle) const {
    if (handle == entt::null) return glm::mat4(1.0f);
    if (!m_registry.valid(handle)) return glm::mat4(1.0f);
    if (!m_registry.all_of<TransformComponent>(handle)) return glm::mat4(1.0f);

    // Walk iterativo desde la entity hacia el root acumulando local matrices
    // en stack. Multiplicamos del root hacia abajo al final. Iterativo para
    // evitar stack overflow con cadenas profundas (32+ niveles); el clamp
    // tambien protege ante ciclos accidentales (parent → ... → handle).
    constexpr int kMaxDepth = 32;
    glm::mat4 chain[kMaxDepth];
    int count = 0;
    entt::entity cur = handle;
    while (cur != entt::null && count < kMaxDepth) {
        if (!m_registry.valid(cur)) break;
        if (!m_registry.all_of<TransformComponent>(cur)) break;
        const auto& tc = m_registry.get<TransformComponent>(cur);
        chain[count++] = tc.worldMatrix();
        // Mismo entity como padre = ciclo trivial, abortar.
        if (tc.parent == cur) break;
        cur = tc.parent;
    }
    // Multiplicar desde el root (chain[count-1]) hacia el entity (chain[0]).
    // El padre transforma al hijo: world = parent_world * child_local.
    glm::mat4 m(1.0f);
    for (int i = count - 1; i >= 0; --i) {
        m = m * chain[i];
    }
    return m;
}

std::vector<entt::entity> Scene::descendantsOf(entt::entity root) const {
    std::vector<entt::entity> out;
    if (root == entt::null) return out;
    if (!m_registry.valid(root)) return out;

    // DFS pre-order. Snapshot del view porque destruir children invalida
    // iteradores — el caller (cascade delete) borra despues de copiar.
    std::vector<entt::entity> stack{root};
    while (!stack.empty()) {
        const entt::entity cur = stack.back();
        stack.pop_back();
        // Buscar todos los entities cuyo parent == cur.
        auto view = m_registry.view<const TransformComponent>();
        for (auto h : view) {
            if (h == cur) continue;
            if (view.get<const TransformComponent>(h).parent == cur) {
                out.push_back(h);
                stack.push_back(h);
            }
        }
    }
    return out;
}

std::vector<entt::entity> Scene::topLevelAncestors(
    const std::vector<entt::entity>& selected) const {

    if (selected.empty()) return {};
    // Set de selectos para lookup O(1).
    std::vector<entt::entity> selectedSorted = selected;
    // Helper: handle X es "top-level del set" si NINGUN ancestro suyo
    // (parent, grandparent, ...) tambien esta en el set.
    auto inSet = [&](entt::entity h) {
        for (entt::entity s : selectedSorted) {
            if (s == h) return true;
        }
        return false;
    };
    std::vector<entt::entity> out;
    out.reserve(selected.size());
    for (entt::entity h : selected) {
        if (h == entt::null || !m_registry.valid(h)) continue;
        if (!m_registry.all_of<TransformComponent>(h)) {
            out.push_back(h);
            continue;
        }
        // Walk arriba hasta root o hasta encontrar un ancestro en el set.
        constexpr int kMaxDepth = 32;
        entt::entity cur = m_registry.get<TransformComponent>(h).parent;
        bool hasAncestorInSet = false;
        int depth = 0;
        while (cur != entt::null && depth++ < kMaxDepth) {
            if (!m_registry.valid(cur)) break;
            if (inSet(cur)) { hasAncestorInSet = true; break; }
            if (!m_registry.all_of<TransformComponent>(cur)) break;
            cur = m_registry.get<TransformComponent>(cur).parent;
        }
        if (!hasAncestorInSet) out.push_back(h);
    }
    return out;
}

bool Scene::isAncestorOf(entt::entity ancestor, entt::entity descendant) const {
    if (ancestor == entt::null || descendant == entt::null) return false;
    if (ancestor == descendant) return false;
    if (!m_registry.valid(descendant)) return false;

    constexpr int kMaxDepth = 32;
    entt::entity cur = descendant;
    int depth = 0;
    while (cur != entt::null && depth++ < kMaxDepth) {
        if (!m_registry.valid(cur)) return false;
        if (!m_registry.all_of<TransformComponent>(cur)) return false;
        const auto& tc = m_registry.get<TransformComponent>(cur);
        if (tc.parent == ancestor) return true;
        if (tc.parent == cur) return false;  // ciclo trivial
        cur = tc.parent;
    }
    return false;
}

} // namespace Mood
