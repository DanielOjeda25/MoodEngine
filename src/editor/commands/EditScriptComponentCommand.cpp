#include "editor/commands/EditScriptComponentCommand.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

EditScriptComponentCommand::EditScriptComponentCommand(
    Scene* scene,
    std::string entityTag,
    bool hadComponent,
    std::string oldPath,
    std::string newPath,
    std::string label)
    : m_scene(scene),
      m_entityTag(std::move(entityTag)),
      m_hadComponent(hadComponent),
      m_oldPath(std::move(oldPath)),
      m_newPath(std::move(newPath)),
      m_label(std::move(label)) {}

namespace {

Entity findByTag(Scene* scene, const std::string& tag) {
    Entity target;
    if (scene == nullptr) return target;
    scene->forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (!static_cast<bool>(target) && t.name == tag) {
            target = e;
        }
    });
    return target;
}

} // namespace

void EditScriptComponentCommand::execute() {
    Entity target = findByTag(m_scene, m_entityTag);
    if (!static_cast<bool>(target)) {
        Log::editor()->warn(
            "EditScriptComponentCommand: no encontre entidad con tag '{}'",
            m_entityTag);
        return;
    }
    if (target.hasComponent<ScriptComponent>()) {
        auto& sc = target.getComponent<ScriptComponent>();
        sc.path      = m_newPath;
        sc.loaded    = false;
        sc.lastError.clear();
    } else {
        target.addComponent<ScriptComponent>(m_newPath);
    }
}

void EditScriptComponentCommand::undo() {
    Entity target = findByTag(m_scene, m_entityTag);
    if (!static_cast<bool>(target)) {
        Log::editor()->warn(
            "EditScriptComponentCommand: no encontre entidad con tag '{}'",
            m_entityTag);
        return;
    }
    if (!m_hadComponent) {
        // El drop CREO el componente — undo lo remueve.
        if (target.hasComponent<ScriptComponent>()) {
            target.removeComponent<ScriptComponent>();
        }
    } else {
        // Restaurar path previo.
        if (target.hasComponent<ScriptComponent>()) {
            auto& sc = target.getComponent<ScriptComponent>();
            sc.path      = m_oldPath;
            sc.loaded    = false;
            sc.lastError.clear();
        } else {
            // Edge: alguien removio el componente entre execute y undo.
            // Recrearlo con el path viejo.
            target.addComponent<ScriptComponent>(m_oldPath);
        }
    }
}

} // namespace Mood
