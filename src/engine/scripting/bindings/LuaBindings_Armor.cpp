// F4H4 — Tabla `armor` para los scripts Lua. Gemelo del binding health.
//
// API expuesta:
//   armor.give(tag, amount)      -- suma amount (clamp a max)
//   armor.set(tag, current)      -- setea current absoluto (clamp [0, max])
//   armor.get(tag) -> table      -- { current, max, absorbRatio } o nil

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/BindingsCommon.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <string>

namespace Mood {

namespace {

using bindings::findEntityByTag;

} // anonymous

void setupArmorBindings(sol::state& lua, Scene* scene) {
    sol::table t = lua.create_named_table("armor");

    t.set_function("give", [scene](const std::string& tag, f32 amount) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<ArmorComponent>()) {
            Log::script()->warn("armor.give: tag '{}' sin ArmorComponent", tag);
            return;
        }
        auto& a = e.getComponent<ArmorComponent>();
        a.current = std::min(a.max, a.current + std::max(0.0f, amount));
    });

    t.set_function("set", [scene](const std::string& tag, f32 current) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<ArmorComponent>()) {
            Log::script()->warn("armor.set: tag '{}' sin ArmorComponent", tag);
            return;
        }
        auto& a = e.getComponent<ArmorComponent>();
        a.current = std::clamp(current, 0.0f, a.max);
    });

    t.set_function("get", [scene, &lua](const std::string& tag) -> sol::object {
        if (scene == nullptr) return sol::nil;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<ArmorComponent>()) return sol::nil;
        const auto& a = e.getComponent<ArmorComponent>();
        sol::table out = lua.create_table();
        out["current"]     = a.current;
        out["max"]         = a.max;
        out["absorbRatio"] = a.absorbRatio;
        return out;
    });
}

} // namespace Mood
