#pragma once

// Bindings C++ -> Lua. API expuesta a los scripts:
//   - self.tag          (string, read-only)
//   - self.transform    -> TransformComponent (mutable)
//   - log.info(str) / log.warn(str)
//   - hud.setHp/setAmmo/setPaused + getters
//   - engine.exposed(name, default)  (Hito 24)
//   - physics.raycast(origin, dir, maxDist) -> {hit, point, normal, distance, bodyId}  (Hito 33)

#include <sol/sol.hpp>

namespace Mood {

class AssetManager;
class Entity;
class PhysicsWorld;
struct ScriptComponent;

/// @brief Registra los usertypes y tablas en `lua` y setea la global
///        `self` al entity dado. `scriptComponent` se usa para soportar
///        `engine.exposed(...)` — leer `overrides` y registrar
///        `exposedProps` al cargar el script. Si es nullptr, el binding
///        de `engine.exposed` siempre devuelve el default sin registrar
///        (util para tests headless sin ECS).
/// @param physics Hito 33: si presente, registra la tabla `physics` con
///        `physics.raycast`. Si nullptr, la tabla no existe y los scripts
///        que llaman `physics.raycast` van a fallar — los tests headless
///        sin Jolt pasan nullptr.
/// @param assets F2H48: si presente, registra `dialog.start(path)` que
///        carga el `.mooddialog` via `AssetManager::loadDialog`. Si
///        nullptr, `dialog.start` loggea warn y retorna false. Tests
///        headless sin AssetManager pueden pasar nullptr y usar las
///        funciones que no dependen de assets (set_var/get_var/isActive/
///        advance/continueNext/stop).
void setupLuaBindings(sol::state& lua, Entity self,
                       ScriptComponent* scriptComponent = nullptr,
                       PhysicsWorld* physics = nullptr,
                       AssetManager* assets = nullptr);

} // namespace Mood
