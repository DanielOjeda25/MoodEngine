#include "engine/dialog/DialogScriptHost.h"

#include "core/Log.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"

#include <sol/sol.hpp>

#include <memory>

namespace Mood::Dialog::DialogScriptHost {

namespace {

std::unique_ptr<sol::state> g_state;
bool g_hooksRegistered = false;

/// Construye los bindings de la sol::state del host. Subset minimo
/// para que un dev pueda escribir `condition_lua` / `on_select_lua`
/// utiles sin abrir surface al resto del motor.
void buildBindings(sol::state& lua) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // --- log.info / log.warn / log.error ---
    sol::table logTable = lua.create_named_table("log");
    logTable.set_function("info",  [](const std::string& s) { Mood::Log::script()->info(s); });
    logTable.set_function("warn",  [](const std::string& s) { Mood::Log::script()->warn(s); });
    logTable.set_function("error", [](const std::string& s) { Mood::Log::script()->error(s); });

    // --- dialog (lectura + vars) ---
    // No exponemos start/advance/continueNext/stop aca porque seria
    // recursivo: el dev podria llamar dialog.advance() desde un
    // on_select_lua que ya esta en medio de un advance. Mantener el
    // host como "lectura + state shared", control de flujo viene del
    // DialogSystem o de scripts de entity.
    sol::table dialogTable = lua.create_named_table("dialog");
    dialogTable.set_function("isActive",
        []() { return Mood::Dialog::DialogSystem::isActive(); });
    dialogTable.set_function("currentNode",
        []() { return Mood::Dialog::DialogSystem::currentNodeId(); });
    dialogTable.set_function("set_var",
        [](const std::string& k, const std::string& v) {
            Mood::GameState::dialogVars()[k] = v;
        });
    dialogTable.set_function("get_var",
        [](const std::string& k) -> std::string {
            auto& vars = Mood::GameState::dialogVars();
            auto it = vars.find(k);
            return it != vars.end() ? it->second : std::string{};
        });
    dialogTable.set_function("has_var",
        [](const std::string& k) {
            return Mood::GameState::dialogVars().count(k) > 0;
        });
    dialogTable.set_function("clear_vars",
        []() { Mood::GameState::dialogVars().clear(); });

    // --- hud (interact_prompt + lecturas comunes) ---
    sol::table hudTable = lua.create_named_table("hud");
    hudTable.set_function("setInteractPrompt",
        [](const std::string& s) { Mood::GameState::hud().interact_prompt = s; });
    hudTable.set_function("getInteractPrompt",
        []() { return Mood::GameState::hud().interact_prompt; });
    hudTable.set_function("getHp",   []() { return Mood::GameState::hud().hp; });
    hudTable.set_function("getMag",  []() { return Mood::GameState::hud().mag; });
    hudTable.set_function("getReserve", []() { return Mood::GameState::hud().reserve; });
}

void ensureState() {
    if (g_state == nullptr) {
        g_state = std::make_unique<sol::state>();
        buildBindings(*g_state);
        Mood::Log::script()->info("[DialogScriptHost] sol::state inicializada");
    }
}

} // namespace

void init() {
    ensureState();
    if (g_hooksRegistered) return;
    // Cablear los hooks del DialogSystem para que pasen por nosotros.
    Mood::Dialog::DialogSystem::setEvaluator(
        [](const std::string& expr) { return evaluate(expr); });
    Mood::Dialog::DialogSystem::setExecutor(
        [](const std::string& code) { execute(code); });
    g_hooksRegistered = true;
    Mood::Log::script()->info(
        "[DialogScriptHost] hooks evaluator/executor registrados en DialogSystem");
}

bool evaluate(const std::string& expr) {
    if (expr.empty()) return true;
    ensureState();
    // Sintaxis Lua: "return <expr>" para que la sentencia evalue a
    // un valor capturable. Wrap defensivo en caso de que el dev
    // ponga ya el `return` (raro pero posible).
    std::string script = "return (" + expr + ")";
    sol::protected_function_result r =
        g_state->safe_script(script, sol::script_pass_on_error);
    if (!r.valid()) {
        sol::error err = r;
        Mood::Log::script()->warn(
            "[DialogScriptHost] evaluate('{}') error: {} — choice oculta",
            expr, err.what());
        return false;  // fail-safe: ante error, ocultar la choice
    }
    sol::object obj = r;
    if (obj.is<bool>()) return obj.as<bool>();
    // Truthiness Lua: nil/false son falsos; el resto es verdadero.
    return !obj.is<sol::nil_t>();
}

void execute(const std::string& code) {
    if (code.empty()) return;
    ensureState();
    sol::protected_function_result r =
        g_state->safe_script(code, sol::script_pass_on_error);
    if (!r.valid()) {
        sol::error err = r;
        Mood::Log::script()->warn(
            "[DialogScriptHost] execute('{}') error: {}",
            code, err.what());
    }
}

void reset() {
    if (g_state == nullptr) return;
    g_state.reset();
    g_hooksRegistered = false;
    Mood::Log::script()->info("[DialogScriptHost] sol::state tirada (reset)");
}

bool isInitialized() {
    return g_state != nullptr;
}

} // namespace Mood::Dialog::DialogScriptHost
