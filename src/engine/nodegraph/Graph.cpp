#include "engine/nodegraph/Graph.h"

#include "core/Log.h"

#include <algorithm>

namespace Mood::NodeGraph {

// =============================================================
// CRUD de nodos
// =============================================================

NodeId Graph::addNode(std::string typeTag, glm::vec2 position) {
    Node n;
    n.id       = m_nextNodeId++;
    n.typeTag  = std::move(typeTag);
    n.position = position;
    m_nodes.push_back(std::move(n));
    return m_nodes.back().id;
}

bool Graph::removeNode(NodeId id) {
    if (id == k_invalidNodeId) return false;
    auto nit = std::find_if(m_nodes.begin(), m_nodes.end(),
                            [id](const Node& n) { return n.id == id; });
    if (nit == m_nodes.end()) return false;

    // Borrar links incidentes (donde from o to pertenece a este nodo).
    // Recolectar los socket ids del nodo primero.
    std::vector<SocketId> ownedSockets;
    ownedSockets.reserve(nit->inputs.size() + nit->outputs.size());
    for (const Socket& s : nit->inputs)  ownedSockets.push_back(s.id);
    for (const Socket& s : nit->outputs) ownedSockets.push_back(s.id);

    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(),
            [&ownedSockets](const Link& l) {
                return std::find(ownedSockets.begin(), ownedSockets.end(), l.from) != ownedSockets.end()
                    || std::find(ownedSockets.begin(), ownedSockets.end(), l.to)   != ownedSockets.end();
            }),
        m_links.end());

    m_nodes.erase(nit);
    return true;
}

const Node* Graph::findNode(NodeId id) const {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                           [id](const Node& n) { return n.id == id; });
    return (it != m_nodes.end()) ? &(*it) : nullptr;
}

Node* Graph::findNode(NodeId id) {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                           [id](const Node& n) { return n.id == id; });
    return (it != m_nodes.end()) ? &(*it) : nullptr;
}

// =============================================================
// CRUD de sockets
// =============================================================

SocketId Graph::addSocket(NodeId node,
                          SocketKind kind,
                          std::string typeTag,
                          std::string name) {
    Node* n = findNode(node);
    if (!n) return k_invalidSocketId;

    Socket s;
    s.id        = m_nextSocketId++;
    s.ownerNode = node;
    s.kind      = kind;
    s.typeTag   = std::move(typeTag);
    s.name      = std::move(name);

    if (kind == SocketKind::Input)  n->inputs.push_back(std::move(s));
    else                            n->outputs.push_back(std::move(s));

    return (kind == SocketKind::Input) ? n->inputs.back().id : n->outputs.back().id;
}

const Socket* Graph::findSocket(SocketId id) const {
    if (id == k_invalidSocketId) return nullptr;
    for (const Node& n : m_nodes) {
        for (const Socket& s : n.inputs)  if (s.id == id) return &s;
        for (const Socket& s : n.outputs) if (s.id == id) return &s;
    }
    return nullptr;
}

bool Graph::removeSocket(SocketId id) {
    if (id == k_invalidSocketId) return false;
    // Borrar links incidentes (donde from o to == este socket).
    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(),
            [id](const Link& l) { return l.from == id || l.to == id; }),
        m_links.end());
    // Buscar y borrar el socket del nodo dueno.
    for (Node& n : m_nodes) {
        auto itIn = std::find_if(n.inputs.begin(), n.inputs.end(),
                                   [id](const Socket& s) { return s.id == id; });
        if (itIn != n.inputs.end()) {
            n.inputs.erase(itIn);
            return true;
        }
        auto itOut = std::find_if(n.outputs.begin(), n.outputs.end(),
                                    [id](const Socket& s) { return s.id == id; });
        if (itOut != n.outputs.end()) {
            n.outputs.erase(itOut);
            return true;
        }
    }
    return false;
}

// =============================================================
// CRUD de links
// =============================================================

bool Graph::canConnect(SocketId from, SocketId to) const {
    if (from == k_invalidSocketId || to == k_invalidSocketId) return false;
    if (from == to) return false;
    const Socket* sFrom = findSocket(from);
    const Socket* sTo   = findSocket(to);
    if (!sFrom || !sTo) return false;
    // Regla 2: kinds correctos.
    if (sFrom->kind != SocketKind::Output) return false;
    if (sTo->kind   != SocketKind::Input)  return false;
    // Regla 3: typeTag identico.
    if (sFrom->typeTag != sTo->typeTag) return false;
    // Regla 4: no duplicar link existente.
    for (const Link& l : m_links) {
        if (l.from == from && l.to == to) return false;
    }
    // Regla 5: input solo acepta UN link.
    for (const Link& l : m_links) {
        if (l.to == to) return false;
    }
    return true;
}

LinkId Graph::addLink(SocketId from, SocketId to) {
    if (!canConnect(from, to)) return k_invalidLinkId;
    Link l;
    l.id   = m_nextLinkId++;
    l.from = from;
    l.to   = to;
    m_links.push_back(std::move(l));
    return m_links.back().id;
}

