// Tests del modelo de datos puro de NodeGraph (F2H46 Bloque G).
// Cubre: CRUD de nodos/sockets/links, reglas de canConnect, cascade
// delete al borrar nodo, busquedas, roundtrip de serializacion JSON,
// schema versioning.

#include <doctest/doctest.h>

#include "engine/nodegraph/Graph.h"

using namespace Mood;
using namespace Mood::NodeGraph;

namespace {

// Helper: crea un grafo con 2 nodos conectados por un link.
// Retorna {nodeA, nodeB, socketOutA, socketInB, linkId}.
struct TwoNodeFixture {
    Graph    g;
    NodeId   nodeA, nodeB;
    SocketId socketOutA, socketInB;
    LinkId   linkAB;
};

TwoNodeFixture makeTwoNodes(const std::string& tag = "flow") {
    TwoNodeFixture f;
    f.nodeA      = f.g.addNode("dialog_line", {10.0f, 20.0f});
    f.nodeB      = f.g.addNode("dialog_line", {200.0f, 20.0f});
    f.socketOutA = f.g.addSocket(f.nodeA, SocketKind::Output, tag, "next");
    f.socketInB  = f.g.addSocket(f.nodeB, SocketKind::Input,  tag, "prev");
    f.linkAB     = f.g.addLink(f.socketOutA, f.socketInB);
    return f;
}

} // namespace

// ============================================================
// CRUD basico
// ============================================================

TEST_CASE("NodeGraph: empty graph has zero nodes/links") {
    Graph g;
    CHECK(g.nodeCount() == 0);
    CHECK(g.linkCount() == 0);
}

TEST_CASE("NodeGraph: addNode returns monotonic IDs starting at 1") {
    Graph g;
    NodeId id1 = g.addNode("typeA", {0.0f, 0.0f});
    NodeId id2 = g.addNode("typeB", {1.0f, 1.0f});
    NodeId id3 = g.addNode("typeC", {2.0f, 2.0f});
    CHECK(id1 == 1u);
    CHECK(id2 == 2u);
    CHECK(id3 == 3u);
    CHECK(g.nodeCount() == 3);
}

TEST_CASE("NodeGraph: findNode returns node by ID, nullptr for missing") {
    Graph g;
    NodeId id = g.addNode("x", {5.0f, 5.0f});
    const Node* n = g.findNode(id);
    REQUIRE(n != nullptr);
    CHECK(n->id == id);
    CHECK(n->typeTag == "x");
    CHECK(n->position.x == 5.0f);
    CHECK(g.findNode(999u) == nullptr);
    CHECK(g.findNode(k_invalidNodeId) == nullptr);
}

TEST_CASE("NodeGraph: addSocket appends to correct list (input vs output)") {
    Graph g;
    NodeId n = g.addNode("t", {0.0f, 0.0f});
    SocketId in1  = g.addSocket(n, SocketKind::Input,  "flow", "prev");
    SocketId out1 = g.addSocket(n, SocketKind::Output, "flow", "next");
    SocketId out2 = g.addSocket(n, SocketKind::Output, "flow", "alt");

    const Node* node = g.findNode(n);
    REQUIRE(node != nullptr);
    CHECK(node->inputs.size() == 1);
    CHECK(node->outputs.size() == 2);
    CHECK(node->inputs[0].id == in1);
    CHECK(node->outputs[0].id == out1);
    CHECK(node->outputs[1].id == out2);
    CHECK(node->inputs[0].name == "prev");
    CHECK(node->outputs[1].name == "alt");
}

TEST_CASE("NodeGraph: addSocket on invalid node returns invalid ID") {
    Graph g;
    SocketId bad = g.addSocket(999u, SocketKind::Input, "flow", "x");
    CHECK(bad == k_invalidSocketId);
}

TEST_CASE("NodeGraph: findSocket searches across all nodes") {
    auto f = makeTwoNodes();
    CHECK(f.g.findSocket(f.socketOutA) != nullptr);
    CHECK(f.g.findSocket(f.socketInB)  != nullptr);
    CHECK(f.g.findSocket(999u)         == nullptr);
    CHECK(f.g.findSocket(k_invalidSocketId) == nullptr);
}

// ============================================================
// Reglas de canConnect
// ============================================================

TEST_CASE("NodeGraph: canConnect rejects same socket on both ends") {
    Graph g;
    NodeId n = g.addNode("t", {0.0f, 0.0f});
    SocketId s = g.addSocket(n, SocketKind::Output, "flow", "");
    CHECK_FALSE(g.canConnect(s, s));
}

