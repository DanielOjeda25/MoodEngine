#include "systems/ScriptSystem.h"

#include "core/Log.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scripting/LuaBindings.h"

#include <system_error>
#include <type_traits>
#include <unordered_set>

namespace Mood {

void ScriptSystem::update(Scene& scene, f32 dt, PhysicsWorld* physics) {
    // Throttle del check de mtime: una vez cada k_hotReloadInterval. El
    // flag se aplica al lookup dentro del forEach.
    m_hotReloadAccumulator += dt;
    bool checkMtimes = false;
    if (m_hotReloadAccumulator >= k_hotReloadInterval) {
        checkMtimes = true;
        m_hotReloadAccumulator = 0.0f;
    }

    scene.forEach<ScriptComponent>([&](Entity e, ScriptComponent& sc) {
        if (sc.path.empty()) return;

        // (Re)carga inicial: crear sol::state, bindings y ejecutar file.
        // `loaded` se pone a false en errores o por "Recargar" del Inspector.
        if (!sc.loaded) {
            auto state = std::make_unique<sol::state>();
            // Hito 24: clear de exposedProps antes de cargar — el binding
            // las redescubre. Asi un script que removio un `engine.exposed`
            // entre reloads no deja la prop fantasma en el Inspector.
            sc.exposedProps.clear();
            setupLuaBindings(*state, e, &sc, physics);

            sol::protected_function_result r = state->safe_script_file(
                sc.path, sol::script_pass_on_error);
            if (!r.valid()) {
                sol::error err = r;
                sc.lastError = err.what();
                Log::script()->warn("Error cargando '{}': {}", sc.path, sc.lastError);
                return; // script desactivado hasta que se repare o recargue
            }
            sc.loaded = true;
            sc.lastError.clear();
            m_states[e.handle()] = std::move(state);

            // Seed de mtime para detectar edits posteriores sin disparar
            // un reload espurio en el frame siguiente.
            std::error_code ec;
            auto mt = std::filesystem::last_write_time(sc.path, ec);
            if (!ec) m_mtimes[sc.path] = mt;

            Log::script()->info("Cargado '{}'", sc.path);

            // Hito 41 B: si habia globals pendientes (restoreGlobals
            // llamado antes de la primera carga), aplicarlas ahora.
            auto pending = m_pendingGlobals.find(e.handle());
            if (pending != m_pendingGlobals.end()) {
                restoreGlobals(e.handle(), pending->second);
                m_pendingGlobals.erase(pending);
            }
        }
        // Hot-reload: si paso el throttle y el archivo cambio en disco,
        // re-ejecutamos sobre el MISMO sol::state para preservar globals.
        else if (checkMtimes) {
            std::error_code ec;
            auto mt = std::filesystem::last_write_time(sc.path, ec);
            if (!ec) {
                auto mit = m_mtimes.find(sc.path);
                if (mit == m_mtimes.end() || mit->second != mt) {
                    m_mtimes[sc.path] = mt;
                    auto sit = m_states.find(e.handle());
                    if (sit != m_states.end()) {
                        sol::state& lua = *sit->second;
                        // Hito 24: re-clear de exposedProps en hot-reload.
                        // El chunk top-level corre de nuevo y los engine.exposed
                        // los re-registran.
                        sc.exposedProps.clear();
                        sol::protected_function_result r = lua.safe_script_file(
                            sc.path, sol::script_pass_on_error);
                        if (!r.valid()) {
                            sol::error err = r;
                            sc.lastError = err.what();
                            sc.loaded = false;
                            Log::script()->warn("Error recargando '{}': {}",
                                                 sc.path, sc.lastError);
                            return;
                        }
                        sc.lastError.clear();
                        Log::script()->info("Recargado '{}'", sc.path);
                    }
                }
            }
        }

        auto it = m_states.find(e.handle());
        if (it == m_states.end()) return;
        sol::state& lua = *it->second;

        // Llamar onUpdate(self, dt) si existe. `self` ya esta como global
        // desde setupLuaBindings; pasamos tambien como primer arg para
        // mantener la convencion `function onUpdate(self, dt)`.
        sol::protected_function onUpdate = lua["onUpdate"];
        if (!onUpdate.valid()) {
            // Script sin onUpdate: valido, pero inert. No spamear warns.
            return;
        }

        sol::protected_function_result r = onUpdate(lua["self"], dt);
        if (!r.valid()) {
            sol::error err = r;
            sc.lastError = err.what();
            sc.loaded = false; // desactivar hasta recarga explicita
            Log::script()->warn("Error en onUpdate de '{}': {}", sc.path, sc.lastError);
            // Preservar el state en el mapa es OK: lo re-creamos al reload.
        }
    });
}

void ScriptSystem::dispatchEvent(entt::entity entity, const char* eventName) {
    auto it = m_states.find(entity);
    if (it == m_states.end()) return;
    sol::state& lua = *it->second;

    sol::protected_function fn = lua[eventName];
    if (!fn.valid()) return;  // script no define el evento — OK

    sol::protected_function_result r = fn();
    if (!r.valid()) {
        sol::error err = r;
        Log::script()->warn("Error en {}: {}", eventName, err.what());
    }
}

std::unordered_map<std::string, ExposedValue> ScriptSystem::captureGlobals(
        entt::entity entity) const {
    std::unordered_map<std::string, ExposedValue> out;
    auto it = m_states.find(entity);
    if (it == m_states.end()) return out;
    sol::state& lua = *it->second;
    // Hito 41 B: walk de globals top-level. Filtramos por tipos del
    // ExposedValue variant. Tablas, funciones, userdata se omiten.
    // Hito 41 fix-up: try/catch defensivo — sol2 puede lanzar excepciones
    // o assert() en algunos tipos exoticos (userdata sin __index, tablas
    // con weak refs). Capturamos cualquier fallo y continuamos con los
    // globals que si pudieron leerse.
    static const std::unordered_set<std::string> kReserved{
        "engine", "log", "hud", "physics", "self",
        "string", "table", "math", "io", "os", "package",
        "coroutine", "debug", "bit32", "utf8",
        // funciones built-in del juego (eventos)
        "onUpdate", "on_trigger_enter", "on_trigger_exit",
        "on_trigger_stay", "on_trigger_body_enter",
        "on_trigger_body_exit", "on_trigger_body_stay"
    };
    try {
        for (const auto& kv : lua.globals()) {
            try {
                sol::object key = kv.first;
                sol::object val = kv.second;
                if (!key.is<std::string>()) continue;
                const std::string name = key.as<std::string>();
                if (!name.empty() && name[0] == '_') continue;
                if (kReserved.count(name)) continue;
                // Lua object type: usar `get_type()` antes que `is<T>()`
                // para evitar conversiones implicitas que pueden assert.
                const sol::type t = val.get_type();
                if (t == sol::type::boolean) {
                    out[name] = ExposedValue{val.as<bool>()};
                } else if (t == sol::type::number) {
                    out[name] = ExposedValue{val.as<f32>()};
                } else if (t == sol::type::string) {
                    out[name] = ExposedValue{val.as<std::string>()};
                } else if (t == sol::type::table) {
                    sol::table tbl = val.as<sol::table>();
                    if (tbl.size() == 3) {
                        sol::object v0 = tbl[1];
                        sol::object v1 = tbl[2];
                        sol::object v2 = tbl[3];
                        if (v0.get_type() == sol::type::number &&
                            v1.get_type() == sol::type::number &&
                            v2.get_type() == sol::type::number) {
                            out[name] = ExposedValue{glm::vec3(
                                v0.as<f32>(), v1.as<f32>(), v2.as<f32>())};
                        }
                    }
                }
                // function/userdata/thread → omit silencioso.
            } catch (const std::exception& e) {
                Log::script()->warn(
                    "captureGlobals: skip global por excepcion: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        Log::script()->warn(
            "captureGlobals: iteracion abortada por excepcion: {}", e.what());
    }
    return out;
}

void ScriptSystem::restoreGlobals(entt::entity entity,
        const std::unordered_map<std::string, ExposedValue>& globals) {
    auto it = m_states.find(entity);
    if (it == m_states.end()) {
        // State aun no existe (script no cargado). Stash hasta el
        // proximo update() post-carga.
        m_pendingGlobals[entity] = globals;
        return;
    }
    sol::state& lua = *it->second;
    for (const auto& [name, value] : globals) {
        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, glm::vec3>) {
                sol::table t = lua.create_table();
                t[1] = v.x; t[2] = v.y; t[3] = v.z;
                lua[name] = t;
            } else {
                lua[name] = v;
            }
        }, value);
    }
}

void ScriptSystem::dispatchEvent(entt::entity entity, const char* eventName, u32 arg) {
    auto it = m_states.find(entity);
    if (it == m_states.end()) return;
    sol::state& lua = *it->second;

    sol::protected_function fn = lua[eventName];
    if (!fn.valid()) return;

    sol::protected_function_result r = fn(arg);
    if (!r.valid()) {
        sol::error err = r;
        Log::script()->warn("Error en {}({}): {}", eventName, arg, err.what());
    }
}

} // namespace Mood
