#include "engine/quest/QuestScriptHost.h"

#include "core/Log.h"
#include "engine/game/state/GameState.h"
#include "engine/quest/QuestSystem.h"
#include "engine/scripting/bindings/LuaBindings.h"  // setupInventoryBindings

#include <sol/sol.hpp>

#include <memory>

namespace Mood::Quest::QuestScriptHost {

namespace {

std::unique_ptr<sol::state> g_state;
bool g_hooksRegistered = false;

// Scene + assets opcionales para resolver player + items en el binding
// `inventory`. Inyectados por `setSceneAndAssets(...)` desde el entry
// a Play Mode (editor) o al cargar mapa (player). nullptr = queries
// silenciosas.
Mood::Scene*        g_scene  = nullptr;
Mood::AssetManager* g_assets = nullptr;

void buildInventoryBindings(sol::state& lua) {
    Mood::setupInventoryBindings(lua, g_scene, g_assets);
}

// Construye los bindings de la sol::state del host. Mismo subset minimo
// que DialogScriptHost: `log`, `dialog` (lectura + vars), `inventory`
// (count + add + remove). NO `self`, NO `engine.exposed`, NO `physics`.
void buildBindings(sol::state& lua) {
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // --- log ---
    sol::table logTable = lua.create_named_table("log");
    logTable.set_function("info",  [](const std::string& s) { Mood::Log::script()->info(s); });
    logTable.set_function("warn",  [](const std::string& s) { Mood::Log::script()->warn(s); });
    logTable.set_function("error", [](const std::string& s) { Mood::Log::script()->error(s); });

    // --- dialog (set_var / get_var / has_var) ---
    // Predicates Talk/Reach del QuestSystem usan `dialog.has_var(...)`.
    // Rewards SetVar usan `dialog.set_var(...)`. Mismo storage que
    // DialogScriptHost: GameState::dialogVars (con persistencia v4+).
    sol::table dialogTable = lua.create_named_table("dialog");
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

    // --- inventory (count/has/add/remove con player-implicit) ---
    // Collect predicates usan `inventory.count('items/x') >= N`. Rewards
    // GiveItem usan `inventory.add('items/y', N)`. Sin g_scene/g_assets
    // las queries retornan 0/false (la entidad-quest player no se
    // resuelve, scriptea como si no tuviera nada).
    buildInventoryBindings(lua);
}

void ensureState() {
    if (g_state == nullptr) {
        g_state = std::make_unique<sol::state>();
        buildBindings(*g_state);
        Mood::Log::script()->info("[QuestScriptHost] sol::state inicializada");
    }
}

} // namespace

void init() {
    ensureState();
    if (g_hooksRegistered) return;
    // Wire hooks del QuestSystem para que tick() use nuestro evaluator
    // y rewards usen nuestro executor.
    Mood::Quest::QuestSystem::setEvaluator(
        [](const std::string& expr) { return evaluate(expr); });
    Mood::Quest::QuestSystem::setExecutor(
        [](const std::string& code) { execute(code); });
    g_hooksRegistered = true;
    Mood::Log::script()->info(
        "[QuestScriptHost] hooks evaluator/executor registrados en QuestSystem");
}

void setSceneAndAssets(Mood::Scene* scene, Mood::AssetManager* assets) {
    g_scene  = scene;
    g_assets = assets;
    if (g_state != nullptr) {
        buildInventoryBindings(*g_state);
        Mood::Log::script()->info(
            "[QuestScriptHost] tabla `inventory` re-bindeada (scene={}, assets={})",
            static_cast<void*>(scene), static_cast<void*>(assets));
    }
}

bool evaluate(const std::string& expr) {
    if (expr.empty()) return true;
    ensureState();
    const std::string script = "return (" + expr + ")";
    sol::protected_function_result r =
        g_state->safe_script(script, sol::script_pass_on_error);
    if (!r.valid()) {
        sol::error err = r;
        Mood::Log::script()->warn(
            "[QuestScriptHost] evaluate('{}') error: {}",
            expr, err.what());
        return false; // fail-safe: objective no se completa
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
            "[QuestScriptHost] execute('{}') error: {}",
            code, err.what());
    }
}

void reset() {
    if (g_state == nullptr) return;
    g_state.reset();
    g_hooksRegistered = false;
    g_scene  = nullptr;
    g_assets = nullptr;
    Mood::Log::script()->info("[QuestScriptHost] sol::state tirada (reset)");
}

bool isInitialized() {
    return g_state != nullptr;
}

} // namespace Mood::Quest::QuestScriptHost
