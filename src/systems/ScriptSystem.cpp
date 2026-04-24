#include "systems/ScriptSystem.h"

#include "core/Log.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scripting/LuaBindings.h"

#include <system_error>

namespace Mood {

void ScriptSystem::update(Scene& scene, f32 dt) {
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
            setupLuaBindings(*state, e);

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

} // namespace Mood
