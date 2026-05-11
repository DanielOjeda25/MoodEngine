// Tests del DialogSystem (F2H48): state machine que recorre un
// DialogAsset segun las choices elegidas. Cubre:
//   - start/stop + isActive
//   - advance con choices (recorrido del graph via output sockets)
//   - continueNext en nodos sin choices
//   - availableChoices filtrado por condition_lua evaluator
//   - hooks on_select_lua / on_node_enter / on_choice ejecutados
//   - GameState::dialogActive() flag toggleado por start/stop
//   - reset() limpia state pero NO hooks
//   - asset null / start_node invalido -> no-op silencioso

#include <doctest/doctest.h>

#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"

using namespace Mood;
using namespace Mood::Dialog;
using namespace Mood::NodeGraph;

namespace {

/// Construye un asset minimo de 2 nodos enlazados por un solo output
/// "continue" — caso mas simple del state machine.
Asset makeTwoLineAsset() {
    Asset a;
    const NodeId n1 = a.graph().addNode(k_typeDialogLine, {0, 0});
    const NodeId n2 = a.graph().addNode(k_typeDialogLine, {200, 0});

    // n1: 1 input + 1 output "continue"
    a.graph().addSocket(n1, SocketKind::Input,  k_socketFlow, "in");
    const SocketId n1Out = a.graph().addSocket(n1, SocketKind::Output, k_socketFlow, "continue");
    // n2: 1 input + 1 output (sink)
    const SocketId n2In = a.graph().addSocket(n2, SocketKind::Input,  k_socketFlow, "in");
    a.graph().addSocket(n2, SocketKind::Output, k_socketFlow, "continue");

    a.graph().addLink(n1Out, n2In);

    // customData minima (parseLine necesita el shape).
    Line l1; l1.text_literal = "Linea 1";
    Line l2; l2.text_literal = "Linea 2";
    a.writeLine(n1, l1);
    a.writeLine(n2, l2);

    a.metadata().start_node_id = n1;
    a.metadata().name = "test_dialog";
    return a;
}

/// Asset con un nodo que tiene 2 choices con destinos distintos.
/// n_root --choice0--> n_a
///        \-choice1--> n_b
Asset makeBranchAsset(NodeId* outRoot, NodeId* outA, NodeId* outB) {
    Asset a;
    const NodeId root = a.graph().addNode(k_typeDialogLine, {0, 0});
    const NodeId nA   = a.graph().addNode(k_typeDialogLine, {200, -100});
    const NodeId nB   = a.graph().addNode(k_typeDialogLine, {200, 100});

    // Root: 1 input + 2 outputs (escritos via writeLine con 2 choices).
    a.graph().addSocket(root, SocketKind::Input, k_socketFlow, "in");
    a.graph().addSocket(nA,   SocketKind::Input, k_socketFlow, "in");
    a.graph().addSocket(nB,   SocketKind::Input, k_socketFlow, "in");

    Line lr;
    lr.text_literal = "Elegi";
    Choice c0; c0.label_literal = "Opcion A"; c0.condition_lua = ""; c0.on_select_lua = "";
    Choice c1; c1.label_literal = "Opcion B"; c1.condition_lua = ""; c1.on_select_lua = "";
    lr.choices = {c0, c1};
    a.writeLine(root, lr);

    // Linkear los 2 outputs del root a nA y nB.
    const Node* rootNode = a.graph().findNode(root);
    REQUIRE(rootNode != nullptr);
    REQUIRE(rootNode->outputs.size() == 2);
    const SocketId out0 = rootNode->outputs[0].id;
    const SocketId out1 = rootNode->outputs[1].id;
    const Node* aNode = a.graph().findNode(nA);
    const Node* bNode = a.graph().findNode(nB);
    REQUIRE(aNode != nullptr);
    REQUIRE(bNode != nullptr);
    a.graph().addLink(out0, aNode->inputs[0].id);
    a.graph().addLink(out1, bNode->inputs[0].id);

    Line la; la.text_literal = "Llegaste a A"; a.writeLine(nA, la);
    Line lb; lb.text_literal = "Llegaste a B"; a.writeLine(nB, lb);

    a.metadata().start_node_id = root;
    a.metadata().name = "branch_dialog";
    if (outRoot) *outRoot = root;
    if (outA)    *outA    = nA;
    if (outB)    *outB    = nB;
    return a;
}

} // namespace

