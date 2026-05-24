#pragma once

// MultiEditTracker (F3H8): tracker hermano del InspectorEditTracker
// (single-entity) — guarda los `before` values de las N entidades del
// SelectionSet cuando el dev edita un field en multi-edit.
//
// Patron simetrico al InspectorEditTracker:
//   - IsItemActivated → snapshot vector<T> before via getter sobre las
//     N entidades. activeId = widget id.
//   - IsItemActive (cada frame) → propaga el currentValue (del active)
//     a todas las peers via setter — live preview, mismo "feel" que
//     F2H23 iter 5 para Transform.
//   - IsItemDeactivatedAfterEdit → captura active's currentValue como
//     `after`, revierte cada entidad a su before via setter, construye
//     MultiEditPropertyCommand<T> + push (que re-aplica el after).
//
// Single widget activo a la vez en ImGui (mismo invariante que
// InspectorEditTracker): un solo struct global por panel basta.

#include "core/Types.h"
#include "engine/scene/core/Entity.h"

#include <glm/vec3.hpp>
#include <imgui.h>

#include <variant>
#include <vector>

namespace Mood {

struct MultiEditTracker {
    /// Widget id que esta siendo editado. 0 = ninguno activo.
    ImGuiID activeId = 0;

    /// Entidades capturadas al inicio del drag (mismo orden que el
    /// vector `before*`). Necesario porque el SelectionSet puede mutar
    /// durante el drag (poco probable pero defensivo).
    std::vector<Entity> entities;

    /// Variant de vectores tipados — solo uno populado por sesion de
    /// edit. `monostate` = sin edit activo. Tipos cubren lo que el
    /// Inspector edita hoy en multi-edit (F3H8): f32 (intensity,
    /// radius, opacity...) y glm::vec3 (color, position offsets...).
    /// Tipos futuros (bool, int) se agregan aca cuando se necesiten.
    std::variant<
        std::monostate,
        std::vector<f32>,
        std::vector<glm::vec3>
    > before;

    /// Limpia el state (post-commit o cancel).
    void reset() {
        activeId = 0;
        entities.clear();
        before = std::monostate{};
    }
};

} // namespace Mood
