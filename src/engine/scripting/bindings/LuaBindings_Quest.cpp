// F2H53 Bloque E — bindings Lua de la tabla `quest`. Split del
// LuaBindings.cpp principal (mismo patron que `inventory`).
//
// API expuesta a los scripts:
//
//   - Lifecycle (operan sobre path logico — el motor resuelve a id via
//     AssetManager. Si el path no existe, retornan false + log warn):
//       quest.start(path)            -- bool
//       quest.complete(path)         -- bool
//       quest.fail(path)             -- bool
//       quest.objective_complete(path, objective_id) -- bool
//
//   - Queries:
//       quest.state(path)            -- string "not_started"|"active"|"complete"|"failed"
//       quest.is_active(path)        -- bool
//       quest.is_complete(path)      -- bool
//       quest.is_failed(path)        -- bool
//       quest.snapshot()             -- table {path, state, objectives_done, objectives_total}
//
//   - Tracked (HUD):
//       quest.set_tracked(path)      -- "" o nil -> desactivar
//       quest.tracked()              -- path o "" si no hay
//
//   - Game hooks:
//       quest.on_start(fn)           -- fn(path)
//       quest.on_complete(fn)        -- fn(path)
//       quest.on_fail(fn)            -- fn(path)
//
// Setup: AL REGISTRAR los bindings tambien se instalan el `LuaEvaluator` y
// `LuaExecutor` del QuestSystem usando la misma sol::state. Eso permite que
// QuestSystem::tick() evalue predicates como `inventory.count(...) >= 3`
// directamente contra los bindings de inventory ya registrados.
//
// Engine-grade: el motor SOLO maneja state machine + predicates + rewards-
// como-codigo-Lua. La semantica game-specific (XP por completar, popups
// de UI custom, etc) vive en los callbacks que el dev registra con
// quest.on_complete(...).

#include "engine/scripting/bindings/LuaBindings.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/quest/QuestAsset.h"
#include "engine/quest/QuestSystem.h"

#include <string>

namespace Mood {

namespace {

const char* stateToString(Quest::QuestSystem::State s) {
    using State = Quest::QuestSystem::State;
    switch (s) {
        case State::NotStarted: return "not_started";
        case State::Active:     return "active";
        case State::Complete:   return "complete";
        case State::Failed:     return "failed";
    }
    return "not_started";
}

/// @brief Resuelve path logico -> QuestAssetId via AssetManager. Si el
///        AssetManager es nullptr o el path no resuelve, devuelve 0
///        (missingQuestId) y deja que el caller decida si loggear.
QuestAssetId resolveQuest(AssetManager* assets, const std::string& path) {
    if (assets == nullptr) return 0;
    return assets->loadQuest(path);
}

} // namespace

void setupQuestBindings(sol::state& lua, AssetManager* assets) {
    namespace QS = Mood::Quest::QuestSystem;

    sol::table q = lua.create_named_table("quest");

    // -------- Lifecycle --------

    q.set_function("start",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            if (id == 0) {
                Log::script()->warn("[quest.start] path '{}' no resuelve", path);
                return false;
            }
            return QS::start(id, *assets);
        });

