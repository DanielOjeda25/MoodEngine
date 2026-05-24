#pragma once

// MultiEditPropertyCommand<T> (F3H8): variante multi-entity del
// EditPropertyCommand<T> (Hito 32 D). Sirve para que el Inspector pueda
// editar un mismo field de N entidades a la vez con UN solo Ctrl+Z
// agrupado.
//
// Diseno snapshot-based (no delta — esa semantica la usa
// MultiEditTransformCommand de F2H23 iter 5): cada Entry guarda su
// propio `before`, pero TODOS comparten el mismo `after` (el dev
// homogeniza al editar). Undo restaura cada entry a su before
// individual.
//
// Resiliencia: cada apply chequea valid(handle); si una entidad fue
// destruida tras el push, esa se skipea silenciosamente (mismo patron
// que MultiEditTransformCommand).

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Mood {

template<typename T>
class MultiEditPropertyCommand : public ICommand {
public:
    /// Cada entidad afectada con su valor pre-edit individual.
    struct Entry {
        Entity entity;
        T      before;
    };

    /// Setter generico: aplica el valor sobre el componente correcto de
    /// la entidad. Mismo contrato que EditPropertyCommand<T>::Setter.
    using Setter = std::function<void(Entity&, const T&)>;

    MultiEditPropertyCommand(std::vector<Entry> entries, T after,
                              Setter setter, std::string label)
        : m_entries(std::move(entries))
        , m_after(std::move(after))
        , m_setter(std::move(setter))
        , m_label(std::move(label)) {}

    void execute() override {
        if (!m_setter) return;
        for (auto& e : m_entries) {
            if (!isEntityValid(e.entity)) continue;
            m_setter(e.entity, m_after);
        }
    }

    void undo() override {
        if (!m_setter) return;
        for (auto& e : m_entries) {
            if (!isEntityValid(e.entity)) continue;
            m_setter(e.entity, e.before);
        }
    }

    std::string name() const override { return m_label; }

    void onEntityRemap(entt::entity oldH, entt::entity newH) override {
        for (auto& e : m_entries) {
            if (e.entity.handle() == oldH) {
                e.entity = Entity(newH, e.entity.scene());
            }
        }
    }

    /// True si TODAS las entries son no-op (before == after en cada una).
    /// El caller usa esto para evitar push en clicks sin drag real.
    bool isNoOp() const {
        for (const auto& e : m_entries) {
            if (e.before != m_after) return false;
        }
        return true;
    }

    /// Cantidad de entidades afectadas (debug / statusbar).
    usize entryCount() const { return m_entries.size(); }

private:
    static bool isEntityValid(const Entity& e) {
        return e.scene() != nullptr
            && e.scene()->registry().valid(e.handle());
    }

    std::vector<Entry> m_entries;
    T                  m_after;
    Setter             m_setter;
    std::string        m_label;
};

} // namespace Mood
