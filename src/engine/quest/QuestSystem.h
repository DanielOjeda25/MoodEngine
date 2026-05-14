#pragma once

// QuestSystem (F2H53 Bloque C): runtime de quests. State machine pura,
// singleton namespace (paridad con `DialogSystem` F2H48). Sin deps a
// sol2 / ImGui — testeable headless con doctest.
//
// Modelo de estados:
//   NotStarted -> Active -> Complete  (camino feliz)
//                       \-> Failed     (fail explicito)
// (No hay transicion Complete -> Active; quests son one-shot. Si el dev
//  quiere "repetable", llama `reset(id)` o pasa por save/load.)
//
// Filosofia engine-grade strict (regla VISION):
// - El motor NO conoce semantica de "main quest" vs "side". Solo
//   transiciona estado y dispara hooks. El dev decide cuando se gana XP.
// - Los predicates de objectives son strings Lua generados a partir de
//   los campos type-specific del `Quest::Asset::Objective`. La evaluacion
//   real la hace el callback `LuaEvaluator` inyectado por el script host
//   (mismo modelo que F2H48.1 dialog hooks). Eso permite testear el
//   QuestSystem sin tener una sol::state real.
// - Las rewards (item / var / lua) se aplican como llamadas Lua via
//   `LuaExecutor` inyectado — el motor genera el codigo, el host lo
//   ejecuta. Engine-grade preservado: cualquier reward custom emerge de
//   la primitiva `lua` sin que el motor agregue knobs hardcoded.
//
// Threading: singleton no-threadsafe. Se asume llamado desde el thread
// principal del engine (mismo orden de tick que el resto de sistemas
// game-side). Si emerge multi-thread, sumar lock interno.

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h"

#include <functional>
#include <string>
#include <vector>

namespace Mood::Quest {
class Asset;
struct Objective;
struct Reward;
}

