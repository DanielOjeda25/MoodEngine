#pragma once

// F3H27: comando undoable para cambiar el `parent` del TransformComponent
// de una entidad. Preserva el world-space (Blender/Unity convention):
// al reparentar, recompute la `position/rotationEuler/scale` LOCAL del
// hijo para que su world matrix VISUAL no cambie en el frame del reparent.
//
// Sin preservación, mover un objeto a un padre con escala/posición no-cero
// lo "teleportaría" — el dev arrastra el objeto y desaparece. Por eso
// snapshot del world matrix antes, y recompute local desde inverse(parent
// world) * oldWorld.

#include "editor/commands/Command.h"

#include <entt/entt.hpp>
#include <glm/vec3.hpp>

namespace Mood {

class Scene;

class SetParentCommand : public ICommand {
public:
    /// @param scene  Scene non-owning. Necesaria para resolver handles +
    ///               calcular world recursivo del nuevo padre.
    /// @param child  Entity que cambia de padre. Debe tener
    ///               TransformComponent.
    /// @param newParent  Nuevo padre. `entt::null` = root (sin padre).
    SetParentCommand(Scene* scene, entt::entity child, entt::entity newParent);

    void execute() override;
    void undo() override;
    std::string name() const override;
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

    /// @brief No-op si oldParent == newParent (idempotente sin cambios).
    bool isNoOp() const { return m_oldParent == m_newParent; }

private:
    Scene* m_scene;
    entt::entity m_child;
    entt::entity m_oldParent;  // capturado en ctor
    entt::entity m_newParent;

    // Snapshot local PRE-cambio (para restore en undo) y POST-cambio
    // (recomputado en ctor con el world preserve). El execute() y undo()
    // solo aplican los snapshots — sin recompute al vuelo.
    glm::vec3 m_oldLocalPos;
    glm::vec3 m_oldLocalEuler;
    glm::vec3 m_oldLocalScale;
    glm::vec3 m_newLocalPos;
    glm::vec3 m_newLocalEuler;
    glm::vec3 m_newLocalScale;
};

} // namespace Mood
