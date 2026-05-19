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

/// @brief F2H52 Bloque E: registra la tabla `inventory` en `lua`.
///        Implementado en LuaBindings_Inventory.cpp (split por tamaño del
///        archivo principal). Llamado por `setupLuaBindings` al final
///        Y por `DialogScriptHost::init` (sandbox de dialog) para que
///        las choices puedan usar `inventory.has(...)` en `condition_lua`.
///        Requiere `scene` para resolver el player (tag scan) y `assets`
///        para mapear path logico -> ItemAssetId. Ambos pueden ser
///        nullptr en tests headless; las queries player-implicit retornan
///        false/0/{} silenciosamente.
void setupInventoryBindings(sol::state& lua, class Scene* scene,
                             AssetManager* assets);

/// @brief F2H53 Bloque E: registra la tabla `quest` en `lua` e instala el
///        `LuaEvaluator` + `LuaExecutor` del `QuestSystem` apuntando a esta
///        misma state. Implementado en LuaBindings_Quest.cpp.
///        Llamado por `setupLuaBindings` despues de `setupInventoryBindings`
///        para que los predicates `inventory.count(...) >= N` resuelvan
///        contra los bindings de inventory ya registrados.
///        `assets` puede ser nullptr en tests headless — las funciones
///        devuelven false/"" silenciosamente.
void setupQuestBindings(sol::state& lua, AssetManager* assets);

/// @brief F2H66 Bloque E: registra la tabla `ragdoll` en `lua`.
///        Implementado en LuaBindings_Ragdoll.cpp.
///        API:
///          ragdoll.enable(tag)                          -- activa sin impulse
///          ragdoll.enable(tag, {x=,y=,z=})              -- con impulse en torso
///          ragdoll.is_ragdolling(tag) -> bool
///        Requiere `scene` para encontrar la entity por tag. Si scene es
///        nullptr (tests headless), las funciones loggean warn y retornan
///        false. La materializacion fisica la hace `RagdollSystem::tick`
///        al siguiente frame.
void setupRagdollBindings(sol::state& lua, class Scene* scene);

} // namespace Mood
