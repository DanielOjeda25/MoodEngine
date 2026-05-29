// F4H1 — Tabla `health` para los scripts Lua.
//
// API expuesta:
//   health.damage(tag, amount)   -- aplica amount HP de daño
//   health.heal(tag, amount)     -- suma amount HP (clamp a max)
//   health.get(tag) -> table     -- { current, max, dead } o nil si no tiene
//   health.is_alive(tag) -> bool
//
// `tag` busca la primera entity con ese tag (convencion editor); si no
// existe o no tiene HealthComponent, la API loguea warn y devuelve nil/0.

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/gameplay/Health.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/BindingsCommon.h"  // F4H1.5: findEntityByTag compartido

#include <sol/sol.hpp>

#include <string>

namespace Mood {

namespace {

using bindings::findEntityByTag;  // alias local — F4H1.5

} // anonymous

void setupHealthBindings(sol::state& lua, Scene* scene) {
    sol::table t = lua.create_named_table("health");

    t.set_function("damage", [scene](const std::string& tag, f32 amount) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e) {
            Log::script()->warn("health.damage: tag '{}' no encontrado", tag);
            return;
        }
        Health::applyDamage(*scene, e, amount);
    });

    t.set_function("heal", [scene](const std::string& tag, f32 amount) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e) {
            Log::script()->warn("health.heal: tag '{}' no encontrado", tag);
            return;
        }
        Health::heal(*scene, e, amount);
    });

    t.set_function("get", [scene, &lua](const std::string& tag) -> sol::object {
        if (scene == nullptr) return sol::nil;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<HealthComponent>()) return sol::nil;
        const auto& h = e.getComponent<HealthComponent>();
        sol::table out = lua.create_table();
        out["current"] = h.current;
        out["max"]     = h.max;
        out["dead"]    = h.dead;
        return out;
    });

    t.set_function("is_alive", [scene](const std::string& tag) -> bool {
        if (scene == nullptr) return false;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<HealthComponent>()) return false;
        return !e.getComponent<HealthComponent>().dead;
    });
}

} // namespace Mood
