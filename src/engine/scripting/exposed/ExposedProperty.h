#pragma once

// Exposed properties: parametros de scripts Lua editables desde el
// Inspector y persistidos por entidad en `.moodmap` (Hito 24).
//
// Un script declara con `engine.exposed("name", default)` los valores
// que quiere exponer. El binding registra metadata + devuelve el
// override del ScriptComponent si lo hay, sino el default.
//
// Tipos soportados V1: number (f32), bool, string, vec3. Cualquier
// otro genera warning y se skipea.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <string>
#include <variant>

namespace Mood {

/// @brief Tipos editable-en-Inspector.
enum class ExposedType {
    Number, // f32
    Bool,
    String,
    Vec3,
};

/// @brief Valor concreto de una exposed property. El variant evita
///        downcasting y matchea natural con `sol::object` via
///        `std::visit`.
using ExposedValue = std::variant<f32, bool, std::string, glm::vec3>;

/// @brief Metadata de una property descubierta al cargar el script.
struct ExposedProperty {
    std::string name;
    ExposedType type = ExposedType::Number;
    /// @brief Valor que el script paso como default — Inspector lo
    ///        muestra al apretar "Reset".
    ExposedValue defaultValue;
};

/// @brief Mapea el variant a su enum (utilidad — se usa al inferir
///        tipo desde el default Lua).
ExposedType typeOf(const ExposedValue& v);

} // namespace Mood
