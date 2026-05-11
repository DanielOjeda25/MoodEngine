// Tests del DialogScriptHost (F2H48.1): sol::state global del juego
// para evaluar `condition_lua` y `on_select_lua` de los Choices.
// Cubre:
//   - init() registra los hooks en DialogSystem
//   - evaluate("") => true (paridad con contrato)
//   - evaluate de expr boolean simple
//   - evaluate con error de parse => false (fail-safe)
//   - evaluate accede a dialog.get_var
//   - execute muta dialog.set_var
//   - integracion end-to-end: choice con condition_lua condicionada
//     por una var seteada antes del start

#include <doctest/doctest.h>

#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogScriptHost.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"

using namespace Mood;
using namespace Mood::Dialog;

namespace {

/// Helper: estado limpio antes de cada test (singletons compartidos).
void freshState() {
    DialogScriptHost::reset();
    DialogSystem::reset();
    DialogSystem::clearHooks();
    GameState::dialogVars().clear();
}

} // namespace

TEST_CASE("DialogScriptHost: init es idempotente + isInitialized true") {
    freshState();
    DialogScriptHost::init();
    CHECK(DialogScriptHost::isInitialized());
    DialogScriptHost::init();  // segunda llamada no rompe
    CHECK(DialogScriptHost::isInitialized());
}

TEST_CASE("DialogScriptHost: evaluate de expr vacia retorna true") {
    freshState();
    DialogScriptHost::init();
    CHECK(DialogScriptHost::evaluate(""));
}

TEST_CASE("DialogScriptHost: evaluate de expr boolean simple") {
    freshState();
    DialogScriptHost::init();
    CHECK(DialogScriptHost::evaluate("true"));
    CHECK_FALSE(DialogScriptHost::evaluate("false"));
    CHECK(DialogScriptHost::evaluate("1 + 1 == 2"));
    CHECK_FALSE(DialogScriptHost::evaluate("1 + 1 == 3"));
}

TEST_CASE("DialogScriptHost: evaluate con error de parse -> false (fail-safe)") {
    freshState();
    DialogScriptHost::init();
    // Sintaxis invalida: la choice NO debe aparecer (defensivo).
    CHECK_FALSE(DialogScriptHost::evaluate("this is not lua @#$"));
}

TEST_CASE("DialogScriptHost: evaluate accede a dialog.get_var") {
    freshState();
    DialogScriptHost::init();
    GameState::dialogVars()["met_guard"] = "true";
    GameState::dialogVars()["gold"]      = "42";

    CHECK(DialogScriptHost::evaluate("dialog.get_var('met_guard') == 'true'"));
    CHECK_FALSE(DialogScriptHost::evaluate("dialog.get_var('met_guard') == 'false'"));
    CHECK(DialogScriptHost::evaluate("tonumber(dialog.get_var('gold')) > 40"));
    CHECK(DialogScriptHost::evaluate("dialog.has_var('gold')"));
    CHECK_FALSE(DialogScriptHost::evaluate("dialog.has_var('nonexistent')"));
}

TEST_CASE("DialogScriptHost: execute muta dialog.set_var") {
    freshState();
    DialogScriptHost::init();
    DialogScriptHost::execute("dialog.set_var('score', '100')");
    CHECK(GameState::dialogVars()["score"] == "100");

    DialogScriptHost::execute("dialog.set_var('flag', 'on')");
    CHECK(GameState::dialogVars()["flag"] == "on");
}

TEST_CASE("DialogScriptHost: execute con expr vacia es no-op") {
    freshState();
    DialogScriptHost::init();
    DialogScriptHost::execute("");
    // No crash, no efecto.
    CHECK(GameState::dialogVars().empty());
}

TEST_CASE("DialogScriptHost: reset limpia la state pero NO dialogVars") {
    freshState();
    DialogScriptHost::init();
    GameState::dialogVars()["persist"] = "yes";
    DialogScriptHost::reset();
    CHECK_FALSE(DialogScriptHost::isInitialized());
    // dialogVars NO se tocan (decision F2H48 #2).
    CHECK(GameState::dialogVars()["persist"] == "yes");
}

TEST_CASE("DialogScriptHost: init() registra hooks en DialogSystem") {
    freshState();
    GameState::dialogVars()["test_flag"] = "true";
    DialogScriptHost::init();

    // Armar un asset con un choice cuya condition_lua dependa del var.
    Asset a;
    using namespace NodeGraph;
    const NodeId n1 = a.graph().addNode(k_typeDialogLine, {0, 0});
    const NodeId n2 = a.graph().addNode(k_typeDialogLine, {100, 0});
    a.graph().addSocket(n1, SocketKind::Input, k_socketFlow, "in");
    a.graph().addSocket(n2, SocketKind::Input, k_socketFlow, "in");
    // Construir line con 1 choice condicionada.
    Line lr;
    lr.text_literal = "Test";
    Choice c;
    c.label_literal = "Solo si flag";
    c.condition_lua = "dialog.get_var('test_flag') == 'true'";
    lr.choices.push_back(c);
    a.writeLine(n1, lr);
    a.metadata().start_node_id = n1;

    REQUIRE(DialogSystem::start(&a));
    auto choices = DialogSystem::availableChoices();
    REQUIRE(choices.size() == 1u);  // condition true -> aparece

    // Cambiar la var y re-evaluar.
    GameState::dialogVars()["test_flag"] = "false";
    choices = DialogSystem::availableChoices();
    CHECK(choices.empty());  // condition false -> no aparece
}

TEST_CASE("DialogScriptHost: on_select_lua se ejecuta via executor + muta state") {
    freshState();
    DialogScriptHost::init();

    Asset a;
    using namespace NodeGraph;
    const NodeId n1 = a.graph().addNode(k_typeDialogLine, {0, 0});
    const NodeId n2 = a.graph().addNode(k_typeDialogLine, {100, 0});
    a.graph().addSocket(n1, SocketKind::Input, k_socketFlow, "in");
    const SocketId n2In = a.graph().addSocket(n2, SocketKind::Input, k_socketFlow, "in");

    Line lr;
    lr.text_literal = "Pregunta";
    Choice c;
    c.label_literal = "Si";
    c.on_select_lua = "dialog.set_var('answered', 'yes')";
    lr.choices.push_back(c);
    a.writeLine(n1, lr);
    Line l2; l2.text_literal = "Eco"; a.writeLine(n2, l2);

    // Linkear el output del root a n2.
    const Node* root = a.graph().findNode(n1);
    REQUIRE(root != nullptr);
    REQUIRE(!root->outputs.empty());
    a.graph().addLink(root->outputs[0].id, n2In);
    a.metadata().start_node_id = n1;

    REQUIRE(DialogSystem::start(&a));
    REQUIRE(DialogSystem::advance(0));  // disparar on_select_lua
    CHECK(GameState::dialogVars()["answered"] == "yes");
    CHECK(DialogSystem::currentNodeId() == n2);
}
