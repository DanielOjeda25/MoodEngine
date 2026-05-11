#include "editor/commands/NodeGraphCommand.h"

#include "core/Log.h"

#include <algorithm>

namespace Mood {

using namespace Mood::NodeGraph;

// ============================================================
// AddNodeCommand
// ============================================================

AddNodeCommand::AddNodeCommand(Graph* graph,
                                std::string typeTag,
                                glm::vec2 position,
                                std::vector<SocketSpec> inputs,
                                std::vector<SocketSpec> outputs,
                                std::string title,
                                std::string label)
    : m_graph(graph),
      m_typeTag(std::move(typeTag)),
      m_position(position),
      m_inputs(std::move(inputs)),
      m_outputs(std::move(outputs)),
      m_title(std::move(title)),
      m_label(std::move(label)) {}

void AddNodeCommand::execute() {
    if (!m_graph) return;
    m_nodeId = m_graph->addNode(m_typeTag, m_position);
    if (Node* n = m_graph->findNode(m_nodeId)) {
        n->title = m_title;
    }
    for (const auto& s : m_inputs) {
        m_graph->addSocket(m_nodeId, SocketKind::Input, s.typeTag, s.name);
    }
    for (const auto& s : m_outputs) {
        m_graph->addSocket(m_nodeId, SocketKind::Output, s.typeTag, s.name);
    }
}

void AddNodeCommand::undo() {
    if (!m_graph || m_nodeId == k_invalidNodeId) return;
    m_graph->removeNode(m_nodeId);
    // Reset m_nodeId para que un eventual redo asigne uno nuevo.
    m_nodeId = k_invalidNodeId;
}

// ============================================================
// RemoveNodeCommand
// ============================================================

RemoveNodeCommand::RemoveNodeCommand(Graph* graph,
                                       NodeId nodeId,
                                       std::string label)
    : m_graph(graph), m_nodeId(nodeId), m_label(std::move(label)) {
    // Capturar snapshot ANTES de execute(). Asi undo() tiene los datos
    // aunque execute() ya borro el nodo del grafo.
    if (m_graph) {
        if (const Node* n = m_graph->findNode(m_nodeId)) {
            m_savedNode = *n;
            // Capturar links incidentes (entran o salen del nodo).
            std::vector<SocketId> owned;
            owned.reserve(n->inputs.size() + n->outputs.size());
            for (const Socket& s : n->inputs)  owned.push_back(s.id);
            for (const Socket& s : n->outputs) owned.push_back(s.id);
            for (const Link& l : m_graph->links()) {
                const bool fromOwned =
                    std::find(owned.begin(), owned.end(), l.from) != owned.end();
                const bool toOwned =
                    std::find(owned.begin(), owned.end(), l.to)   != owned.end();
                if (fromOwned || toOwned) m_savedIncidentLinks.push_back(l);
            }
            m_captured = true;
        } else {
            Log::editor()->warn("[NodeGraphCmd] RemoveNodeCommand: nodeId {} no existe", m_nodeId);
        }
    }
}

void RemoveNodeCommand::execute() {
    if (!m_graph || !m_captured) return;
    m_graph->removeNode(m_nodeId);
}

void RemoveNodeCommand::undo() {
    if (!m_graph || !m_captured) return;
    // Reconstruir nodo con su pos, title, customData, sockets.
    NodeId newNodeId = m_graph->addNode(m_savedNode.typeTag, m_savedNode.position);
    if (Node* n = m_graph->findNode(newNodeId)) {
        n->title      = m_savedNode.title;
        n->customData = m_savedNode.customData;
    }
    // Re-crear sockets. Los IDs nuevos NO matchearan a los originales —
    // hay que remappear los links. Construyo el mapping socket_old → socket_new.
    std::vector<std::pair<SocketId, SocketId>> sockMap;
    sockMap.reserve(m_savedNode.inputs.size() + m_savedNode.outputs.size());
    for (const Socket& s : m_savedNode.inputs) {
        SocketId ns = m_graph->addSocket(newNodeId, SocketKind::Input, s.typeTag, s.name);
        sockMap.emplace_back(s.id, ns);
    }
    for (const Socket& s : m_savedNode.outputs) {
        SocketId ns = m_graph->addSocket(newNodeId, SocketKind::Output, s.typeTag, s.name);
        sockMap.emplace_back(s.id, ns);
    }
    auto remap = [&](SocketId oldId) -> SocketId {
        for (const auto& p : sockMap) {
            if (p.first == oldId) return p.second;
        }
        return oldId;  // socket no era del nodo borrado — sigue valido.
    };
    // Re-crear los links incidentes con los socket IDs remapeados.
    for (const Link& l : m_savedIncidentLinks) {
        m_graph->addLink(remap(l.from), remap(l.to));
    }
    // Actualizar m_nodeId para que execute() re-corra (redo) opere
    // sobre el nodo nuevo. NOTA: este re-execute volvera a invocar
    // removeNode con el ID nuevo; el snapshot original NO se vuelve a
    // capturar. Para soportar redo->undo asimetrico (caso raro, pero
    // posible si el user encadena operaciones), capturamos el nuevo
    // snapshot ahora.
    m_nodeId = newNodeId;
    if (const Node* recreated = m_graph->findNode(newNodeId)) {
        m_savedNode = *recreated;
    }
    m_savedIncidentLinks.clear();
    for (const Link& l : m_graph->links()) {
        // Determinar si l es incidente al nodo recreado.
        bool isIncident = false;
        const Node* nn = m_graph->findNode(newNodeId);
        if (nn) {
            for (const Socket& s : nn->inputs)  { if (s.id == l.from || s.id == l.to) { isIncident = true; break; } }
            if (!isIncident) {
                for (const Socket& s : nn->outputs) { if (s.id == l.from || s.id == l.to) { isIncident = true; break; } }
            }
        }
        if (isIncident) m_savedIncidentLinks.push_back(l);
    }
}

// ============================================================
// MoveNodeCommand
// ============================================================

MoveNodeCommand::MoveNodeCommand(Graph* graph,
                                  NodeId nodeId,
                                  glm::vec2 oldPos,
                                  glm::vec2 newPos,
                                  std::string label)
    : m_graph(graph),
      m_nodeId(nodeId),
      m_oldPos(oldPos),
      m_newPos(newPos),
      m_label(std::move(label)) {}

void MoveNodeCommand::execute() {
    if (!m_graph) return;
    if (Node* n = m_graph->findNode(m_nodeId)) {
        n->position = m_newPos;
    }
}

void MoveNodeCommand::undo() {
    if (!m_graph) return;
    if (Node* n = m_graph->findNode(m_nodeId)) {
        n->position = m_oldPos;
    }
}

// ============================================================
// AddLinkCommand
// ============================================================

AddLinkCommand::AddLinkCommand(Graph* graph,
                                SocketId fromSocket,
                                SocketId toSocket,
                                std::string label)
    : m_graph(graph),
      m_fromSocket(fromSocket),
      m_toSocket(toSocket),
      m_label(std::move(label)) {}

void AddLinkCommand::execute() {
    if (!m_graph) return;
    m_linkId = m_graph->addLink(m_fromSocket, m_toSocket);
    if (m_linkId == k_invalidLinkId) {
        Log::editor()->warn("[NodeGraphCmd] AddLinkCommand: canConnect rechazo {}-{}",
                              m_fromSocket, m_toSocket);
    }
}

void AddLinkCommand::undo() {
    if (!m_graph || m_linkId == k_invalidLinkId) return;
    m_graph->removeLink(m_linkId);
    m_linkId = k_invalidLinkId;
}

// ============================================================
// RemoveLinkCommand
// ============================================================

RemoveLinkCommand::RemoveLinkCommand(Graph* graph,
                                       LinkId linkId,
                                       std::string label)
    : m_graph(graph), m_linkId(linkId), m_label(std::move(label)) {
    if (m_graph) {
        for (const Link& l : m_graph->links()) {
            if (l.id == m_linkId) {
                m_savedLink = l;
                m_captured  = true;
                break;
            }
        }
    }
}

void RemoveLinkCommand::execute() {
    if (!m_graph || !m_captured) return;
    m_graph->removeLink(m_linkId);
}

void RemoveLinkCommand::undo() {
    if (!m_graph || !m_captured) return;
    // Re-add con los mismos from/to. El ID interno cambia (graph
    // genera ID nuevo) — guardamos el nuevo para que un eventual redo
    // siga funcionando.
    LinkId newId = m_graph->addLink(m_savedLink.from, m_savedLink.to);
    m_linkId = newId;
}

} // namespace Mood
