// F4H7 — Tabla `enemy` para los scripts Lua.
//
// API expuesta:
//   enemy.get_state(tag) -> string         -- "idle"|"alert"|"chase"|...
//   enemy.set_state(tag, state)            -- force-set debug
//   enemy.kill(tag)                        -- damage masivo via Health
//   enemy.spec(tag) -> table               -- {display_name, health, ...}

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/enemy/EnemySpec.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/BindingsCommon.h"

#include <sol/sol.hpp>

#include <string>

namespace Mood {

namespace {

using bindings::findEntityByTag;

const char* stateToLuaStr(EnemyState s) {
    switch (s) {
        case EnemyState::Idle:   return "idle";
        case EnemyState::Alert:  return "alert";
        case EnemyState::Chase:  return "chase";
        case EnemyState::Attack: return "attack";
        case EnemyState::Pain:   return "pain";
        case EnemyState::Dead:   return "dead";
    }
    return "idle";
}

bool luaStrToState(const std::string& s, EnemyState& out) {
    if      (s == "idle")   { out = EnemyState::Idle;   return true; }
    else if (s == "alert")  { out = EnemyState::Alert;  return true; }
    else if (s == "chase")  { out = EnemyState::Chase;  return true; }
    else if (s == "attack") { out = EnemyState::Attack; return true; }
    else if (s == "pain")   { out = EnemyState::Pain;   return true; }
    else if (s == "dead")   { out = EnemyState::Dead;   return true; }
    return false;
}

} // anonymous

void setupEnemyBindings(sol::state& lua, Scene* scene, AssetManager* assets) {
    sol::table t = lua.create_named_table("enemy");

    t.set_function("get_state", [scene](const std::string& tag) -> std::string {
        if (scene == nullptr) return "idle";
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<EnemyComponent>()) {
            Log::script()->warn("enemy.get_state: tag '{}' sin EnemyComponent", tag);
            return "idle";
        }
        return stateToLuaStr(e.getComponent<EnemyComponent>().state);
    });

    t.set_function("set_state", [scene](const std::string& tag,
                                          const std::string& stateStr) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<EnemyComponent>()) {
            Log::script()->warn("enemy.set_state: tag '{}' sin EnemyComponent", tag);
            return;
        }
        EnemyState next = EnemyState::Idle;
        if (!luaStrToState(stateStr, next)) {
            Log::script()->warn("enemy.set_state: estado desconocido '{}'", stateStr);
            return;
        }
        auto& ec = e.getComponent<EnemyComponent>();
        ec.state = next;
        ec.stateTime = 0.0f;
    });

    t.set_function("kill", [scene](const std::string& tag) {
        if (scene == nullptr) return;
        Entity e = findEntityByTag(*scene, tag);
        if (!e) {
            Log::script()->warn("enemy.kill: tag '{}' no encontrado", tag);
            return;
        }
        // 99999 dmg garantiza la muerte sin asumir el max del Health.
        Health::applyDamage(*scene, e, 99999.0f);
    });

    t.set_function("spec", [scene, assets, &lua](const std::string& tag)
                                                  -> sol::object {
        if (scene == nullptr || assets == nullptr) return sol::nil;
        Entity e = findEntityByTag(*scene, tag);
        if (!e || !e.hasComponent<EnemyComponent>()) return sol::nil;

        const auto& ec = e.getComponent<EnemyComponent>();
        const Enemy::Spec* spec = assets->getEnemy(ec.enemyAssetId);
        if (spec == nullptr) return sol::nil;

        sol::table out = lua.create_table();
        out["display_name"]    = spec->displayName;
        out["health"]          = spec->health;
        out["aggro_range"]     = spec->aggroRange;
        out["attack_range"]    = spec->attackRange;
        out["move_speed"]      = spec->moveSpeed;
        out["damage"]          = spec->damage;
        out["attack_cooldown"] = spec->attackCooldown;
        out["pain_threshold"]  = spec->painThreshold;
        out["pain_duration"]   = spec->painDuration;
        // F4H9 attack fields.
        out["attack_kind"]        = spec->attackKind;
        out["wind_up_sec"]        = spec->windUpSec;
        out["projectile_weapon"]  = spec->projectileWeapon;
        return out;
    });
}

} // namespace Mood
