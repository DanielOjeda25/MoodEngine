#pragma once

// EditTransformCommand (Hito 27 Bloque 2): comando undo/redo para edits
// de TransformComponent (position / rotationEuler / scale) hechos via
// gizmo en el viewport.
//
// El gizmo dispara mutaciones cada frame durante el drag — pushear un
// comando por frame produciria 60 entradas en el undo stack por gesto.
// Estrategia: el callsite captura `startValue` al hacer click (drag
// start) y `endValue` al soltar (drag end), y pushea UN solo comando
// con (start, end). El comando es no-op si ambos coinciden.
//
// Resiliencia ante destruccion de la entidad: si entre el push y el
// undo/redo la entidad fue destruida (ej. via DeleteEntityCommand),
// el `m_registry.valid(handle)` falla y la operacion es no-op (con
// log warning). Esto preserva el invariante "Ctrl+Z nunca crashea".

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

class EditTransformCommand : public ICommand {
public:
    enum class Field : u8 { Position, Rotation, Scale };

    EditTransformCommand(Entity entity, Field field,
                          glm::vec3 before, glm::vec3 after);

    void execute() override;
    void undo() override;
    std::string name() const override;
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

    /// True si before == after (push no-op para evitar entradas vacias
    /// en el history al hacer click sin drag).
    bool isNoOp() const;

private:
    void applyValue(const glm::vec3& v);

    Entity     m_entity;
    Field      m_field;
    glm::vec3  m_before;
    glm::vec3  m_after;
};

} // namespace Mood