namespace Mood::Quest::QuestSystem {

/// @brief Estado de un quest en runtime.
enum class State : u8 {
    NotStarted = 0,  // nunca se llamo start, o se reseteo
    Active     = 1,  // start() OK, objectives en evaluacion cada tick
    Complete   = 2,  // todos los objectives done; rewards aplicadas
    Failed     = 3,  // fail() explicito
};

/// @brief Progreso individual de un objective.
struct ObjectiveProgress {
    bool completed = false;
};

/// @brief Snapshot runtime de un quest activo. NO se serializa (la
///        persistencia entera del estado de quests es un Bloque aparte —
///        Bloque H).
struct ActiveQuest {
    QuestAssetId id = 0;
    State        state = State::NotStarted;
    std::vector<ObjectiveProgress> objectives;  // paralelo a Asset::objectives
};

// =============================================================
// Lifecycle de quests
// =============================================================

/// @brief Inicia un quest. Carga el asset si no esta cacheado, crea el
///        registro `ActiveQuest` con state=Active + objectives inicializados
///        (todos `completed=false`), dispara el `StartHook`.
/// @return true si arranco; false si:
///         - id es `missingQuestId()` (slot 0)
///         - el quest no tiene objectives (asset invalido)
///         - el quest ya esta Active o Complete
bool start(QuestAssetId id, AssetManager& am);

/// @brief Fuerza Complete manualmente (sin chequear objectives). Aplica
///        rewards via el `LuaExecutor`. Dispara `CompleteHook`. Util para
///        casos donde el dev quiere atajar el flujo (ej. "el NPC te
///        completa la quest por ser amable").
/// @return true si transiciono; false si el quest no esta Active.
bool complete(QuestAssetId id, AssetManager& am);

/// @brief Marca el quest como Failed. NO aplica rewards. Dispara `FailHook`.
/// @return true si transiciono; false si el quest no esta Active.
bool fail(QuestAssetId id);

/// @brief Marca un objective como completado manualmente (escape hatch para
///        Lua: `quest.objective_complete(id, "find_cat")`). NO chequea
///        predicate — confia en el caller. Util para objectives custom_lua
///        donde el dev quiere marcar como done desde un callback de gameplay.
///        Si todos los objectives quedan completados tras esta llamada, el
///        quest auto-completa.
/// @return true si encontro y marco; false si quest no Active o objectiveId no existe.
bool objectiveComplete(QuestAssetId id, const std::string& objectiveId,
                        AssetManager& am);

// =============================================================
// Tick — evaluacion automatica de predicates
// =============================================================

/// @brief Itera todos los quests Active, evalua los predicates de cada
///        objective no-completado via `LuaEvaluator`. Si todos los
///        objectives de un quest quedan done, auto-completa (incluye
///        aplicar rewards + disparar `CompleteHook`).
///        Llamar 1x por frame desde el game loop, mismo orden que el
///        DialogSystem (despues de Lua scripts, antes del render).
void tick(AssetManager& am);

// =============================================================
// Queries
// =============================================================

State stateOf(QuestAssetId id);
bool  isActive  (QuestAssetId id);
bool  isComplete(QuestAssetId id);
bool  isFailed  (QuestAssetId id);

/// @brief Acceso const al vector de quests trackeados. Para UI (Quest Log
///        panel, HUD tracker) sin copia.
const std::vector<ActiveQuest>& snapshot();

// =============================================================
// Tracked quest (HUD)
// =============================================================

/// @brief El "tracked quest" es el que aparece destacado en el HUD widget.
///        Convencion Skyrim/RPG estandar. Puede ser cualquier id, incluso
///        uno que no este en `snapshot()` — el HUD lo skipea sin trackear.
///        `setTracked(missingQuestId())` desactiva tracking.
void setTracked(QuestAssetId id);

/// @brief Id del quest trackeado actualmente. 0 = ninguno.
QuestAssetId tracked();

// =============================================================
// Hooks Lua (predicate evaluator + script executor)
// =============================================================

using LuaEvaluator = std::function<bool(const std::string& expr)>;
using LuaExecutor  = std::function<void(const std::string& code)>;

/// @brief Inyecta el evaluador de predicates. Llamado desde el script host
///        (ej. `DialogScriptHost` o un `QuestScriptHost` futuro). Si nullptr,
///        los predicates de objectives nunca se satisfacen automaticamente —
///        el dev tiene que usar `objectiveComplete(...)` manualmente.
void setEvaluator(LuaEvaluator fn);

/// @brief Inyecta el ejecutor para aplicar rewards (genera Lua tipo
///        `inventory.add('items/x', 5)` y deja que el host lo ejecute).
///        Si nullptr, las rewards no se aplican (queda solo el state
///        Complete + el hook).
void setExecutor(LuaExecutor fn);

// =============================================================
// Game hooks (notificaciones para el dev)
// =============================================================

using StartHook    = std::function<void(QuestAssetId)>;
using CompleteHook = std::function<void(QuestAssetId)>;
using FailHook     = std::function<void(QuestAssetId)>;

/// @brief Disparados tras la transicion exitosa. NO veto-style (despues del
///        cambio de state). Si el dev registra 2 veces el mismo hook, el
///        segundo override silenciosamente al primero — mismo patron que
///        InventoryHooks (F2H52 Bloque F).
void setStartHook   (StartHook    fn);
void setCompleteHook(CompleteHook fn);
void setFailHook    (FailHook     fn);

// =============================================================
// Lifecycle (tests + scene reset)
// =============================================================

/// @brief Limpia TODOS los hooks (Lua + game). Llamar en teardown de tests
///        o al cargar una scene nueva si el script host se re-inicializa.
void clearHooks();

/// @brief Resetea TODO el state runtime (quests activos, tracked).
///        Hooks quedan intactos. Util para tests aislados + scene unload.
void reset();

// =============================================================
// Save / Load (F2H53 Bloque H)
// =============================================================

/// @brief Inserta un quest directamente al state runtime con un progreso
///        arbitrario. NO dispara hooks (start/complete/fail) ni aplica
///        rewards — esto es restauracion pura, no transicion. Si el
///        quest ya estaba en el state, lo sobreescribe.
///
///        Pensado SOLO para `SaveLoad::load(...)`. En gameplay normal el
///        dev usa `start/complete/fail/objectiveComplete` que SI disparan
///        hooks. No expuesto a Lua para evitar abusos.
///
///        `objectiveDone` se alinea por indice contra `Asset::objectives`.
///        Si el length no coincide con el asset (ej. el .moodquest cambio
///        de objectives entre save y load), se trunca / padea con `false`.
void restore(QuestAssetId id, State state,
              const std::vector<bool>& objectiveDone,
              AssetManager& am);

// =============================================================
// Helpers (publicos para tests)
// =============================================================

/// @brief Genera el predicate Lua para un objective dado a partir del
///        type + campos type-specific. Exportado para tests y para que el
///        Quest Log panel lo pueda mostrar como hint al dev. NO ejecuta;
///        solo construye el string.
///        - Collect:   `inventory.count('<item_path>') >= <min_quantity>`
///        - Talk:      `dialog.has_var('<var_name>')`
///        - Reach:     `dialog.has_var('<var_name>')`
///        - CustomLua: el `custom_predicate` tal cual
///        - Vacios / unknown: ""  (evaluator devuelve false)
std::string generatePredicate(const Objective& o);

/// @brief Genera el codigo Lua para aplicar una reward. Exportado para
///        tests.
///        - Item: `inventory.add('<item_path>', <quantity>)`
///        - Var:  `dialog.set_var('<var_name>', '<var_value>')`
///        - Lua:  el `lua_script` tal cual
///        - Vacios: ""
std::string generateRewardCode(const Reward& r);

} // namespace Mood::Quest::QuestSystem
