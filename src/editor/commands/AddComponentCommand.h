#pragma once

// AddComponentCommand (F2H45 Bloque A): undoable add de un componente
// sobre una entidad. Type-erased via std::function — un solo archivo
// cubre los 11 componentes del popup "Add Component" del Inspector.
//
// Modelo:
//   - El callsite NO llama `addComponent<T>()` directamente; arma el
//     comando con dos closures `add` y `remove` y lo empuja al
//     HistoryStack.
//   - `execute()` invoca `add(entity)` (ese sera el primer add real Y
//     cualquier `redo()` posterior).
//   - `undo()` invoca `remove(entity)` — devuelve la entidad al estado
//     pre-add.
//
// Helper templated `makeAddComponentCommand<T>(entity, label)` evita
// boilerplate en cada uno de los 11 calls del popup.
//
// Caveat (component con valores default vs editado): si el dev hace
// add → edita campos (cada edit emite su propio EditPropertyCommand) →
// undo, los EditProperty se deshacen primero (LIFO) y luego el
// AddComponent remueve el componente. Redo recrea con default y
// re-aplica los edits en orden. No requiere snapshot extra.

#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace Mood {

class AddComponentCommand : public ICommand {
public:
    using AddFn    = std::function<void(Entity&)>;
    using RemoveFn = std::function<void(Entity&)>;

    AddComponentCommand(Entity entity, AddFn addFn, RemoveFn removeFn,
                        std::string label)
        : m_entity(entity)
        , m_add(std::move(addFn))
        , m_remove(std::move(removeFn))
        , m_label(std::move(label)) {}

    void execute() override {
        if (!isEntityValid() || !m_add) return;
        m_add(m_entity);
    }
    void undo() override {
        if (!isEntityValid() || !m_remove) return;
        m_remove(m_entity);
    }
    std::string name() const override { return m_label; }
    void onEntityRemap(entt::entity oldH, entt::entity newH) override {
        if (m_entity.handle() == oldH) {
            m_entity = Entity(newH, m_entity.scene());
        }
    }

private:
    bool isEntityValid() const {
        return m_entity.scene() != nullptr
            && m_entity.scene()->registry().valid(m_entity.handle());
    }

    Entity      m_entity;
    AddFn       m_add;
    RemoveFn    m_remove;
    std::string m_label;
};

/// Helper templated: arma el comando capturando T en las dos closures
/// (add via `addComponent<T>()` con valores default; remove via
/// `removeComponent<T>()`). Requiere que T sea default-constructible —
/// los 11 componentes del popup lo son.
template<typename T>
std::unique_ptr<AddComponentCommand>
makeAddComponentCommand(Entity entity, std::string label) {
    return std::make_unique<AddComponentCommand>(
        entity,
        [](Entity& e) { e.addComponent<T>(); },
        [](Entity& e) { e.removeComponent<T>(); },
        std::move(label));
}

} // namespace Mood