TEST_CASE("DialogSystem: start nulo o sin start_node es no-op") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    CHECK_FALSE(DialogSystem::start(nullptr));
    CHECK_FALSE(DialogSystem::isActive());

    Asset empty;  // sin start_node_id
    CHECK_FALSE(DialogSystem::start(&empty));
    CHECK_FALSE(DialogSystem::isActive());
}

TEST_CASE("DialogSystem: start activa el flag global + setea currentNode") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    Asset a = makeTwoLineAsset();

    CHECK_FALSE(GameState::dialogActive());
    REQUIRE(DialogSystem::start(&a));
    CHECK(DialogSystem::isActive());
    CHECK(GameState::dialogActive());
    CHECK(DialogSystem::currentNodeId() == a.metadata().start_node_id);
    CHECK(DialogSystem::currentLine().text_literal == "Linea 1");
}

TEST_CASE("DialogSystem: continueNext avanza por unico output socket") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    Asset a = makeTwoLineAsset();
    REQUIRE(DialogSystem::start(&a));

    REQUIRE(DialogSystem::continueNext());
    CHECK(DialogSystem::currentLine().text_literal == "Linea 2");
    // Segunda llamada: n2 no tiene link saliente -> stop.
    REQUIRE(DialogSystem::continueNext());
    CHECK_FALSE(DialogSystem::isActive());
    CHECK_FALSE(GameState::dialogActive());
}

TEST_CASE("DialogSystem: advance con choices recorre el branch correcto") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);
    REQUIRE(DialogSystem::start(&a));
    REQUIRE(DialogSystem::currentNodeId() == root);

    // Elegir choice 0 (Opcion A) -> deberia ir a nA.
    REQUIRE(DialogSystem::advance(0));
    CHECK(DialogSystem::currentNodeId() == nA);
    CHECK(DialogSystem::currentLine().text_literal == "Llegaste a A");
}

TEST_CASE("DialogSystem: advance branch B") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);
    REQUIRE(DialogSystem::start(&a));

    REQUIRE(DialogSystem::advance(1));
    CHECK(DialogSystem::currentNodeId() == nB);
}

TEST_CASE("DialogSystem: advance fuera de rango o inactivo es no-op") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);

    CHECK_FALSE(DialogSystem::advance(0));  // inactivo
    REQUIRE(DialogSystem::start(&a));
    CHECK_FALSE(DialogSystem::advance(-1));   // out of range
    CHECK_FALSE(DialogSystem::advance(99));   // out of range
    CHECK(DialogSystem::currentNodeId() == root); // no se movio
}

TEST_CASE("DialogSystem: availableChoices filtra por condition_lua via evaluator") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);

    // Editar el root para agregar condition_lua a la choice B.
    Line lr = a.parseLine(root);
    lr.choices[1].condition_lua = "player.has_key";
    a.writeLine(root, lr);

    // Evaluator dice false a "player.has_key".
    DialogSystem::setEvaluator([](const std::string& expr) {
        return expr == "player.has_key" ? false : true;
    });

    REQUIRE(DialogSystem::start(&a));
    auto choices = DialogSystem::availableChoices();
    REQUIRE(choices.size() == 1);
    CHECK(choices[0].choiceIndex == 0);
    CHECK(choices[0].label == "Opcion A");
}