bool Graph::removeLink(LinkId id) {
    if (id == k_invalidLinkId) return false;
    auto it = std::find_if(m_links.begin(), m_links.end(),
                           [id](const Link& l) { return l.id == id; });
    if (it == m_links.end()) return false;
    m_links.erase(it);
    return true;
}

std::optional<LinkId> Graph::findLinkByInput(SocketId inputSocket) const {
    for (const Link& l : m_links) {
        if (l.to == inputSocket) return l.id;
    }
    return std::nullopt;
}

// =============================================================
// Reset
// =============================================================

void Graph::clear() {
    m_nodes.clear();
    m_links.clear();
    m_nextNodeId   = 1;
    m_nextSocketId = 1;
    m_nextLinkId   = 1;
}

// =============================================================
// Serializacion JSON
// =============================================================

namespace {

nlohmann::json socketToJson(const Socket& s) {
    return nlohmann::json{
        {"id",         s.id},
        {"owner_node", s.ownerNode},
        {"kind",       static_cast<u8>(s.kind)},
        {"type_tag",   s.typeTag},
        {"name",       s.name},
    };
}

Socket socketFromJson(const nlohmann::json& j) {
    Socket s;
    s.id        = j.value("id", k_invalidSocketId);
    s.ownerNode = j.value("owner_node", k_invalidNodeId);
    s.kind      = static_cast<SocketKind>(j.value("kind", static_cast<u8>(SocketKind::Input)));
    s.typeTag   = j.value("type_tag", std::string{});
    s.name      = j.value("name", std::string{});
    return s;
}

nlohmann::json nodeToJson(const Node& n) {
    nlohmann::json inputs  = nlohmann::json::array();
    nlohmann::json outputs = nlohmann::json::array();
    for (const Socket& s : n.inputs)  inputs.push_back(socketToJson(s));
    for (const Socket& s : n.outputs) outputs.push_back(socketToJson(s));
    return nlohmann::json{
        {"id",          n.id},
        {"type_tag",    n.typeTag},
        {"title",       n.title},
        {"position",    {n.position.x, n.position.y}},
        {"inputs",      inputs},
        {"outputs",     outputs},
        {"custom_data", n.customData},
    };
}

Node nodeFromJson(const nlohmann::json& j) {
    Node n;
    n.id      = j.value("id", k_invalidNodeId);
    n.typeTag = j.value("type_tag", std::string{});
    n.title   = j.value("title", std::string{});
    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 2) {
        n.position.x = j["position"][0].get<f32>();
        n.position.y = j["position"][1].get<f32>();
    }
    if (j.contains("inputs") && j["inputs"].is_array()) {
        for (const auto& s : j["inputs"])  n.inputs.push_back(socketFromJson(s));
    }
    if (j.contains("outputs") && j["outputs"].is_array()) {
        for (const auto& s : j["outputs"]) n.outputs.push_back(socketFromJson(s));
    }
    if (j.contains("custom_data")) n.customData = j["custom_data"];
    else                            n.customData = nlohmann::json::object();
    return n;
}

nlohmann::json linkToJson(const Link& l) {
    return nlohmann::json{
        {"id",   l.id},
        {"from", l.from},
        {"to",   l.to},
    };
}

Link linkFromJson(const nlohmann::json& j) {
    Link l;
    l.id   = j.value("id", k_invalidLinkId);
    l.from = j.value("from", k_invalidSocketId);
    l.to   = j.value("to", k_invalidSocketId);
    return l;
}

} // namespace

nlohmann::json Graph::toJson() const {
    nlohmann::json nodes = nlohmann::json::array();
    nlohmann::json links = nlohmann::json::array();
    for (const Node& n : m_nodes) nodes.push_back(nodeToJson(n));
    for (const Link& l : m_links) links.push_back(linkToJson(l));
    return nlohmann::json{
        {"_version",       k_schemaVersion},
        {"next_node_id",   m_nextNodeId},
        {"next_socket_id", m_nextSocketId},
        {"next_link_id",   m_nextLinkId},
        {"nodes",          nodes},
        {"links",          links},
    };
}

Graph Graph::fromJson(const nlohmann::json& j) {
    Graph g;
    if (!j.is_object()) {
        Log::engine()->error("[NodeGraph] fromJson: JSON no es objeto");
        return g;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[NodeGraph] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return g;
    }
    g.m_nextNodeId   = j.value("next_node_id",   1u);
    g.m_nextSocketId = j.value("next_socket_id", 1u);
    g.m_nextLinkId   = j.value("next_link_id",   1u);
    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& jn : j["nodes"]) g.m_nodes.push_back(nodeFromJson(jn));
    }
    if (j.contains("links") && j["links"].is_array()) {
        for (const auto& jl : j["links"]) g.m_links.push_back(linkFromJson(jl));
    }
    return g;
}

} // namespace Mood::NodeGraph
