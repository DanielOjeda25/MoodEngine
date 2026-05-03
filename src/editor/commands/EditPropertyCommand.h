#pragma once

// EditPropertyCommand<T> (Hito 32 D): comando undo/redo genérico para
// edits de UN campo de UN componente de UNA entidad. Templado en el
// tipo del valor (f32, glm::vec3, glm::vec4, bool, std::string, etc).
//
// Patrón de uso desde el InspectorPanel:
//   1. ImGui::DragFloat3 muta el campo en vivo durante el drag.
//   2. `IsItemActivated()` (frame inicial del drag) → snapshot del before.
//   3. `IsItemDeactivatedAfterEdit()` (frame final tras edit real) →
//      capturar after, revertir a before, push command (que re-aplica).
//
// Asi un drag completo (60 frames) genera UN solo comando en el history,
// equivalente al patrón usado por `EditTransformCommand` en el gizmo.
//
// Resiliencia: si la entidad fue destruida entre push y undo, no-op
// silencioso (chequeo `registry.valid(handle)`). Mismo invariante que
// los otros commands del stack.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <functional>
#include <string>
#include <utility>

namespace Mood {

template<typename T>
class EditPropertyCommand : public ICommand {
public:
    /// Setter generico: recibe la entidad viva + nuevo valor, aplica el
    /// cambio sobre el componente correcto. Cada callsite del Inspector
    /// captura el path al campo via lambda (ej. `[](Entity& e, glm::vec3
    /// const& v){ e.getComponent<TransformComponent>().position = v; }`).
    using Setter = std::function<void(Entity&, const T&)>;

    EditPropertyCommand(Entity entity, T before, T after,
                         Setter setter, std::string label)
        : m_entity(entity)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_setter(std::move(setter))
        , m_label(std::move(label)) {}

    void execute() override {
        if (!isEntityValid() || !m_setter) return;
        m_setter(m_entity, m_after);
    }
    void undo() override {
        if (!isEntityValid() || !m_setter) return;
        m_setter(m_entity, m_before);
    }
    std::string name() const override { return m_label; }
    void onEntityRemap(entt::entity oldH, entt::entity newH) override {
        if (m_entity.handle() == oldH) {
            m_entity = Entity(newH, m_entity.scene());
        }
    }

    /// True si before == after — push() se vuelve no-op para evitar
    /// entradas vacias en el history al hacer click sin drag.
    bool isNoOp() const { return m_before == m_after; }

private:
    bool isEntityValid() const {
        return m_entity.scene() != nullptr
            && m_entity.scene()->registry().valid(m_entity.handle());
    }

    Entity m_entity;
    T      m_before;
    T      m_after;
    Setter m_setter;
    std::string m_label;
};

} // namespace Mood
