#pragma once

// MultiEditTransformCommand (F2H23 polish iter 5): variante multi-entity
// del EditTransformCommand. Necesario para que el gizmo del viewport
// pueda hacer Ctrl+Z agrupado cuando el dev movio/roto/escalo N
// entidades a la vez via Shift+click + drag.
//
// Diseno: vector de (entity, before, after) que comparten el mismo
// Field. Resilience ante destruccion de cualquier entidad: cada apply
// chequea valid(handle); si una se borro, esa se skipea silenciosamente
// (mismo patron que EditTransformCommand single).

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "editor/commands/EditTransformCommand.h"
#include "engine/scene/core/Entity.h"

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

class MultiEditTransformCommand : public ICommand {
public:
    struct Entry {
        Entity entity;
        glm::vec3 before;
        glm::vec3 after;
    };

    MultiEditTransformCommand(EditTransformCommand::Field field,
                                std::vector<Entry> entries);

    void execute() override;
    void undo() override;
    std::string name() const override;
    void onEntityRemap(entt::entity oldH, entt::entity newH) override;

    /// True si todas las entries son no-op (before == after en cada una).
    bool isNoOp() const;

private:
    void applyValue(const Entry& e, const glm::vec3& v);

    EditTransformCommand::Field m_field;
    std::vector<Entry>           m_entries;
};

} // namespace Mood
