#include "editor/commands/VisGroupCommands.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

namespace {

/// @brief Busca una entity por tag. Devuelve Entity{} si no existe.
Entity findEntityByTag(Scene* scene, const std::string& tag) {
    Entity result;
    if (scene == nullptr) return result;
    scene->forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (!static_cast<bool>(result) && t.name == tag) result = e;
    });
    return result;
}

/// @brief Setea la membership de una entity al groupId pasado (0 =
///        remove componente). Loggea warn si la entity no existe.
void applyMembership(Scene* scene, const std::string& tag, u64 groupId) {
    Entity e = findEntityByTag(scene, tag);
    if (!e) {
        Log::editor()->warn(
            "VisGroupCommand: no encontre entidad con tag '{}'", tag);
        return;
    }
    if (groupId == 0) {
        if (e.hasComponent<VisGroupMembershipComponent>()) {
            e.removeComponent<VisGroupMembershipComponent>();
        }
    } else {
        e.addComponent<VisGroupMembershipComponent>(groupId);
    }
}

} // namespace

// --- CreateVisGroupCommand ---

CreateVisGroupCommand::CreateVisGroupCommand(Scene* scene,
                                              std::string name,
                                              glm::vec3 color)
    : m_scene(scene),
      m_groupName(std::move(name)),
      m_groupColor(color) {}

void CreateVisGroupCommand::execute() {
    if (m_scene == nullptr) return;
    if (m_firstExecute) {
        VisGroup& vg = m_scene->addVisGroup(m_groupName, m_groupColor);
        m_groupId = vg.id;
        m_firstExecute = false;
    } else {
        // Re-execute (Ctrl+Y): restaurar con el id original para que
        // las membership de entities sigan valiendo si hubo Assign +
        // Undo del create + Redo.
        VisGroup vg;
        vg.id = m_groupId;
        vg.name = m_groupName;
        vg.color = m_groupColor;
        vg.hidden = false;
        m_scene->insertVisGroup(std::move(vg));
    }
}

void CreateVisGroupCommand::undo() {
    if (m_scene == nullptr || m_groupId == 0) return;
    m_scene->removeVisGroup(m_groupId);
}

// --- DeleteVisGroupCommand ---

DeleteVisGroupCommand::DeleteVisGroupCommand(Scene* scene, u64 groupId)
    : m_scene(scene), m_groupId(groupId) {}

void DeleteVisGroupCommand::execute() {
    if (m_scene == nullptr || m_groupId == 0) return;

    if (m_firstExecute) {
        // Snapshot del grupo + tags de las entities miembros.
        if (const VisGroup* vg = m_scene->findVisGroup(m_groupId); vg != nullptr) {
            m_groupSnapshot = *vg;
        }
        m_memberTags.clear();
        m_scene->forEach<TagComponent, VisGroupMembershipComponent>(
            [&](Entity, TagComponent& t, VisGroupMembershipComponent& m) {
                if (m.groupId == m_groupId) m_memberTags.push_back(t.name);
            });
        m_firstExecute = false;
    }

    // Remover membership de cada miembro + borrar el grupo.
    for (const std::string& tag : m_memberTags) {
        applyMembership(m_scene, tag, 0);
    }
    m_scene->removeVisGroup(m_groupId);
}

void DeleteVisGroupCommand::undo() {
    if (m_scene == nullptr || m_groupId == 0) return;
    m_scene->insertVisGroup(m_groupSnapshot);
    for (const std::string& tag : m_memberTags) {
        applyMembership(m_scene, tag, m_groupId);
    }
}

// --- EditVisGroupCommand ---

EditVisGroupCommand::EditVisGroupCommand(Scene* scene, u64 groupId,
                                          VisGroup before, VisGroup after,
                                          std::string label)
    : m_scene(scene),
      m_groupId(groupId),
      m_before(std::move(before)),
      m_after(std::move(after)),
      m_label(std::move(label)) {
    // Forzar id correcto en ambos snapshots.
    m_before.id = m_groupId;
    m_after.id = m_groupId;
}

void EditVisGroupCommand::execute() { apply(m_after); }
void EditVisGroupCommand::undo()    { apply(m_before); }

void EditVisGroupCommand::apply(const VisGroup& vg) {
    if (m_scene == nullptr || m_groupId == 0) return;
    VisGroup* target = m_scene->findVisGroup(m_groupId);
    if (target == nullptr) {
        Log::editor()->warn(
            "EditVisGroupCommand: grupo id={} no existe", m_groupId);
        return;
    }
    *target = vg;
}

// --- AssignVisGroupCommand ---

AssignVisGroupCommand::AssignVisGroupCommand(Scene* scene,
                                              std::string entityTag,
                                              u64 beforeGroupId,
                                              u64 afterGroupId,
                                              std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_beforeGroupId(beforeGroupId),
      m_afterGroupId(afterGroupId),
      m_label(std::move(label)) {}

void AssignVisGroupCommand::execute() { apply(m_afterGroupId); }
void AssignVisGroupCommand::undo()    { apply(m_beforeGroupId); }

void AssignVisGroupCommand::apply(u64 groupId) {
    applyMembership(m_scene, m_entityTag, groupId);
}

} // namespace Mood
