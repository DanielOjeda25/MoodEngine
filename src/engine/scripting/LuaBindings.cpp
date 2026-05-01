#include "engine/scripting/LuaBindings.h"

#include "core/Log.h"
#include "engine/game/GameState.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scripting/ExposedProperty.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <string>

namespace Mood {

void setupLuaBindings(sol::state& lua, Entity self,
                       ScriptComponent* scriptComponent,
                       PhysicsWorld* physics) {
    // Libs basicas. `io`/`os` quedan fuera: los scripts no deberian hacer
    // I/O al FS ni ejecutar procesos. `package` tampoco (sin require).
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // --- glm::vec3 ---
    lua.new_usertype<glm::vec3>("Vec3",
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z);

    // --- TransformComponent ---
    lua.new_usertype<TransformComponent>("TransformComponent",
        "position",      &TransformComponent::position,
        "rotationEuler", &TransformComponent::rotationEuler,
        "scale",         &TransformComponent::scale);

    // --- Entity ---
    // `tag`: string inmutable derivado de TagComponent.
    // `transform`: referencia al TransformComponent para mutacion en-place.
    lua.new_usertype<Entity>("Entity",
        "tag", sol::property([](Entity& e) -> std::string {
            return e.hasComponent<TagComponent>()
                ? e.getComponent<TagComponent>().name
                : std::string{};
        }),
        "transform", sol::property([](Entity& e) -> TransformComponent& {
            return e.getComponent<TransformComponent>();
        }));

    // --- log.info / log.warn ---
    sol::table logTable = lua.create_named_table("log");
    logTable.set_function("info", [](const std::string& msg) {
        Log::script()->info("{}", msg);
    });
    logTable.set_function("warn", [](const std::string& msg) {
        Log::script()->warn("{}", msg);
    });

    // --- hud (Hito 20 Bloque 5) ---
    // Tabla global para que cualquier script pueda mutar el HUD del juego
    // y togglear la pausa. El estado real vive en `GameState` (singleton);
    // los scripts solo leen/escriben copias o flags.
    sol::table hudTable = lua.create_named_table("hud");
    hudTable.set_function("setHp",     [](int v) { GameState::hud().hp   = v; });
    hudTable.set_function("setAmmo",   [](int v) { GameState::hud().ammo = v; });
    hudTable.set_function("setPaused", [](bool v) { GameState::paused() = v; });
    hudTable.set_function("getHp",     []() { return GameState::hud().hp; });
    hudTable.set_function("getAmmo",   []() { return GameState::hud().ammo; });
    hudTable.set_function("getPaused", []() { return GameState::paused(); });

    // --- engine.exposed (Hito 24) ---
    // Registra una exposed property + devuelve el override de la
    // entidad (si existe) o el default. Inferencia de tipo del default:
    // number/bool/string/table-de-3-numbers (=> vec3). Otros tipos
    // (function, userdata, table generica) se ignoran con warning.
    sol::table engineTable = lua.create_named_table("engine");
    engineTable.set_function("exposed",
        [scriptComponent, &lua](const std::string& name,
                                  sol::object defaultVal) -> sol::object {
        // Sin ScriptComponent (tests / scope reducido) devolvemos el
        // default sin registrar nada.
        if (scriptComponent == nullptr) return defaultVal;

        // Inferir tipo del default + envolverlo en ExposedValue.
        ExposedValue defVar;
        ExposedType type = ExposedType::Number;
        bool ok = true;
        if (defaultVal.is<bool>()) {
            defVar = defaultVal.as<bool>();
            type = ExposedType::Bool;
        } else if (defaultVal.is<f32>() || defaultVal.is<int>()) {
            defVar = static_cast<f32>(defaultVal.as<double>());
            type = ExposedType::Number;
        } else if (defaultVal.is<std::string>()) {
            defVar = defaultVal.as<std::string>();
            type = ExposedType::String;
        } else if (defaultVal.is<sol::table>()) {
            sol::table t = defaultVal.as<sol::table>();
            if (t.size() == 3) {
                glm::vec3 v(static_cast<f32>(t.get_or(1, 0.0)),
                            static_cast<f32>(t.get_or(2, 0.0)),
                            static_cast<f32>(t.get_or(3, 0.0)));
                defVar = v;
                type = ExposedType::Vec3;
            } else {
                ok = false;
            }
        } else {
            ok = false;
        }

        if (!ok) {
            Log::script()->warn(
                "engine.exposed('{}'): tipo de default no soportado, ignorado",
                name);
            return defaultVal;
        }

        // Registrar metadata en exposedProps (deduplicado por nombre).
        auto& props = scriptComponent->exposedProps;
        auto it = std::find_if(props.begin(), props.end(),
            [&name](const ExposedProperty& p) { return p.name == name; });
        if (it == props.end()) {
            props.push_back(ExposedProperty{name, type, defVar});
        } else {
            it->type = type;
            it->defaultValue = defVar;
        }

        // Si hay override, devolverlo. Sino, el default original.
        auto& over = scriptComponent->overrides;
        auto ov = over.find(name);
        if (ov == over.end()) return defaultVal;

        // Convertir el ExposedValue del override a sol::object para
        // devolver al script. std::visit en el variant.
        return std::visit([&lua](auto&& arg) -> sol::object {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, f32>) {
                return sol::make_object(lua, static_cast<double>(arg));
            } else if constexpr (std::is_same_v<T, bool>) {
                return sol::make_object(lua, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return sol::make_object(lua, arg);
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
                sol::table t = lua.create_table();
                t[1] = arg.x; t[2] = arg.y; t[3] = arg.z;
                return sol::make_object(lua, t);
            }
            return sol::nil;
        }, ov->second);
    });

    // --- physics (Hito 33) ---
    // Solo si el caller proveyo un PhysicsWorld (tests headless sin Jolt
    // pasan nullptr y no exponen la tabla). API:
    //   local hit = physics.raycast({x,y,z}, {dx,dy,dz}, maxDist [, ignoredBodyId])
    //   if hit.hit then
    //     log.info(string.format("at %f %f %f", hit.point[1],
    //                              hit.point[2], hit.point[3]))
    //   end
    //
    // Hito 34 B: el 4to argumento es opcional — si se pasa un bodyId !=0,
    // ese body se descarta del cast (util para "ignore self" en armas FPS
    // disparadas desde un body que no quiere autodetectarse).
    //
    // Convencion: origin/direction son tablas {x, y, z} (1-indexed por
    // Lua). El return tambien usa tablas para vec3 (point, normal) — en
    // Hito 24 ya establecimos esa convencion para vec3 cross-binding.
    if (physics != nullptr) {
        sol::table physicsTable = lua.create_named_table("physics");
        physicsTable.set_function("raycast",
            [&lua, physics](sol::table origin, sol::table dir, f32 maxDist,
                             sol::optional<u32> ignoredBodyId) -> sol::object {
                const glm::vec3 o(
                    static_cast<f32>(origin.get_or(1, 0.0)),
                    static_cast<f32>(origin.get_or(2, 0.0)),
                    static_cast<f32>(origin.get_or(3, 0.0)));
                const glm::vec3 d(
                    static_cast<f32>(dir.get_or(1, 0.0)),
                    static_cast<f32>(dir.get_or(2, 0.0)),
                    static_cast<f32>(dir.get_or(3, 0.0)));
                const u32 ignored = ignoredBodyId.value_or(0u);
                const auto h = physics->raycast(o, d, maxDist, ignored);
                sol::table result = lua.create_table();
                result["hit"]      = h.hit;
                result["distance"] = h.distance;
                result["bodyId"]   = h.bodyId;
                sol::table p = lua.create_table();
                p[1] = h.point.x; p[2] = h.point.y; p[3] = h.point.z;
                result["point"]  = p;
                sol::table n = lua.create_table();
                n[1] = h.normal.x; n[2] = h.normal.y; n[3] = h.normal.z;
                result["normal"] = n;
                return sol::make_object(lua, result);
            });
    }

    // self como global.
    lua["self"] = self;
}

} // namespace Mood
