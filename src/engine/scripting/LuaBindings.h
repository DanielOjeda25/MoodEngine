#pragma once

// Bindings C++ -> Lua. API expuesta a los scripts:
//   - self.tag          (string, read-only)
//   - self.transform    -> TransformComponent (mutable)
//   - log.info(str) / log.warn(str)
//   - hud.setHp/setAmmo/setPaused + getters
//   - engine.exposed(name, default)  (Hito 24)

#include <sol/sol.hpp>

namespace Mood {

class Entity;
struct ScriptComponent;

/// @brief Registra los usertypes y tablas en `lua` y setea la global
///        `self` al entity dado. `scriptComponent` se usa para soportar
///        `engine.exposed(...)` — leer `overrides` y registrar
///        `exposedProps` al cargar el script. Si es nullptr, el binding
///        de `engine.exposed` siempre devuelve el default sin registrar
///        (util para tests headless sin ECS).
void setupLuaBindings(sol::state& lua, Entity self,
                       ScriptComponent* scriptComponent = nullptr);

} // namespace Mood