TEST_CASE("DialogSystem: on_select_lua se ejecuta via executor antes del salto") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);

    Line lr = a.parseLine(root);
    lr.choices[0].on_select_lua = "player.score = player.score + 10";
    a.writeLine(root, lr);

    std::vector<std::string> executed;
    DialogSystem::setExecutor([&executed](const std::string& code) {
        executed.push_back(code);
    });

    REQUIRE(DialogSystem::start(&a));
    REQUIRE(DialogSystem::advance(0));
    REQUIRE(executed.size() == 1);
    CHECK(executed[0] == "player.score = player.score + 10");
    CHECK(DialogSystem::currentNodeId() == nA);
}

TEST_CASE("DialogSystem: on_node_enter hook invoked en start + transitions") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);

    std::vector<NodeId> visited;
    DialogSystem::setNodeEnterHook([&visited](NodeId id) {
        visited.push_back(id);
    });

    REQUIRE(DialogSystem::start(&a));
    REQUIRE(DialogSystem::advance(0));
    REQUIRE(visited.size() == 2);
    CHECK(visited[0] == root);
    CHECK(visited[1] == nA);
}

TEST_CASE("DialogSystem: on_choice hook invoked con (currentNode, choiceIndex)") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    NodeId root, nA, nB;
    Asset a = makeBranchAsset(&root, &nA, &nB);

    struct Captured { NodeId node; int idx; };
    std::vector<Captured> captured;
    DialogSystem::setChoiceHook([&captured](NodeId id, int idx) {
        captured.push_back({id, idx});
    });

    REQUIRE(DialogSystem::start(&a));
    REQUIRE(DialogSystem::advance(1));
    REQUIRE(captured.size() == 1);
    CHECK(captured[0].node == root);
    CHECK(captured[0].idx == 1);
}

TEST_CASE("DialogSystem: stop limpia state + flag global") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    Asset a = makeTwoLineAsset();
    REQUIRE(DialogSystem::start(&a));
    CHECK(GameState::dialogActive());

    DialogSystem::stop();
    CHECK_FALSE(DialogSystem::isActive());
    CHECK_FALSE(GameState::dialogActive());
    CHECK(DialogSystem::currentNodeId() == k_invalidNodeId);
    CHECK(DialogSystem::currentAsset() == nullptr);
}

TEST_CASE("DialogSystem: reset limpia state pero NO hooks") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    int calls = 0;
    DialogSystem::setNodeEnterHook([&calls](NodeId) { ++calls; });

    Asset a = makeTwoLineAsset();
    REQUIRE(DialogSystem::start(&a));
    REQUIRE(calls == 1);

    DialogSystem::reset();
    CHECK_FALSE(DialogSystem::isActive());

    // Volver a arrancar: hook deberia seguir activo (reset NO los limpia).
    REQUIRE(DialogSystem::start(&a));
    CHECK(calls == 2);
}

TEST_CASE("DialogSystem: clearHooks limpia los 4 callbacks") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    int calls = 0;
    DialogSystem::setNodeEnterHook([&calls](NodeId) { ++calls; });
    DialogSystem::clearHooks();

    Asset a = makeTwoLineAsset();
    REQUIRE(DialogSystem::start(&a));
    CHECK(calls == 0);
}

TEST_CASE("DialogSystem: GameState::dialogVars sobrevive entre dialogs") {
    DialogSystem::reset();
    DialogSystem::clearHooks();
    GameState::dialogVars().clear();
    GameState::dialogVars()["talked_to_guard"] = "true";

    Asset a = makeTwoLineAsset();
    REQUIRE(DialogSystem::start(&a));
    DialogSystem::stop();

    // El var sigue ahi (decision F2H48 #2).
    CHECK(GameState::dialogVars()["talked_to_guard"] == "true");
}

TEST_CASE("DialogSystem: GameState::reset limpia dialogVars + flag") {
    GameState::dialogVars()["foo"] = "bar";
    GameState::dialogActive() = true;
    GameState::reset();
    CHECK(GameState::dialogVars().empty());
    CHECK_FALSE(GameState::dialogActive());
}
