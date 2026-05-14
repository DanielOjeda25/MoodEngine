#include "engine/quest/QuestSystem.h"

#include "core/Log.h"
#include "engine/quest/QuestAsset.h"

#include <algorithm>

namespace Mood::Quest::QuestSystem {

namespace {

// ---- Estado global del sistema (singleton namespace) ----
//
// Mismo patron que DialogSystem (F2H48). El "singleton de archivos" via
// variables namespace anon evita el boilerplate de getInstance() y se
// alinea con como ya estan armados Dialog/Inventory hooks. El precio es
// que no son testeables en paralelo — los tests llaman `reset()` en setup.

std::vector<ActiveQuest> g_active;
QuestAssetId             g_tracked = 0;

LuaEvaluator g_evaluator;
LuaExecutor  g_executor;

StartHook    g_onStart;
CompleteHook g_onComplete;
FailHook     g_onFail;

// Busca el ActiveQuest por id. Devuelve nullptr si no esta.
ActiveQuest* find(QuestAssetId id) {
    for (auto& q : g_active) {
        if (q.id == id) return &q;
    }
    return nullptr;
}

// Aplica las rewards de un quest. NO chequea state (caller asume Complete).
void applyRewards(const Asset& asset) {
    if (!g_executor) return;
    for (const Reward& r : asset.rewards) {
        const std::string code = generateRewardCode(r);
        if (!code.empty()) {
            g_executor(code);
        }
    }
}

// Helper string para escape: replace ' with \' (suficiente para item paths
// + var values del schema, que no admiten quotes). Si en el futuro emerge
// necesidad de strings con quotes embedded, migrar a JSON-style encoding.
std::string escapeForLuaSingleQuoted(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\'') out += "\\'";
        else out += c;
    }
    return out;
}

} // namespace

// =============================================================
// Helpers de generacion de codigo Lua
// =============================================================

std::string generatePredicate(const Objective& o) {
    switch (o.type) {
        case ObjectiveType::Collect:
            if (o.item_path.empty()) return "";
            return "inventory.count('" + escapeForLuaSingleQuoted(o.item_path) +
                   "') >= " + std::to_string(o.min_quantity);
        case ObjectiveType::Talk:
        case ObjectiveType::Reach:
            if (o.var_name.empty()) return "";
            return "dialog.has_var('" + escapeForLuaSingleQuoted(o.var_name) + "')";
        case ObjectiveType::CustomLua:
            return o.custom_predicate;
    }
    return "";
}

std::string generateRewardCode(const Reward& r) {
    switch (r.type) {
        case RewardType::Item:
            if (r.item_path.empty()) return "";
            return "inventory.add('" + escapeForLuaSingleQuoted(r.item_path) +
                   "', " + std::to_string(r.quantity) + ")";
        case RewardType::Var:
            if (r.var_name.empty()) return "";
            return "dialog.set_var('" + escapeForLuaSingleQuoted(r.var_name) +
                   "', '" + escapeForLuaSingleQuoted(r.var_value) + "')";
        case RewardType::Lua:
            return r.lua_script;
    }
    return "";
}

// =============================================================
// Lifecycle de quests
// =============================================================

bool start(QuestAssetId id, AssetManager& am) {
    if (id == am.missingQuestId()) {
        Log::engine()->warn("[QuestSystem] start({}): id invalido (slot 0)", id);
        return false;
    }

    if (ActiveQuest* existing = find(id)) {
        // Ya esta registrado. Solo Active y Complete bloquean restart;
        // si esta NotStarted o Failed, permite "re-empezar" reseteando los
        // objectives. Caso Failed reiniciado: util si el dev quiere
        // "retry quest" desde Lua.
        if (existing->state == State::Active || existing->state == State::Complete) {
            return false;
        }
    }

    const Asset* asset = am.getQuest(id);
    if (asset == nullptr || asset->objectives.empty()) {
        Log::engine()->warn(
            "[QuestSystem] start({}): asset invalido o sin objectives", id);
        return false;
    }

    ActiveQuest fresh;
    fresh.id    = id;
    fresh.state = State::Active;
    fresh.objectives.resize(asset->objectives.size());
    // ObjectiveProgress::completed default = false; nada que setear.

    if (ActiveQuest* existing = find(id)) {
        *existing = std::move(fresh);
    } else {
        g_active.push_back(std::move(fresh));
    }

    if (g_onStart) g_onStart(id);
    Log::engine()->info("[QuestSystem] iniciado quest id={} ({} objectives)",
                         id, asset->objectives.size());
    return true;
}