    q.set_function("complete",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            if (id == 0) return false;
            return QS::complete(id, *assets);
        });

    q.set_function("fail",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            if (id == 0) return false;
            return QS::fail(id);
        });

    q.set_function("objective_complete",
        [assets](const std::string& path,
                  const std::string& objectiveId) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            if (id == 0) return false;
            return QS::objectiveComplete(id, objectiveId, *assets);
        });

    // -------- Queries --------

    q.set_function("state",
        [assets](const std::string& path) -> std::string {
            if (assets == nullptr) return "not_started";
            const QuestAssetId id = resolveQuest(assets, path);
            if (id == 0) return "not_started";
            return stateToString(QS::stateOf(id));
        });

    q.set_function("is_active",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            return id != 0 && QS::isActive(id);
        });

    q.set_function("is_complete",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            return id != 0 && QS::isComplete(id);
        });

    q.set_function("is_failed",
        [assets](const std::string& path) -> bool {
            if (assets == nullptr) return false;
            const QuestAssetId id = resolveQuest(assets, path);
            return id != 0 && QS::isFailed(id);
        });

    // snapshot() — devuelve array de {path, state, objectives_done, objectives_total}
    q.set_function("snapshot",
        [assets, &lua]() -> sol::table {
            sol::state_view L(lua);
            sol::table arr = L.create_table();
            if (assets == nullptr) return arr;
            int luaIdx = 1;
            for (const auto& aq : QS::snapshot()) {
                sol::table row = L.create_table();
                row["path"]  = assets->questPathOf(aq.id);
                row["state"] = stateToString(aq.state);
                int done = 0;
                for (const auto& op : aq.objectives) if (op.completed) ++done;
                row["objectives_done"]  = done;
                row["objectives_total"] = static_cast<int>(aq.objectives.size());
                arr[luaIdx++] = row;
            }
            return arr;
        });

    // -------- Tracked --------

    q.set_function("set_tracked",
        [assets](sol::optional<std::string> pathOpt) {
            if (assets == nullptr) return;
            if (!pathOpt.has_value() || pathOpt->empty()) {
                QS::setTracked(assets->missingQuestId());
                return;
            }
            const QuestAssetId id = resolveQuest(assets, *pathOpt);
            QS::setTracked(id);  // si id==0 (no resuelve) desactiva igual
        });

    q.set_function("tracked",
        [assets]() -> std::string {
            if (assets == nullptr) return "";
            const QuestAssetId id = QS::tracked();
            if (id == 0) return "";
            return assets->questPathOf(id);
        });

    // -------- Game hooks --------
    //
    // Mismo patron que inventory.on_pickup/on_drop/on_use: el dev registra
    // UNA vez. El sol::function captura el Lua state — si el dev hace
    // hot-reload, la funcion vieja queda colgando hasta que se sobreescriba.
    // QuestSystem::clearHooks() en scene-unload se encarga.

    q.set_function("on_start",
        [assets](sol::function fn) {
            if (!fn.valid()) {
                QS::setStartHook(nullptr);
                return;
            }
            QS::setStartHook(
                [fn, assets](QuestAssetId id) {
                    const std::string path = assets != nullptr
                        ? assets->questPathOf(id) : std::string{};
                    sol::protected_function_result r = fn(path);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[quest.on_start] callback error: {}", err.what());
                    }
                });
        });

    q.set_function("on_complete",
        [assets](sol::function fn) {
            if (!fn.valid()) {
                QS::setCompleteHook(nullptr);
                return;
            }
            QS::setCompleteHook(
                [fn, assets](QuestAssetId id) {
                    const std::string path = assets != nullptr
                        ? assets->questPathOf(id) : std::string{};
                    sol::protected_function_result r = fn(path);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[quest.on_complete] callback error: {}", err.what());
                    }
                });
        });

    q.set_function("on_fail",
        [assets](sol::function fn) {
            if (!fn.valid()) {
                QS::setFailHook(nullptr);
                return;
            }
            QS::setFailHook(
                [fn, assets](QuestAssetId id) {
                    const std::string path = assets != nullptr
                        ? assets->questPathOf(id) : std::string{};
                    sol::protected_function_result r = fn(path);
                    if (!r.valid()) {
                        sol::error err = r;
                        Log::script()->warn(
                            "[quest.on_fail] callback error: {}", err.what());
                    }
                });
        });

    // -------- LuaEvaluator + LuaExecutor (integracion runtime) --------
    //
    // Cada tick() de QuestSystem necesita evaluar predicates ("inventory.count(...) >= 3")
    // y aplicar rewards ("inventory.add(...)" / "dialog.set_var(...)"). El motor
    // delega ese eval/exec a la sol::state donde estan registrados los bindings
    // de inventory/dialog. Sin esto, los objectives nunca se auto-completan
    // y las rewards no se aplican.
    //
    // CRITICO: el `sol::state&` capturado por referencia tiene que vivir
    // mientras los hooks esten activos. Los tests + el ScriptSystem llaman
    // `QS::clearHooks()` en teardown para desconectar antes de destruir la
    // state. Capturamos `&lua` (no por valor) porque `sol::state` no es
    // copy-constructible; ademas evita el lifetime mismatch con state_view.
    sol::state* statePtr = &lua;
    QS::setEvaluator(
        [statePtr](const std::string& expr) -> bool {
            // "return <expr>" para que loadstring devuelva el valor.
            sol::protected_function_result r =
                statePtr->safe_script("return (" + expr + ")",
                                       sol::script_pass_on_error);
            if (!r.valid()) {
                sol::error err = r;
                Log::script()->warn("[quest.evaluator] '{}' -> {}",
                                     expr, err.what());
                return false;
            }
            sol::object v = r;
            if (v.is<bool>()) return v.as<bool>();
            // Permitir que predicates devuelvan numero (truthy si != 0) o
            // string (truthy si no empty), siguiendo convencion Lua.
            return v.valid() && !v.is<sol::nil_t>();
        });

    QS::setExecutor(
        [statePtr](const std::string& code) {
            if (code.empty()) return;
            sol::protected_function_result r =
                statePtr->safe_script(code, sol::script_pass_on_error);
            if (!r.valid()) {
                sol::error err = r;
                Log::script()->warn("[quest.executor] '{}' -> {}",
                                     code, err.what());
            }
        });
}

} // namespace Mood
