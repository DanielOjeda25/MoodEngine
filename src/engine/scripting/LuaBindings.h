#pragma once

// Bindings C++ -> Lua (Hito 8 Bloque 3, scope minimo confirmado).
// API expuesta a los scripts:
//   - self.tag          (string, read-only)
//   - self.transform    -> TransformComponent (mutable)
//       .position.x/y/z
//       .rotationEuler.x/y/z
//       .scale.x/y/z
//   - log.info(str)
//   - log.warn(str)
//
// Input y scene:create/destroy NO estan expuestos en Hito 8. Ver PLAN_HITO8.

#include <sol/sol.hpp>

namespace Mood {

class Entity;

/// @brief Registra los usertypes y tablas en `lua` y setea la global
///        `self` al entity dado. Llamar una vez por `sol::state` antes
///        de cargar el script.
void setupLuaBindings(sol::state& lua, Entity self);

} // namespace Mood
