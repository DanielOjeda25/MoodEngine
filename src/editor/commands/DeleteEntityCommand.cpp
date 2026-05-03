#include "editor/commands/DeleteEntityCommand.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/EntitySerializer.h"
#include "engine/serialization/SceneLoader.h"

namespace Mood {

DeleteEntityCommand::DeleteEntityCommand(Entity entity,
                                          Scene* scene,
                                          AssetManager* assets,
                                          BodyCleanup bodyCleanup,
                                          HistoryStack* history)
    : m_scene(scene), m_assets(assets),
      m_bodyCleanup(std::move(bodyCleanup)),
      m_history(history),
      m_alive(entity),
      m_originalHandle(entity.handle()) {
    // Capturamos el snapshot AHORA, mientras la entity todavia existe.
    // serializeEntityToJson + parseEntityFromJson nos da una SavedEntity
    // independiente del handle vivo.
    if (m_assets != nullptr && static_cast<bool>(m_alive)) {
        const auto json = serializeEntityToJson(m_alive, *m_assets);
        m_snapshot = parseEntityFromJson(json);
    }
}

void DeleteEntityCommand::execute() {
    destroyAlive();
}

void DeleteEntityCommand::undo() {
    if (m_scene == nullptr || m_assets == nullptr) {
        Log::editor()->warn("DeleteEntityCommand::undo sin Scene/AssetManager");
        return;
    }
    // Recrea desde el snapshot. El handle EnTT NUEVO no coincide con el
    // viejo — por eso m_alive se reasigna.
    m_alive = SceneLoader::applyOneEntity(m_snapshot, *m_scene, *m_assets);
    Log::editor()->info("Recreada entidad '{}' (undo de delete).", m_snapshot.tag);

    // Hito 32: notificar al history stack del cambio de handle. Comandos
    // previos (EditTransform, EditProperty, otros Create/Delete) que
    // referenciaban la entidad por el handle viejo se patchean aca.
    if (m_history != nullptr) {
        m_history->remapEntityInStack(m_originalHandle, m_alive.handle());
    }
}

std::string DeleteEntityCommand::name() const {
    return "Eliminar '" + m_snapshot.tag + "'";
}

void DeleteEntityCommand::destroyAlive() {
    if (m_scene == nullptr || !static_cast<bool>(m_alive)) return;
    if (!m_scene->registry().valid(m_alive.handle())) {
        m_alive = Entity{};
        return;
    }

    // Cleanup del body de Jolt antes del destroyEntity (mismo flujo que
    // EditorApplication::deleteSelectedEntity). Via callback inyectado
    // para no acoplar el comando con PhysicsWorld (tests pasan {}).
    if (m_bodyCleanup && m_alive.hasComponent<RigidBodyComponent>()) {
        auto& rb = m_alive.getComponent<RigidBodyComponent>();
        if (rb.bodyId != 0) {
            m_bodyCleanup(rb.bodyId);
            rb.bodyId = 0;
        }
    }
    m_scene->destroyEntity(m_alive);
    m_alive = Entity{};
    Log::editor()->info("Eliminada entidad '{}'.", m_snapshot.tag);
}

} // namespace Mood