bool complete(QuestAssetId id, AssetManager& am) {
    ActiveQuest* q = find(id);
    if (q == nullptr || q->state != State::Active) {
        return false;
    }
    const Asset* asset = am.getQuest(id);
    if (asset == nullptr) {
        Log::engine()->warn("[QuestSystem] complete({}): asset no resuelve", id);
        return false;
    }

    q->state = State::Complete;
    // Marca todos los objectives como done (consistencia visual en UI).
    for (auto& op : q->objectives) op.completed = true;

    applyRewards(*asset);
    if (g_onComplete) g_onComplete(id);
    Log::engine()->info("[QuestSystem] completado quest id={}", id);
    return true;
}

bool fail(QuestAssetId id) {
    ActiveQuest* q = find(id);
    if (q == nullptr || q->state != State::Active) {
        return false;
    }
    q->state = State::Failed;
    // Objectives NO se marcan done — failed = "abandonado". El UI
    // probablemente los muestra grises / tachados.
    if (g_onFail) g_onFail(id);
    Log::engine()->info("[QuestSystem] fallado quest id={}", id);
    return true;
}

bool objectiveComplete(QuestAssetId id, const std::string& objectiveId,
                        AssetManager& am) {
    ActiveQuest* q = find(id);
    if (q == nullptr || q->state != State::Active) {
        return false;
    }
    const Asset* asset = am.getQuest(id);
    if (asset == nullptr) return false;

    // Buscar el objective por id (string).
    int idx = -1;
    for (size_t i = 0; i < asset->objectives.size(); ++i) {
        if (asset->objectives[i].id == objectiveId) {
            idx = static_cast<int>(i);
            break;
        }
    }
    if (idx < 0) {
        Log::engine()->warn(
            "[QuestSystem] objectiveComplete({}, '{}'): objective no encontrado",
            id, objectiveId);
        return false;
    }
    if (idx >= static_cast<int>(q->objectives.size())) return false;
    q->objectives[idx].completed = true;

    // Auto-complete si todos done.
    bool allDone = true;
    for (const auto& op : q->objectives) {
        if (!op.completed) { allDone = false; break; }
    }
    if (allDone) {
        complete(id, am);
    }
    return true;
}

// =============================================================
// Tick — auto-evaluacion via Lua
// =============================================================

void tick(AssetManager& am) {
    // Snapshot del array para evitar invalidacion de iteradores si
    // complete() recursivamente toca g_active (no deberia, pero defensivo).
    // Iteramos por indice porque el size podria cambiar si futuro hook
    // dispara un start(). El estado base no es threadsafe.
    for (size_t i = 0; i < g_active.size(); ++i) {
        // ActiveQuest puede cambiar de slot si se llama start() durante el
        // tick — por seguridad releemos por indice cada iteracion.
        if (i >= g_active.size()) break;
        auto& q = g_active[i];
        if (q.state != State::Active) continue;

        const Asset* asset = am.getQuest(q.id);
        if (asset == nullptr) continue;

        bool allDone = true;
        for (size_t oi = 0; oi < q.objectives.size() && oi < asset->objectives.size(); ++oi) {
            if (q.objectives[oi].completed) continue;
            const std::string pred = generatePredicate(asset->objectives[oi]);
            if (pred.empty()) {
                // Predicate vacio = nunca satisfacible automaticamente.
                // El dev tiene que usar objectiveComplete() manual.
                allDone = false;
                continue;
            }
            if (g_evaluator && g_evaluator(pred)) {
                q.objectives[oi].completed = true;
            }
            if (!q.objectives[oi].completed) allDone = false;
        }

        if (allDone) {
            complete(q.id, am);
        }
    }
}

// =============================================================
// Queries
// =============================================================

State stateOf(QuestAssetId id) {
    if (const ActiveQuest* q = find(id)) return q->state;
    return State::NotStarted;
}

bool isActive  (QuestAssetId id) { return stateOf(id) == State::Active; }
bool isComplete(QuestAssetId id) { return stateOf(id) == State::Complete; }
bool isFailed  (QuestAssetId id) { return stateOf(id) == State::Failed; }

const std::vector<ActiveQuest>& snapshot() { return g_active; }

// =============================================================
// Tracked
// =============================================================

void setTracked(QuestAssetId id) { g_tracked = id; }
QuestAssetId tracked() { return g_tracked; }

// =============================================================
// Hooks
// =============================================================

void setEvaluator   (LuaEvaluator fn) { g_evaluator   = std::move(fn); }
void setExecutor    (LuaExecutor  fn) { g_executor    = std::move(fn); }
void setStartHook   (StartHook    fn) { g_onStart     = std::move(fn); }
void setCompleteHook(CompleteHook fn) { g_onComplete  = std::move(fn); }
void setFailHook    (FailHook     fn) { g_onFail      = std::move(fn); }

void clearHooks() {
    g_evaluator  = {};
    g_executor   = {};
    g_onStart    = {};
    g_onComplete = {};
    g_onFail     = {};
}

void reset() {
    g_active.clear();
    g_tracked = 0;
}

} // namespace Mood::Quest::QuestSystem
