#include "editor/commands/CreateEntityCommand.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"
#include "engine/serialization/EntitySerializer.h"
#include "engine/serialization/SceneLoader.h"

#include <utility>

namespace Mood {

CreateEntityCommand::CreateEntityCommand(std::vector<Entity> created,
                                          Scene* scene,
                                          AssetManager* assets,
                                          BodyCleanup bodyCleanup,
                                          std::string actionLabel)
    : m_scene(scene), m_assets(assets),
      m_bodyCleanup(std::move(bodyCleanup)),
      m_alive(std::move(created)),
      m_label(std::move(actionLabel)) {
    // Capturamos snapshots ahora que las entidades estan completas y
    // vivas. Las que ya no son validas se ignoran (defensa contra
    // callsite buggy que pase entidades destruidas).
    if (m_assets == nullptr) return;
    m_snapshots.reserve(m_alive.size());
    for (const Entity& e : m_alive) {
        if (!static_cast<bool>(e)) continue;
        if (m_scene && !m_scene->registry().valid(e.handle())) continue;
        const auto json = serializeEntityToJson(e, *m_assets);
        m_snapshots.push_back(parseEntityFromJson(json));
    }
}

void CreateEntityCommand::execute() {
    // Discriminador entre "primer push" (callsite ya creo las entidades)
    // y "redo tras undo" (las entidades fueron destruidas): si CUALQUIERA
    // de los handles `m_alive` sigue valido en el registry, asumimos que
    // estamos en el primer push y dejamos las cosas como estan. Si todos
    // estan invalidos (post-undo), recreamos desde snapshots.
    bool anyAlive = false;
    if (m_scene != nullptr) {
        for (const Entity& e : m_alive) {
            if (static_cast<bool>(e) && m_scene->registry().valid(e.handle())) {
                anyAlive = true;
                break;
            }
        }
    }
    if (anyAlive) return;
    recreateFromSnapshots();
}

void CreateEntityCommand::undo() {
    destroyAllAlive();
}

std::string CreateEntityCommand::name() const {
    return m_label;
}

void CreateEntityCommand::onEntityRemap(entt::entity oldH, entt::entity newH) {
    // Hito 32: si alguno de las entidades vivas referenciadas por este
    // comando coincide con oldH, patcheamos su handle. Cubre el caso de
    // que un DeleteEntityCommand anterior haya destruido + recreado una
    // de "nuestras" entidades — un siguiente undo del create necesita
    // apuntar al handle correcto para destruirlas.
    for (auto& e : m_alive) {
        if (e.handle() == oldH) {
            e = Entity(newH, e.scene());
        }
    }
}

void CreateEntityCommand::destroyAllAlive() {
    if (m_scene == nullptr) return;
    for (Entity& e : m_alive) {
        if (!static_cast<bool>(e)) continue;
        if (!m_scene->registry().valid(e.handle())) {
            e = Entity{};
            continue;
        }
        if (m_bodyCleanup && e.hasComponent<RigidBodyComponent>()) {
            auto& rb = e.getComponent<RigidBodyComponent>();
            if (rb.bodyId != 0) {
                m_bodyCleanup(rb.bodyId);
                rb.bodyId = 0;
            }
        }
        m_scene->destroyEntity(e);
        e = Entity{};
    }
    Log::editor()->info("Deshecho '{}' ({} entidad(es)).",
                         m_label, m_snapshots.size());
}

void CreateEntityCommand::recreateFromSnapshots() {
    if (m_scene == nullptr || m_assets == nullptr) {
        Log::editor()->warn("CreateEntityCommand::execute sin Scene/AssetManager");
        return;
    }
    m_alive.clear();
    m_alive.reserve(m_snapshots.size());
    for (const SavedEntity& se : m_snapshots) {
        m_alive.push_back(SceneLoader::applyOneEntity(se, *m_scene, *m_assets));
    }
    Log::editor()->info("Rehecho '{}' ({} entidad(es)).",
                         m_label, m_snapshots.size());
}

} // namespace Mood
