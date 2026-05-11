#include "engine/dialog/DialogSystem.h"

#include "core/Log.h"
#include "engine/game/state/GameState.h"

namespace Mood::Dialog::DialogSystem {

using namespace Mood::NodeGraph;

// =============================================================
// Estado interno (file-scope statics — singleton namespace)
// =============================================================

namespace {

const Asset*       g_asset       = nullptr;
NodeId             g_currentNode = k_invalidNodeId;

LuaEvaluator       g_evaluator;
LuaExecutor        g_executor;
NodeEnterHook      g_onNodeEnter;
ChoiceHook         g_onChoice;

void setActiveFlag(bool on) {
    Mood::GameState::dialogActive() = on;
}

void enterNode(NodeId id) {
    g_currentNode = id;
    if (g_onNodeEnter) {
        g_onNodeEnter(id);
    }
}

/// @brief Resuelve el output socket [index] del nodo actual, busca su
///        link saliente y retorna el NodeId destino. `k_invalidNodeId`
///        si no hay link o el indice fuera de rango.
NodeId resolveTargetByOutputIndex(int index) {
    if (g_asset == nullptr) return k_invalidNodeId;
    const Node* n = g_asset->graph().findNode(g_currentNode);
    if (n == nullptr) return k_invalidNodeId;
    if (index < 0 || static_cast<size_t>(index) >= n->outputs.size()) {
        return k_invalidNodeId;
    }
    const SocketId fromSocket = n->outputs[index].id;
    // Buscar link cuyo `from` == fromSocket. Inputs son unicos (regla 5
    // del Graph) pero outputs pueden tener N — necesitamos lookup directo.
    for (const Link& link : g_asset->graph().links()) {
        if (link.from == fromSocket) {
            const Socket* toSock = g_asset->graph().findSocket(link.to);
            if (toSock != nullptr) {
                return toSock->ownerNode;
            }
        }
    }
    return k_invalidNodeId;
}

} // namespace

// =============================================================
// Lifecycle
// =============================================================

bool start(const Asset* asset) {
    if (asset == nullptr) {
        Log::engine()->warn("[DialogSystem] start: asset null, no-op.");
        return false;
    }
    const NodeId startId = asset->metadata().start_node_id;
    if (startId == k_invalidNodeId) {
        Log::engine()->warn(
            "[DialogSystem] start: asset '{}' sin start_node_id.",
            asset->metadata().name);
        return false;
    }
    if (asset->graph().findNode(startId) == nullptr) {
        Log::engine()->warn(
            "[DialogSystem] start: start_node_id {} no existe en graph de '{}'.",
            startId, asset->metadata().name);
        return false;
    }
    g_asset = asset;
    setActiveFlag(true);
    enterNode(startId);
    return true;
}

bool advance(int choiceIndex) {
    if (!isActive()) return false;
    if (choiceIndex < 0) return false;

    // 1) Disparar on_select_lua de la choice antes de transicionar
    //    (el dev espera que el hook lea/escriba state ANTES del salto).
    const Line line = currentLine();
    if (static_cast<size_t>(choiceIndex) >= line.choices.size()) {
        return false;
    }
    const Choice& c = line.choices[choiceIndex];
    if (g_onChoice) {
        g_onChoice(g_currentNode, choiceIndex);
    }
    if (g_executor && !c.on_select_lua.empty()) {
        g_executor(c.on_select_lua);
    }

    // 2) Resolver target via output socket [choiceIndex].
    const NodeId next = resolveTargetByOutputIndex(choiceIndex);
    if (next == k_invalidNodeId) {
        // No hay link saliente: fin del dialog.
        stop();
        return true;
    }
    enterNode(next);
    return true;
}

bool continueNext() {
    if (!isActive()) return false;
    // Si el nodo tiene choices, requiere advance(idx) explicito.
    const Line line = currentLine();
    if (!line.choices.empty()) return false;

    // No tiene choices => exactamente 1 output socket "continue" por
    // invariante del DialogAsset. Resolver y saltar.
    const NodeId next = resolveTargetByOutputIndex(0);
    if (g_onChoice) {
        g_onChoice(g_currentNode, 0);
    }
    if (next == k_invalidNodeId) {
        stop();
        return true;
    }
    enterNode(next);
    return true;
}

void stop() {
    g_asset       = nullptr;
    g_currentNode = k_invalidNodeId;
    setActiveFlag(false);
}

void reset() {
    stop();
    // hooks NO se limpian — el caller los inyecta una vez al arranque
    // del editor/player y permanecen activos a lo largo de la sesion.
}

// =============================================================
// Queries
// =============================================================

bool isActive() {
    return g_currentNode != k_invalidNodeId;
}

NodeId currentNodeId() {
    return g_currentNode;
}

const Asset* currentAsset() {
    return g_asset;
}

Line currentLine() {
    if (!isActive() || g_asset == nullptr) return Line{};
    return g_asset->parseLine(g_currentNode);
}

std::vector<AvailableChoice> availableChoices() {
    std::vector<AvailableChoice> result;
    if (!isActive()) return result;
    const Line line = currentLine();
    result.reserve(line.choices.size());
    for (size_t i = 0; i < line.choices.size(); ++i) {
        const Choice& c = line.choices[i];
        // Si hay evaluator y la expresion no esta vacia, llamarlo.
        // Si no hay evaluator o la expresion es vacia, asumir true.
        bool visible = true;
        if (!c.condition_lua.empty() && g_evaluator) {
            visible = g_evaluator(c.condition_lua);
        }
        if (!visible) continue;

        AvailableChoice ac;
        ac.choiceIndex = static_cast<int>(i);
        // Label: prefer label_literal si esta; sino label_key (el caller
        // del HUD aplica I18n::T si corresponde — DialogSystem no conoce
        // i18n para mantener el modulo libre de deps adicionales).
        ac.label = !c.label_literal.empty() ? c.label_literal : c.label_key;
        result.push_back(std::move(ac));
    }
    return result;
}

// =============================================================
// Hooks
// =============================================================

void setEvaluator(LuaEvaluator fn)     { g_evaluator   = std::move(fn); }
void setExecutor(LuaExecutor fn)       { g_executor    = std::move(fn); }
void setNodeEnterHook(NodeEnterHook fn){ g_onNodeEnter = std::move(fn); }
void setChoiceHook(ChoiceHook fn)      { g_onChoice    = std::move(fn); }

void clearHooks() {
    g_evaluator   = {};
    g_executor    = {};
    g_onNodeEnter = {};
    g_onChoice    = {};
}

} // namespace Mood::Dialog::DialogSystem
