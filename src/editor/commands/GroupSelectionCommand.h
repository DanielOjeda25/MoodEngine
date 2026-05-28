#pragma once

// F3H27: comando undoable para agrupar la selección bajo un Empty padre
// nuevo (Ctrl+G estilo Blender). Crea un entity sin mesh/brush con
// `position = centroide del AABB combinado` de los selectos, y reparenta
// cada selecto al nuevo Empty preservando world-space.
//
// Undo: destruye el Empty + restaura el parent original de cada child a
// su valor PRE-group. Si algún child tenía padre PRE, se respeta.
//
// El Empty se llama "Group_<N>" (N = índice incremental basado en la
// scene). Sin componentes adicionales — solo Tag + Transform — para que
// el dev pueda agregarle mesh/brush/etc después si quiere.

#include "editor/commands/Command.h"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

class Scene;
class AssetManager;

class GroupSelectionCommand : public ICommand {
public:
    GroupSelectionCommand(Scene* scene, AssetManager* assets,
                            std::vector<entt::entity> children);

    void execute() override;
    void undo() override;
    std::string name() const override { return "Agrupar selección"; }
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

    /// @brief Handle del Empty creado tras execute(); útil para
    ///        seleccionar el padre nuevo en el caller.
    entt::entity emptyHandle() const { return m_emptyHandle; }

    bool isNoOp() const { return m_children.size() < 2; }

private:
    Scene* m_scene;
    AssetManager* m_assets;
    glm::vec3 m_centroid;  // pos absoluta del Empty (world)
    std::string m_emptyTag;
    entt::entity m_emptyHandle;  // entt::null antes de execute()

    struct PreSnapshot {
        entt::entity handle;
        entt::entity oldParent;
        glm::vec3 oldLocalPos;
        glm::vec3 oldLocalEuler;
        glm::vec3 oldLocalScale;
        glm::mat4 oldWorld;  // para recalcular localNew tras crear Empty
    };
    std::vector<PreSnapshot> m_pre;
    std::vector<entt::entity> m_children;  // copia para iteración estable
};

} // namespace Mood
