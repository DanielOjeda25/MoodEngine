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
#include "engine/scripting/exposed/ExposedProperty.h"  // ExposedValue (Hito 41 B)

#include <entt/entt.hpp>
#include <sol/sol.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace Mood {

class PhysicsWorld;
class Scene;

class ScriptSystem {
public:
    /// @brief Corre `onUpdate(self, dt)` en cada entidad con
    ///        ScriptComponent cargado. Carga / recarga archivos al vuelo.
    /// @param physics Hito 33: opcional. Si presente se cablea a la tabla
    ///        `physics` de Lua (raycast + futuros). Tests sin Jolt pasan
    ///        nullptr y los scripts que no usen `physics.*` siguen
    ///        funcionando.
    void update(Scene& scene, f32 dt, PhysicsWorld* physics = nullptr);

    /// @brief Tira todos los sol::state. Se llama desde
    ///        `EditorApplication::rebuildSceneFromMap` porque al hacer
    ///        `registry.clear()` los entt::entity quedan invalidados y
    ///        sus states quedarian huerfanos.
    void clear() {
        m_states.clear();
        m_mtimes.clear();
        m_hotReloadAccumulator = 0.0f;
    }

    /// @brief Hito 33: llama una funcion Lua opcional definida en el
    ///        script de la entidad (ej. "on_trigger_enter"). Si la
    ///        funcion no existe en el sol::state, no-op silencioso.
    ///        Errores de Lua se loguean al canal `script` y se acumulan
    ///        en `lastError` (mismo flujo que `onUpdate`).
    void dispatchEvent(entt::entity entity, const char* eventName);

    /// @brief Hito 37 B: variante con un argumento `u32` (typicamente
    ///        el handle raw de un entt::entity). Util para callbacks
    ///        que reportan "que body activo el trigger". El script
    ///        recibe el id como numero Lua: `function on_xxx(bodyId)`.
    ///        Miss silencioso + log de errores igual que la variante
    ///        sin args.
    void dispatchEvent(entt::entity entity, const char* eventName, u32 arg);

    /// @brief Hito 41 B: snapshot de globals top-level del sol::state
    ///        de la entidad, filtrando por tipos del `ExposedValue`
    ///        variant (number, bool, string, vec3 array de 3 floats).
    ///        Tablas, funciones, userdata se omiten silencio. Si la
    ///        entidad no tiene state cargado, devuelve mapa vacio.
    std::unordered_map<std::string, ExposedValue> captureGlobals(
        entt::entity entity) const;

    /// @brief Hito 41 B: restaura globals desde un mapa al sol::state
    ///        de la entidad. Si el state aun no existe (ej. el script
    ///        todavia no se cargo), guarda los pendings y los aplica
    ///        en el proximo update() tras la primera carga.
    void restoreGlobals(entt::entity entity,
                         const std::unordered_map<std::string, ExposedValue>& globals);

private:
    // Mapa entidad -> su sol::state persistente. Usamos unordered_map para
    // que la direccion del sol::state no se invalide al agregar otros
    // (ownership via unique_ptr evita mover el state).
    std::unordered_map<entt::entity, std::unique_ptr<sol::state>> m_states;

    // Hito 41 B: globals pendientes de aplicar cuando un sol::state
    // aun no existe al hacer restoreGlobals (ej. el script no se cargo
    // todavia). Se aplican en el proximo update() post-load.
    std::unordered_map<entt::entity,
                        std::unordered_map<std::string, ExposedValue>> m_pendingGlobals;

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
