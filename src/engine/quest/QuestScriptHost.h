#pragma once

// QuestScriptHost (break-A3, 2026-05-23): sol::state dedicada al sistema
// de quests. Provee evaluator + executor para los predicates y rewards
// del QuestSystem, independiente de las entidades-script.
//
// Problema pre-A3 (placebo A3 del cierre Fase 2):
//   `setupQuestBindings` (LuaBindings_Quest) seteaba el evaluator y el
//   executor del QuestSystem capturando `&lua` de la sol::state del
//   script al cual estaba siendo bindeado. Eso significaba:
//   - Si la escena tenia 0 entidades con ScriptComponent -> evaluator/
//     executor quedaban en nullptr -> `tick()` nunca auto-completaba
//     ningun objective Collect/Talk/Reach. El Quest Log se veia
//     congelado.
//   - El evaluator quedaba apuntando a la sol::state del ULTIMO script
//     cargado (acoplamiento fragil + lifetime intrincado).
//
// Solucion (clon del patron DialogScriptHost de F2H48.1):
//   - sol::state propia del host, viva mientras el editor/player corre.
//   - Bindings minimos: `inventory` (Collect predicates), `dialog`
//     (Talk/Reach predicates + set_var rewards), `log`. No `self`, no
//     `engine.exposed`, no `physics` — esto es sandbox global de quest,
//     no script de entity.
//   - `init()` registra los hooks en `QuestSystem::setEvaluator/
//     setExecutor` una sola vez al arranque del editor/player.
//   - `setSceneAndAssets(...)` se llama al entrar/salir Play Mode para
//     que el binding `inventory` resuelva player + items con la scene
//     actual (mismo patron que DialogScriptHost::setSceneAndAssets).
//
// Lifetime:
//   - El host vive durante toda la sesion del editor/player.
//   - `reset()` destruye la sol::state (clear total). Se llama desde
//     el shutdown del app — no en exitPlayMode (los hooks tienen que
//     seguir vivos entre Play Mode sessions del editor).
//   - `QuestSystem::clearHooks()` se llama al exitPlayMode/loadScene
//     desde el caller; el host RE-REGISTRA via `init()` si emerge un
//     nuevo Play. La logica de re-registro es idempotente (flag
//     g_hooksRegistered).

#include <string>

namespace Mood {
class Scene;
class AssetManager;
} // namespace Mood

namespace Mood::Quest::QuestScriptHost {

/// @brief Inicializa la sol::state (si no existe) y registra los hooks
///        en `QuestSystem::setEvaluator/setExecutor`. Idempotente — la
///        segunda llamada NO recrea la state, solo asegura hooks.
///        Llamar una vez al arrancar el editor/player.
void init();

/// @brief Setea `Scene*` + `AssetManager*` que la tabla `inventory` del
///        host usa para resolver player + items. Llamar al entrar Play
///        Mode (editor) o al cargar un mapa (player). nullptr/nullptr
///        deja la tabla pero las queries player-implicit retornan
///        false/0/{} silenciosamente — util para reset al exitPlayMode.
void setSceneAndAssets(Mood::Scene* scene, Mood::AssetManager* assets);

/// @brief Evalua un predicate Lua boolean (ej. `inventory.count('items/x') >= 3`).
///        Empty => true (sentido del contrato del QuestSystem). Errores
///        de parse/runtime => false + log warn.
bool evaluate(const std::string& expr);

/// @brief Ejecuta codigo Lua sin retorno (rewards: `inventory.add(...)`,
///        `dialog.set_var(...)`). "" no se invoca. Errores => log warn.
void execute(const std::string& code);

/// @brief Tira la sol::state. NO toca QuestSystem state. Llamar al
///        shutdown del app.
void reset();

/// @brief True si init() fue llamado y la sol::state esta viva. Util
///        para tests + debug.
bool isInitialized();

} // namespace Mood::Quest::QuestScriptHost