TEST_CASE("NodeGraph: canConnect rejects mismatched typeTag") {
    Graph g;
    NodeId n1 = g.addNode("a", {0.0f, 0.0f});
    NodeId n2 = g.addNode("b", {0.0f, 0.0f});
    SocketId out = g.addSocket(n1, SocketKind::Output, "flow", "");
    SocketId in  = g.addSocket(n2, SocketKind::Input,  "data", "");
    CHECK_FALSE(g.canConnect(out, in));
}

TEST_CASE("NodeGraph: canConnect rejects output-to-output (kind mismatch)") {
    Graph g;
    NodeId n1 = g.addNode("a", {0.0f, 0.0f});
    NodeId n2 = g.addNode("b", {0.0f, 0.0f});
    SocketId out1 = g.addSocket(n1, SocketKind::Output, "flow", "");
    SocketId out2 = g.addSocket(n2, SocketKind::Output, "flow", "");
    CHECK_FALSE(g.canConnect(out1, out2));
}

TEST_CASE("NodeGraph: canConnect rejects input-as-from (must be Output)") {
    Graph g;
    NodeId n1 = g.addNode("a", {0.0f, 0.0f});
    NodeId n2 = g.addNode("b", {0.0f, 0.0f});
    SocketId in1 = g.addSocket(n1, SocketKind::Input,  "flow", "");
    SocketId in2 = g.addSocket(n2, SocketKind::Input,  "flow", "");
    CHECK_FALSE(g.canConnect(in1, in2));
}

TEST_CASE("NodeGraph: canConnect accepts valid output->input same type") {
    Graph g;
    NodeId n1 = g.addNode("a", {0.0f, 0.0f});
    NodeId n2 = g.addNode("b", {0.0f, 0.0f});
    SocketId out = g.addSocket(n1, SocketKind::Output, "flow", "");
    SocketId in  = g.addSocket(n2, SocketKind::Input,  "flow", "");
    CHECK(g.canConnect(out, in));
}

TEST_CASE("NodeGraph: addLink rejects duplicate (same from+to)") {
    auto f = makeTwoNodes();
    LinkId dup = f.g.addLink(f.socketOutA, f.socketInB);
    CHECK(dup == k_invalidLinkId);
    CHECK(f.g.linkCount() == 1);
}

TEST_CASE("NodeGraph: addLink rejects second link to same input socket") {
    Graph g;
    NodeId a = g.addNode("a", {0, 0});
    NodeId b = g.addNode("b", {0, 0});
    NodeId c = g.addNode("c", {0, 0});
    SocketId outA = g.addSocket(a, SocketKind::Output, "flow", "");
    SocketId outC = g.addSocket(c, SocketKind::Output, "flow", "");
    SocketId inB  = g.addSocket(b, SocketKind::Input,  "flow", "");

    LinkId first  = g.addLink(outA, inB);
    LinkId second = g.addLink(outC, inB);  // mismo input, debe fallar
    CHECK(first  != k_invalidLinkId);
    CHECK(second == k_invalidLinkId);
    CHECK(g.linkCount() == 1);
}

TEST_CASE("NodeGraph: addLink allows fan-out from single output") {
    Graph g;
    NodeId a = g.addNode("a", {0, 0});
    NodeId b = g.addNode("b", {0, 0});
    NodeId c = g.addNode("c", {0, 0});
    SocketId outA = g.addSocket(a, SocketKind::Output, "flow", "");
    SocketId inB  = g.addSocket(b, SocketKind::Input,  "flow", "");
    SocketId inC  = g.addSocket(c, SocketKind::Input,  "flow", "");

    LinkId l1 = g.addLink(outA, inB);
    LinkId l2 = g.addLink(outA, inC);  // mismo output, distintos inputs: OK
    CHECK(l1 != k_invalidLinkId);
    CHECK(l2 != k_invalidLinkId);
    CHECK(g.linkCount() == 2);
}

// ============================================================
// findLinkByInput
// ============================================================

TEST_CASE("NodeGraph: findLinkByInput returns link or nullopt") {
    auto f = makeTwoNodes();
    auto found = f.g.findLinkByInput(f.socketInB);
    REQUIRE(found.has_value());
    CHECK(*found == f.linkAB);

    auto missing = f.g.findLinkByInput(999u);
    CHECK_FALSE(missing.has_value());
}

// ============================================================
// Cascade delete al remover nodo
// ============================================================

TEST_CASE("NodeGraph: removeNode also removes incident links") {
    auto f = makeTwoNodes();
    CHECK(f.g.linkCount() == 1);
    CHECK(f.g.removeNode(f.nodeA));
    CHECK(f.g.linkCount() == 0);   // el link incidente desaparecio
    CHECK(f.g.nodeCount() == 1);   // nodeB sigue
}

