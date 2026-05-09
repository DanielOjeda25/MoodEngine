#pragma once

// F2H33: comandos undoable para VisGroups. 4 ops:
//   - CreateVisGroupCommand  : agrega un grupo nuevo al Scene.
//   - DeleteVisGroupCommand  : quita el grupo + remueve membership de las
//                              entities que pertenecian (snapshot pre).
//   - EditVisGroupCommand    : muta name/color/hidden del grupo. Snapshot
//                              before/after del VisGroup completo.
//   - AssignVisGroupCommand  : muta la membership de una entity (puede
//                              ser id != 0 → un grupo, o 0 → sin grupo).
//                              Snapshot before/after del groupId.
//
// Captura por TAG (no entt handle) — mismo patron que el resto post-F2H16
// para sobrevivir Delete + Undo de la entity.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/VisGroup.h"

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

class Scene;

/// @brief Crea un VisGroup nuevo. execute() lo agrega al Scene con el id
///        capturado en m_groupId (la primera vez se computa al construir
///        el comando, usando `scene.addVisGroup(name, color).id`). undo()
///        lo elimina por id.
class CreateVisGroupCommand : public ICommand {
public:
    CreateVisGroupCommand(Scene* scene, std::string name, glm::vec3 color);

    void execute() override;
    void undo() override;
    std::string name() const override { return "Crear VisGroup"; }

    /// @brief Id del grupo creado (util para tests + para que el caller
    ///        asigne entities al grupo recien creado).
    u64 groupId() const { return m_groupId; }

private:
    Scene* m_scene;
    u64 m_groupId = 0;       // se setea en el primer execute()
    std::string m_groupName;
    glm::vec3 m_groupColor;
    bool m_firstExecute = true;
};

/// @brief Elimina un VisGroup + remueve membership de todas las entities
///        que apuntaban a el. undo() restaura el grupo y reasigna las
///        entities (por TAG snapshot).
class DeleteVisGroupCommand : public ICommand {
public:
    DeleteVisGroupCommand(Scene* scene, u64 groupId);

    void execute() override;
    void undo() override;
    std::string name() const override { return "Eliminar VisGroup"; }

private:
    Scene* m_scene;
    u64 m_groupId;
    VisGroup m_groupSnapshot;          // captura antes del primer execute
    std::vector<std::string> m_memberTags;  // entities que pertenecian
    bool m_firstExecute = true;
};

/// @brief Cambia name / color / hidden de un grupo. Snapshot completo del
///        VisGroup pre/post para revertirlo en bloque.
class EditVisGroupCommand : public ICommand {
public:
    EditVisGroupCommand(Scene* scene, u64 groupId,
                        VisGroup before, VisGroup after,
                        std::string label = "Editar VisGroup");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    void apply(const VisGroup& vg);

    Scene* m_scene;
    u64 m_groupId;
    VisGroup m_before;
    VisGroup m_after;
    std::string m_label;
};

/// @brief Cambia la membership de una entity (groupId before -> after).
///        Si after == 0, remove el componente; si != 0, emplace_or_replace.
///        Captura por TAG.
class AssignVisGroupCommand : public ICommand {
public:
    AssignVisGroupCommand(Scene* scene, std::string entityTag,
                          u64 beforeGroupId, u64 afterGroupId,
                          std::string label = "Asignar VisGroup");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    void apply(u64 groupId);

    Scene* m_scene;
    std::string m_entityTag;
    u64 m_beforeGroupId;
    u64 m_afterGroupId;
    std::string m_label;
};

} // namespace Mood
