#pragma once

// F3H27: comando undoable para des-agrupar la selección
// (Shift+Ctrl+G estilo Blender). Para cada selecto que tenga padre,
// setea parent = null (root) preservando world-space. Si el padre queda
// sin hijos tras el ungroup y es un Empty (sin geometría visible), NO
// se borra automáticamente — el dev decide. Anti-sorpresa.
//
// Undo: restaura el parent original + el local original de cada child.
// No-op si ningún selecto tiene padre.

#include "editor/commands/Command.h"

#include <entt/entt.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace Mood {

class Scene;

class UngroupSelectionCommand : public ICommand {
public:
    UngroupSelectionCommand(Scene* scene, std::vector<entt::entity> children);

    void execute() override;
    void undo() override;
    std::string name() const override { return "Des-agrupar selección"; }
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

    bool isNoOp() const { return m_pre.empty(); }

private:
    Scene* m_scene;

    struct PreSnapshot {
        entt::entity handle;
        entt::entity oldParent;
        glm::vec3 oldLocalPos;
        glm::vec3 oldLocalEuler;
        glm::vec3 oldLocalScale;
        glm::vec3 newWorldPos;
        glm::vec3 newWorldEuler;
        glm::vec3 newWorldScale;
    };
    std::vector<PreSnapshot> m_pre;
};

} // namespace Mood
