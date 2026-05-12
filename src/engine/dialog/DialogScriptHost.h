#pragma once

// DialogScriptHost (F2H48.1): sol::state dedicada al sistema de
// dialogs. Provee dos funciones thin que el `DialogSystem` consume
// como evaluator/executor para los hooks `condition_lua` y
// `on_select_lua` de cada Choice.
//
// Diseno:
// - Singleton namespace (paridad con `DialogSystem`).
// - Una sola sol::state compartida — los `condition_lua`/`on_select_lua`
//   de TODOS los dialogs corren contra el mismo entorno. Las vars
//   se persisten en `GameState::dialogVars()` (decision F2H48 #2).
// - Bindings expuestos: tabla `dialog` (set_var/get_var/has_var/
//   clear_vars/isActive/currentNode), tabla `hud` (interact_prompt
//   + getters/setters comunes), tabla `log`.
//   NO `self`, NO `engine.exposed`, NO `physics` — esto es sandbox
//   global, no script de entity.
// - Init perezoso: la primera llamada a `evaluate`/`execute` construye
//   la sol::state si no esta. `reset()` la tira (clear de vars + state
//   fresh) — se llama al exitPlayMode.
//
// Wireup: `init()` se llama una vez en el arranque del editor/player
// y registra los hooks en `DialogSystem::setEvaluator/setExecutor`.
// Despues, cualquier `condition_lua`/`on_select_lua` que el dev
// escriba en un Choice se ejecuta automaticamente via este host.

#include <string>

namespace Mood {
class Scene;
class AssetManager;
} // namespace Mood

namespace Mood::Dialog::DialogScriptHost {

/// @brief Inicializa la sol::state (si no existe) y registra los
///        hooks en `DialogSystem::setEvaluator/setExecutor`. Idempotente
///        — llamadas repetidas no recrean la state, solo re-registran.
///        Llamar una vez al arrancar el editor/player.
void init();

/// @brief F2H52 Bloque G: setea `Scene*` + `AssetManager*` que la tabla
///        `inventory` del host usa para resolver player + items. Llamar
///        cuando una scene/assets nueva esta lista (entrar Play Mode en
///        el editor, cargar mapa en el player). Si `init()` ya corrio,
///        re-bindea la tabla `inventory` con los nuevos punteros. Pasar
///        nullptr/nullptr deja la tabla pero las queries player-implicit
///        retornan false/0/{} silenciosamente — util para reset al
///        exitPlayMode.
void setSceneAndAssets(Mood::Scene* scene, Mood::AssetManager* assets);

/// @brief Evalua una expresion Lua boolean. Si la expr es vacia,
///        retorna true (paridad con el contrato del DialogSystem).
///        Si hay error de parse/runtime, loggea + retorna false (la
///        choice no aparece — fail-safe defensivo).
bool evaluate(const std::string& expr);

/// @brief Ejecuta codigo Lua sin retorno (statement). "" no se invoca.
///        Errores se loggean al canal `script`.
void execute(const std::string& code);

/// @brief Tira la sol::state (clear total). NO toca `GameState::
///        dialogVars()` — esos persisten por design. Llamar al
///        exitPlayMode para liberar memoria + reset eventuales globals
///        del host que el dev haya seteado durante la partida.
void reset();

/// @brief True si init() fue llamado y la sol::state esta viva.
///        Util para tests + debug.
bool isInitialized();

} // namespace Mood::Dialog::DialogScriptHost
