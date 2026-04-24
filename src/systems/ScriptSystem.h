#pragma once

// Sistema que evalua cada frame las entidades con ScriptComponent.
// Para cada una:
//   1) La primera vez (o tras edicion del path), crea un sol::state
//      nuevo, registra los bindings via `setupLuaBindings(state, entity)`
//      y carga el archivo Lua.
//   2) Si el archivo define `onUpdate(self, dt)`, lo llama cada frame.
//   3) Errores de carga o ejecucion se loguean al canal `script` y se
//      guardan en `ScriptComponent::lastError`. El script queda
//      desactivado hasta que se solicite recarga.
//
// Cada entidad tiene su propio sol::state (islas aisladas; variables
// globales del script no cruzan entre entidades).

#include "core/Types.h"

#include <entt/entt.hpp>
#include <sol/sol.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Mood {

class Scene;

class ScriptSystem {
public:
    /// @brief Corre `onUpdate(self, dt)` en cada entidad con
    ///        ScriptComponent cargado. Carga / recarga archivos al vuelo.
    void update(Scene& scene, f32 dt);

    /// @brief Tira todos los sol::state. Se llama desde
    ///        `EditorApplication::rebuildSceneFromMap` porque al hacer
    ///        `registry.clear()` los entt::entity quedan invalidados y
    ///        sus states quedarian huerfanos.
    void clear() {
        m_states.clear();
        m_mtimes.clear();
        m_hotReloadAccumulator = 0.0f;
    }

private:
    // Mapa entidad -> su sol::state persistente. Usamos unordered_map para
    // que la direccion del sol::state no se invalide al agregar otros
    // (ownership via unique_ptr evita mover el state).
    std::unordered_map<entt::entity, std::unique_ptr<sol::state>> m_states;

    // Hot-reload: cache de mtime por path para detectar cambios. Compartido
    // entre entidades que apunten al mismo .lua, pero cada una recarga sobre
    // SU sol::state (islas aisladas, plan Hito 8).
    std::unordered_map<std::string, std::filesystem::file_time_type> m_mtimes;

    // Throttle del stat(): acumula dt y consulta mtime solo cada
    // `k_hotReloadInterval`. Evita spammear I/O cada frame.
    f32 m_hotReloadAccumulator = 0.0f;
    static constexpr f32 k_hotReloadInterval = 0.5f;
};

} // namespace Mood
