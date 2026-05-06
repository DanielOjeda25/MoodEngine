#pragma once

// F2H13: modelo de seleccion multi-elemento del editor. Header-only,
// helpers libres, sin acoplamiento mas alla del Entity opaque handle.
//
// Convencion estilo Blender / Hammer / TrenchBroom:
//   - Click       -> reset + setActive(e).
//   - Shift+click -> toggle(e).
//   - Ctrl+click  -> add(e) (que tambien hace setActive).
//
// Invariantes garantizados por los helpers:
//   - Si selected.empty()        =>  active == Entity{}.
//   - Si active != Entity{}      =>  contains(set, active).
//   - selected sin duplicados (mismo handle solo una vez).
//
// Usado por:
//   - HierarchyPanel (multi-click con modifiers).
//   - EditorApplication::onViewportClick (picking).
//   - EditorUI::drawBooleanOpMenu (cascade ops).
//   - SelectionOverlay (outline visual).

#include "engine/scene/core/Entity.h"

#include <algorithm>
#include <vector>

namespace Mood {

struct SelectionSet {
    std::vector<Entity> selected;
    Entity active{};
};

inline bool contains(const SelectionSet& set, Entity e) {
    if (!static_cast<bool>(e)) return false;
    return std::find_if(set.selected.begin(), set.selected.end(),
        [&](const Entity& s) { return s.handle() == e.handle(); })
        != set.selected.end();
}

inline void clear(SelectionSet& set) {
    set.selected.clear();
    set.active = Entity{};
}

/// @brief Agrega `e` (si no estaba) y lo setea como active.
///        No-op si `e` es default-constructed.
inline void add(SelectionSet& set, Entity e) {
    if (!static_cast<bool>(e)) return;
    if (!contains(set, e)) {
        set.selected.push_back(e);
    }
    set.active = e;
}

/// @brief Quita `e` (si estaba). Si era el active, el nuevo active
///        es el ULTIMO elemento del set tras la remocion (o
///        Entity{} si quedo vacio). El "ultimo" es la convencion
///        de "la mas recientemente clickeada de las que quedan",
///        consistente con la mental model de "active = ultima".
inline void remove(SelectionSet& set, Entity e) {
    if (!static_cast<bool>(e)) return;
    auto it = std::find_if(set.selected.begin(), set.selected.end(),
        [&](const Entity& s) { return s.handle() == e.handle(); });
    if (it == set.selected.end()) return;
    const bool wasActive = (set.active.handle() == e.handle());
    set.selected.erase(it);
    if (wasActive) {
        set.active = set.selected.empty()
            ? Entity{} : set.selected.back();
    }
}

/// @brief Si `e` esta -> remove. Si no -> add.
inline void toggle(SelectionSet& set, Entity e) {
    if (!static_cast<bool>(e)) return;
    if (contains(set, e)) {
        remove(set, e);
    } else {
        add(set, e);
    }
}

/// @brief Reemplaza el set por la unica entidad `e`. Patron click
///        plain: clear + add atomic. Si `e` es default-constructed,
///        equivale a `clear(set)`.
inline void replaceWithSingle(SelectionSet& set, Entity e) {
    clear(set);
    add(set, e);
}

} // namespace Mood