TEST_CASE("NodeGraph: removeNode on invalid ID is no-op returning false") {
    Graph g;
    g.addNode("t", {0, 0});
    CHECK_FALSE(g.removeNode(999u));
    CHECK_FALSE(g.removeNode(k_invalidNodeId));
    CHECK(g.nodeCount() == 1);
}

TEST_CASE("NodeGraph: removeLink only removes that link, preserves nodes") {
    auto f = makeTwoNodes();
    CHECK(f.g.removeLink(f.linkAB));
    CHECK(f.g.linkCount() == 0);
    CHECK(f.g.nodeCount() == 2);  // nodos intactos
}

// ============================================================
// Clear
// ============================================================

TEST_CASE("NodeGraph: clear resets state and IDs") {
    Graph g;
    g.addNode("a", {0, 0});
    g.addNode("b", {0, 0});
    g.clear();
    CHECK(g.nodeCount() == 0);
    CHECK(g.linkCount() == 0);
    // El proximo addNode debe arrancar de nuevo en 1
    CHECK(g.addNode("c", {0, 0}) == 1u);
}

// ============================================================
// Serializacion roundtrip
// ============================================================

TEST_CASE("NodeGraph: toJson includes schema version") {
    Graph g;
    nlohmann::json j = g.toJson();
    REQUIRE(j.contains("_version"));
    CHECK(j["_version"].get<u32>() == Graph::k_schemaVersion);
}

TEST_CASE("NodeGraph: empty graph roundtrip preserves emptiness") {
    Graph g;
    nlohmann::json j = g.toJson();
    Graph g2 = Graph::fromJson(j);
    CHECK(g2.nodeCount() == 0);
    CHECK(g2.linkCount() == 0);
}

TEST_CASE("NodeGraph: 2-node graph roundtrip preserves structure") {
    auto f = makeTwoNodes();
    // Setear customData no-trivial
    Node* nA = f.g.findNode(f.nodeA);
    REQUIRE(nA != nullptr);
    nA->customData = nlohmann::json{
        {"text", "Hola mundo"},
        {"audio", "vo/intro.wav"},
        {"flags", nlohmann::json::array({"important", "translated"})}
    };
    nA->title = "Intro Line";

    nlohmann::json j = f.g.toJson();
    Graph restored = Graph::fromJson(j);

    CHECK(restored.nodeCount() == 2);
    CHECK(restored.linkCount() == 1);

    const Node* rA = restored.findNode(f.nodeA);
    REQUIRE(rA != nullptr);
    CHECK(rA->typeTag == "dialog_line");
    CHECK(rA->title == "Intro Line");
    CHECK(rA->position.x == 10.0f);
    CHECK(rA->position.y == 20.0f);
    CHECK(rA->outputs.size() == 1);
    CHECK(rA->customData["text"].get<std::string>() == "Hola mundo");
    CHECK(rA->customData["flags"].size() == 2);

    // Link debe preservar from/to socket IDs.
    const auto& links = restored.links();
    REQUIRE(links.size() == 1);
    CHECK(links[0].from == f.socketOutA);
    CHECK(links[0].to   == f.socketInB);
}

TEST_CASE("NodeGraph: fromJson rejects wrong schema version") {
    nlohmann::json j;
    j["_version"]       = 999u;  // version invalida
    j["next_node_id"]   = 5u;
    j["next_socket_id"] = 10u;
    j["next_link_id"]   = 3u;
    j["nodes"]          = nlohmann::json::array();
    j["links"]          = nlohmann::json::array();

    Graph g = Graph::fromJson(j);
    // Si la version no matchea, devuelve grafo vacio (NO copia los counters
    // ni los nodos del JSON invalido).
    CHECK(g.nodeCount() == 0);
    CHECK(g.linkCount() == 0);
}

TEST_CASE("NodeGraph: fromJson rejects non-object JSON") {
    nlohmann::json arr = nlohmann::json::array();
    Graph g = Graph::fromJson(arr);
    CHECK(g.nodeCount() == 0);
}

TEST_CASE("NodeGraph: roundtrip preserves next_*_id so new nodes get fresh IDs") {
    Graph g;
    g.addNode("a", {0, 0});  // id 1
    g.addNode("b", {0, 0});  // id 2
    g.removeNode(2u);         // id 2 borrado, pero el contador NO retrocede

    nlohmann::json j = g.toJson();
    Graph restored = Graph::fromJson(j);

    // El proximo addNode en restored debe usar ID 3, no reusar 2.
    NodeId newId = restored.addNode("c", {0, 0});
    CHECK(newId == 3u);
}
