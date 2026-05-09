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

#include "core/Types.h"
#include "engine/scene/core/Entity.h"

#include <algorithm>
#include <vector>

namespace Mood {

struct SelectionSet {
    std::vector<Entity> selected;
    Entity active{};
    /// @brief F2H17 + F2H33: caras seleccionadas del `active` cuando
    ///        EditorSubMode == Face. Vacio = ninguna cara picked. La
    ///        ULTIMA del vector es la "active" (la mas recientemente
    ///        clickeada). Solo valido cuando submode == Face Y `active`
    ///        tiene BrushComponent. En Object Mode siempre vacio.
    ///        F2H17 era `i32 activeFaceIndex`; F2H33 lo extiende a
    ///        multi-cara para texture alignment "Treat as one face".
    std::vector<i32> selectedFaceIndices;

    /// @brief Indice de la cara activa (= ultima clickeada), o -1 si
    ///        ninguna. Compat con call-sites pre-F2H33 que solo
    ///        necesitan saber la "primary".
    i32 activeFaceIndex() const {
        return selectedFaceIndices.empty()
            ? -1 : selectedFaceIndices.back();
    }
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
    set.selectedFaceIndices.clear();  // F2H33
}

/// @brief Agrega `e` (si no estaba) y lo setea como active.
///        No-op si `e` es default-constructed.
inline void add(SelectionSet& set, Entity e) {
    if (!static_cast<bool>(e)) return;
    if (!contains(set, e)) {
        set.selected.push_back(e);
    }
    // F2H17: cambiar el active resetea las caras (las que estaban
    // eran de OTRO brush; dejan de tener sentido).
    if (!static_cast<bool>(set.active) ||
        set.active.handle() != e.handle()) {
        set.selectedFaceIndices.clear();  // F2H33
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
        // F2H17: el active cambio (o desaparecio) -> caras pierden
        // sentido (eran del brush viejo).
        set.selectedFaceIndices.clear();  // F2H33
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

// --- F2H33: helpers para multi-select de caras ---

/// @brief Reemplaza la seleccion de caras por una sola. Patron click
///        plain en Face Mode. faceIdx == -1 limpia el set.
inline void setSingleFace(SelectionSet& set, i32 faceIdx) {
    set.selectedFaceIndices.clear();
    if (faceIdx >= 0) set.selectedFaceIndices.push_back(faceIdx);
}

/// @brief Si la cara ya esta seleccionada -> remove. Si no -> push back
///        (queda como nueva active). Patron Shift+click en Face Mode.
inline void toggleFace(SelectionSet& set, i32 faceIdx) {
    if (faceIdx < 0) return;
    auto it = std::find(set.selectedFaceIndices.begin(),
                         set.selectedFaceIndices.end(), faceIdx);
    if (it != set.selectedFaceIndices.end()) {
        set.selectedFaceIndices.erase(it);
    } else {
        set.selectedFaceIndices.push_back(faceIdx);
    }
}

/// @brief True si la cara esta dentro del set actual.
inline bool containsFace(const SelectionSet& set, i32 faceIdx) {
    if (faceIdx < 0) return false;
    return std::find(set.selectedFaceIndices.begin(),
                      set.selectedFaceIndices.end(), faceIdx)
        != set.selectedFaceIndices.end();
}

} // namespace Mood
