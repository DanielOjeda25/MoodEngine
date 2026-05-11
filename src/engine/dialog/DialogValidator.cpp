#include "engine/dialog/DialogValidator.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Mood::Dialog {

using namespace Mood::NodeGraph;

namespace rules {

std::vector<ValidationIssue> checkStartNode(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    const NodeId start = asset.metadata().start_node_id;
    if (start == k_invalidNodeId) {
        issues.push_back({ValidationIssue::Severity::Error, 0,
                          "start_node_id no esta seteado (vacio dialog?)"});
        return issues;
    }
    if (!asset.graph().findNode(start)) {
        issues.push_back({ValidationIssue::Severity::Error, start,
                          "start_node_id apunta a un nodo que no existe"});
    }
    return issues;
}

std::vector<ValidationIssue> checkInputSockets(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    for (const Node& n : asset.graph().nodes()) {
        if (n.typeTag != k_typeDialogLine) continue;
        // Contar inputs con typeTag flow.
        int flowInputs = 0;
        for (const Socket& s : n.inputs) {
            if (s.typeTag == k_socketFlow) ++flowInputs;
        }
        if (flowInputs != 1) {
            issues.push_back({ValidationIssue::Severity::Error, n.id,
                              "dialog_line debe tener exactamente 1 input socket flow"});
        }
    }
    return issues;
}

std::vector<ValidationIssue> checkOutputSockets(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    for (const Node& n : asset.graph().nodes()) {
        if (n.typeTag != k_typeDialogLine) continue;
        int flowOutputs = 0;
        for (const Socket& s : n.outputs) {
            if (s.typeTag == k_socketFlow) ++flowOutputs;
        }
        if (flowOutputs < 1) {
            issues.push_back({ValidationIssue::Severity::Error, n.id,
                              "dialog_line necesita al menos 1 output socket flow"});
        }
    }
    return issues;
}

std::vector<ValidationIssue> checkChoicesSync(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    for (const Node& n : asset.graph().nodes()) {
        if (n.typeTag != k_typeDialogLine) continue;
        const Line line = asset.parseLine(n.id);
        const size_t expected = line.choices.empty() ? 1u : line.choices.size();
        if (n.outputs.size() != expected) {
            issues.push_back({ValidationIssue::Severity::Error, n.id,
                              "output sockets count desincronizado con choices array"});
        }
    }
    return issues;
}

std::vector<ValidationIssue> checkCycles(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    // DFS con tres estados: WHITE (no visitado), GRAY (en stack actual),
    // BLACK (terminado). Encontrar un GRAY durante DFS = back edge =
    // ciclo. Reporto el primer nodo donde lo detecto como warning.
    enum class Color : u8 { White = 0, Gray = 1, Black = 2 };
    std::unordered_map<NodeId, Color> color;
    for (const Node& n : asset.graph().nodes()) color[n.id] = Color::White;

    // Mapa socket -> owner node para resolver edges rapido.
    std::unordered_map<SocketId, NodeId> socketOwner;
    for (const Node& n : asset.graph().nodes()) {
        for (const Socket& s : n.inputs)  socketOwner[s.id] = n.id;
        for (const Socket& s : n.outputs) socketOwner[s.id] = n.id;
    }

    // Adjacency: para cada nodo, los nodos a los que apunta via outputs.
    std::unordered_map<NodeId, std::vector<NodeId>> adj;
    for (const Link& l : asset.graph().links()) {
        const auto itFrom = socketOwner.find(l.from);
        const auto itTo   = socketOwner.find(l.to);
        if (itFrom != socketOwner.end() && itTo != socketOwner.end()) {
            adj[itFrom->second].push_back(itTo->second);
        }
    }

    bool cycleFound = false;
    NodeId cycleAtNode = k_invalidNodeId;

    // DFS iterativo (recursion explicita via stack para grafos grandes).
    auto dfs = [&](NodeId root) {
        std::vector<std::pair<NodeId, size_t>> stack;  // (node, next_child_idx)
        stack.push_back({root, 0});
        color[root] = Color::Gray;
        while (!stack.empty()) {
            auto& [cur, idx] = stack.back();
            const auto& children = adj[cur];
            if (idx >= children.size()) {
                color[cur] = Color::Black;
                stack.pop_back();
                continue;
            }
            NodeId next = children[idx];
            ++idx;
            if (color[next] == Color::Gray) {
                cycleFound = true;
                cycleAtNode = next;
                return;
            }
            if (color[next] == Color::White) {
                color[next] = Color::Gray;
                stack.push_back({next, 0});
            }
        }
    };

    for (const Node& n : asset.graph().nodes()) {
        if (color[n.id] == Color::White) {
            dfs(n.id);
            if (cycleFound) break;
        }
    }

    if (cycleFound) {
        issues.push_back({ValidationIssue::Severity::Warning, cycleAtNode,
                          "ciclo detectado en el grafo (puede ser intencional si hay flags Lua que lo rompen)"});
    }
    return issues;
}

std::vector<ValidationIssue> checkOrphans(const Asset& asset) {
    std::vector<ValidationIssue> issues;
    const NodeId start = asset.metadata().start_node_id;
    // Recolectar todos los input sockets que reciben algun link.
    std::unordered_set<SocketId> reachedInputs;
    for (const Link& l : asset.graph().links()) reachedInputs.insert(l.to);
    // Por cada dialog_line (excepto start), verificar que algun input
    // socket suyo este en reachedInputs.
    for (const Node& n : asset.graph().nodes()) {
        if (n.typeTag != k_typeDialogLine) continue;
        if (n.id == start) continue;
        bool reached = false;
        for (const Socket& s : n.inputs) {
            if (reachedInputs.count(s.id) > 0) { reached = true; break; }
        }
        if (!reached) {
            issues.push_back({ValidationIssue::Severity::Warning, n.id,
                              "nodo huerfano (sin link entrante; puede ser nodo en construccion)"});
        }
    }
    return issues;
}

} // namespace rules

// =============================================================
// validate (corre todas las reglas)
// =============================================================

std::vector<ValidationIssue> validate(const Asset& asset) {
    std::vector<ValidationIssue> all;
    auto append = [&](std::vector<ValidationIssue> xs) {
        for (auto& x : xs) all.push_back(std::move(x));
    };
    append(rules::checkStartNode(asset));
    append(rules::checkInputSockets(asset));
    append(rules::checkOutputSockets(asset));
    append(rules::checkChoicesSync(asset));
    append(rules::checkCycles(asset));
    append(rules::checkOrphans(asset));
    return all;
}

} // namespace Mood::Dialog
