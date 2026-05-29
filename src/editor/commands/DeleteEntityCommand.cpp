#include "editor/commands/DeleteEntityCommand.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"  // F3H29: snapshot del brush
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/EntitySerializer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"  // F3H29: serializeBrush/parseBrush

namespace Mood {

DeleteEntityCommand::DeleteEntityCommand(Entity entity,
                                          Scene* scene,
                                          AssetManager* assets,
                                          BodyCleanup bodyCleanup,
                                          HistoryStack* history,
                                          ConstraintCleanup constraintCleanup,
                                          RagdollCleanup ragdollCleanup)
    : m_scene(scene), m_assets(assets),
      m_bodyCleanup(std::move(bodyCleanup)),
      m_constraintCleanup(std::move(constraintCleanup)),
      m_ragdollCleanup(std::move(ragdollCleanup)),
      m_history(history),
      m_alive(entity),
      m_originalHandle(entity.handle()) {
    // Capturamos el snapshot AHORA, mientras la entity todavia existe.
    // serializeEntityToJson + parseEntityFromJson nos da una SavedEntity
    // independiente del handle vivo.
    if (m_assets != nullptr && static_cast<bool>(m_alive)) {
        const auto json = serializeEntityToJson(m_alive, *m_assets);
        m_snapshot = parseEntityFromJson(json);
        // F3H29 bugfix: serializeEntityToJson NO incluye BrushComponent
        // (los brushes se persisten en otro array del .moodmap via
        // serializeBrush). Si la entity es un brush, capturamos el
        // SavedBrush en paralelo — el undo lo aplicará sobre la entity
        // recreada. Sin esto, el dev borraba un brush, hacía Ctrl+Z y
        // recuperaba la entity vacía sin caras.
        if (m_alive.hasComponent<BrushComponent>()) {
            const auto brushJson = serializeBrush(m_alive, *m_assets);
            m_brushSnapshot = parseBrush(brushJson);
            m_hasBrush = true;
        }
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
    // F3H29 bugfix: si era un brush, restaurar la geometría CSG +
    // materiales encima de la entity recreada. applyBrushFromSaved
    // detecta que el BrushComponent no existe aún y lo agrega.
    if (m_hasBrush && static_cast<bool>(m_alive)) {
        SceneLoader::applyBrushFromSaved(
            m_brushSnapshot, m_alive, *m_assets,
            /*applyVisGroupMembership=*/true);
    }
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
    // F2H65: cleanup del constraint si la entidad tiene JointComponent.
    if (m_constraintCleanup && m_alive.hasComponent<JointComponent>()) {
        auto& joint = m_alive.getComponent<JointComponent>();
        if (joint.constraintId != 0) {
            m_constraintCleanup(joint.constraintId);
            joint.constraintId = 0;
        }
    }
    // F2H66: cleanup del ragdoll si la entidad lo tiene materializado.
    if (m_ragdollCleanup && m_alive.hasComponent<RagdollComponent>()) {
        auto& rag = m_alive.getComponent<RagdollComponent>();
        if (rag.ragdollId != 0) {
            m_ragdollCleanup(rag.ragdollId);
            rag.ragdollId = 0;
        }
    }
    m_scene->destroyEntity(m_alive);
    m_alive = Entity{};
    Log::editor()->info("Eliminada entidad '{}'.", m_snapshot.tag);
}

} // namespace Mood
