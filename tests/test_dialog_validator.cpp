// Tests del DialogValidator (F2H47 Bloque G). Cubre cada regla individual
// + el validate() unificado. Patron: setup minimo del asset con un bug
// concreto + assertion del issue retornado.

#include <doctest/doctest.h>

#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogValidator.h"

using namespace Mood;
using namespace Mood::NodeGraph;
using namespace Mood::Dialog;

// ============================================================
// checkStartNode
// ============================================================

TEST_CASE("Validator: empty asset reports invalid start_node") {
    Asset a;
    auto issues = rules::checkStartNode(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Error);
    CHECK(issues[0].nodeId == k_invalidNodeId);
}

TEST_CASE("Validator: asset with start_node set + node existing passes") {
    Asset a;
    a.addLine({0, 0});
    auto issues = rules::checkStartNode(a);
    CHECK(issues.empty());
}

TEST_CASE("Validator: dangling start_node_id reports error") {
    Asset a;
    a.addLine({0, 0});
    a.metadata().start_node_id = 999u;  // ID huerfano
    auto issues = rules::checkStartNode(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Error);
    CHECK(issues[0].nodeId == 999u);
}

// ============================================================
// checkInputSockets / checkOutputSockets
// ============================================================

TEST_CASE("Validator: dialog_line con sockets default pasa input/output checks") {
    Asset a;
    a.addLine({0, 0});
    CHECK(rules::checkInputSockets(a).empty());
    CHECK(rules::checkOutputSockets(a).empty());
}

TEST_CASE("Validator: dialog_line sin output socket reporta error") {
    Asset a;
    NodeId id = a.addLine({0, 0});
    // Borrar el output socket manualmente para forzar el bug.
    Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    REQUIRE(n->outputs.size() == 1);
    a.graph().removeSocket(n->outputs[0].id);
    auto issues = rules::checkOutputSockets(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Error);
    CHECK(issues[0].nodeId == id);
}

TEST_CASE("Validator: dialog_line sin input socket reporta error") {
    Asset a;
    NodeId id = a.addLine({0, 0});
    Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    a.graph().removeSocket(n->inputs[0].id);
    auto issues = rules::checkInputSockets(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Error);
    CHECK(issues[0].nodeId == id);
}

// ============================================================
// checkChoicesSync (invariante de auto-sync)
// ============================================================

TEST_CASE("Validator: choices count == output sockets count pasa") {
    Asset a;
    NodeId id = a.addLine({0, 0});
    Line line;
    line.choices = {Choice{"a","","",""}, Choice{"b","","",""}};
    a.writeLine(id, line);
    CHECK(rules::checkChoicesSync(a).empty());
}

TEST_CASE("Validator: detecta desync si manualmente se agrega socket extra") {
    Asset a;
    NodeId id = a.addLine({0, 0});
    Line line;
    line.choices = {Choice{"a","","",""}};
    a.writeLine(id, line);
    // Ahora outputs.size()==1. Agregamos socket extra manualmente
    // sin actualizar choices => desync.
    a.graph().addSocket(id, SocketKind::Output, k_socketFlow, "extra");
    auto issues = rules::checkChoicesSync(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].nodeId == id);
}

// ============================================================
// checkCycles
// ============================================================

TEST_CASE("Validator: grafo lineal sin ciclo pasa") {
    Asset a;
    NodeId n1 = a.addLine({0, 0});
    NodeId n2 = a.addLine({100, 0});
    NodeId n3 = a.addLine({200, 0});
    // Link n1 -> n2 -> n3.
    const Node* N1 = a.graph().findNode(n1);
    const Node* N2 = a.graph().findNode(n2);
    const Node* N3 = a.graph().findNode(n3);
    a.graph().addLink(N1->outputs[0].id, N2->inputs[0].id);
    a.graph().addLink(N2->outputs[0].id, N3->inputs[0].id);
    CHECK(rules::checkCycles(a).empty());
}

TEST_CASE("Validator: detecta ciclo n1 -> n2 -> n1 como WARNING") {
    Asset a;
    NodeId n1 = a.addLine({0, 0});
    NodeId n2 = a.addLine({100, 0});
    // n1 necesita un segundo input socket porque n2 va a apuntar
    // a el (regla 5 de canConnect: 1 input acepta 1 link).
    // Pero default solo 1 input — agreguemos un input mas o usemos el
    // mismo: el primer addLink lo hace, el segundo seria de n2 a n1
    // PERO no se permite 2 entrantes al mismo input. Simplifiquemos:
    // hagamos n1 con 2 inputs.
    // Actually, dialog v1 dice 1 input por dialog_line. Pero para
    // testear el ciclo, simulemos con un input extra hack.
    a.graph().addSocket(n1, SocketKind::Input, k_socketFlow, "back");
    const Node* N1 = a.graph().findNode(n1);
    const Node* N2 = a.graph().findNode(n2);
    a.graph().addLink(N1->outputs[0].id, N2->inputs[0].id);
    a.graph().addLink(N2->outputs[0].id, N1->inputs[1].id);  // back edge

    auto issues = rules::checkCycles(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Warning);
}

TEST_CASE("Validator: self-loop n1 -> n1 detectado como ciclo") {
    Asset a;
    NodeId n1 = a.addLine({0, 0});
    a.graph().addSocket(n1, SocketKind::Input, k_socketFlow, "back");
    const Node* N1 = a.graph().findNode(n1);
    a.graph().addLink(N1->outputs[0].id, N1->inputs[1].id);
    auto issues = rules::checkCycles(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Warning);
}

// ============================================================
// checkOrphans
// ============================================================

TEST_CASE("Validator: solo el start_node huerfano NO reporta warning") {
    Asset a;
    a.addLine({0, 0});
    CHECK(rules::checkOrphans(a).empty());
}

TEST_CASE("Validator: nodo no-start sin link entrante reporta warning") {
    Asset a;
    a.addLine({0, 0});            // start
    NodeId orphan = a.addLine({100, 0});  // sin links
    auto issues = rules::checkOrphans(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].severity == ValidationIssue::Severity::Warning);
    CHECK(issues[0].nodeId == orphan);
}

TEST_CASE("Validator: nodo no-start con link entrante NO reporta warning") {
    Asset a;
    NodeId start = a.addLine({0, 0});
    NodeId next = a.addLine({100, 0});
    const Node* S = a.graph().findNode(start);
    const Node* N = a.graph().findNode(next);
    a.graph().addLink(S->outputs[0].id, N->inputs[0].id);
    CHECK(rules::checkOrphans(a).empty());
}

// ============================================================
// validate() unificado
// ============================================================

TEST_CASE("Validator: validate() en asset valido retorna empty") {
    Asset a;
    NodeId n1 = a.addLine({0, 0});
    NodeId n2 = a.addLine({100, 0});
    const Node* N1 = a.graph().findNode(n1);
    const Node* N2 = a.graph().findNode(n2);
    a.graph().addLink(N1->outputs[0].id, N2->inputs[0].id);
    auto issues = validate(a);
    CHECK(issues.empty());
}

TEST_CASE("Validator: validate() acumula issues de multiples reglas") {
    Asset a;
    // Asset vacio: error de start_node ausente.
    auto issues = validate(a);
    CHECK_FALSE(issues.empty());
    // Pero las otras reglas pasan (no hay nodos que validar).
    int errors = 0;
    for (const auto& i : issues) {
        if (i.severity == ValidationIssue::Severity::Error) ++errors;
    }
    CHECK(errors >= 1);
}
