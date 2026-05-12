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

#include "engine/assets/manager/AssetManager.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogScriptHost.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <memory>

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

// ============================================================
// F2H52 Bloque G: gating de dialog choices con inventory.has()
// ============================================================

namespace {

class DummyTexture : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory dummyTexFactory() {
    return [](const std::string&) { return std::make_unique<DummyTexture>(); };
}

/// Helper: setea scene + assets con un player que tiene InventoryComponent
/// y un item de quest en el AssetManager. Devuelve la fixture viva (el
/// caller la mantiene mientras corre el test).
struct DialogInvFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    Scene scene;
    Entity player;
    ItemAssetId keyId = 0;
};

std::unique_ptr<DialogInvFixture> makeDialogInvFixture(const std::string& tag) {
    namespace fs = std::filesystem;
    auto fx = std::make_unique<DialogInvFixture>();
    fx->tmpRoot = fs::temp_directory_path() / ("mood_dialog_inv_" + tag);
    fs::remove_all(fx->tmpRoot);
    fs::create_directories(fx->tmpRoot / "items");

    Inventory::Asset key;
    key.id = "quest_key";
    key.name_literal = "Quest Key";
    key.tags = {"quest_item"};
    key.saveToFile(fx->tmpRoot / "items" / "quest_key.mooditem");

    fx->am = std::make_unique<AssetManager>(fx->tmpRoot.string(), dummyTexFactory());
    fx->keyId = fx->am->loadItem("items/quest_key.mooditem");
    REQUIRE(fx->keyId != fx->am->missingItemId());

    fx->player = fx->scene.createEntity("Player");
    fx->player.getComponent<TagComponent>().name = "Player";
    fx->player.addComponent<InventoryComponent>();
    return fx;
}

} // namespace

TEST_CASE("DialogScriptHost: condition_lua con inventory.has() gateaa choices") {
    freshState();
    auto fx = makeDialogInvFixture("gate_by_inv");
    DialogScriptHost::init();
    DialogScriptHost::setSceneAndAssets(&fx->scene, fx->am.get());

    // Sin la key: la choice no aparece.
    CHECK_FALSE(DialogScriptHost::evaluate(
        "inventory.has('items/quest_key.mooditem')"));

    // Agregar la key: la choice aparece.
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->keyId, 1, *fx->am);
    CHECK(DialogScriptHost::evaluate(
        "inventory.has('items/quest_key.mooditem')"));

    // Limpiar scene/assets: queries vuelven a false (estado post-Play
    // Mode).
    DialogScriptHost::setSceneAndAssets(nullptr, nullptr);
    CHECK_FALSE(DialogScriptHost::evaluate(
        "inventory.has('items/quest_key.mooditem')"));
}

TEST_CASE("DialogScriptHost: on_select_lua puede llamar inventory.remove") {
    freshState();
    auto fx = makeDialogInvFixture("on_select_remove");
    DialogScriptHost::init();
    DialogScriptHost::setSceneAndAssets(&fx->scene, fx->am.get());

    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->keyId, 1, *fx->am);
    CHECK(inv.state.count(fx->keyId) == 1);

    // Caso "el NPC te quita la llave al elegir esa opcion":
    DialogScriptHost::execute(
        "inventory.remove('items/quest_key.mooditem', 1)");
    CHECK(inv.state.count(fx->keyId) == 0);
}

TEST_CASE("DialogScriptHost: condition_lua complejo combina vars + inventory") {
    freshState();
    auto fx = makeDialogInvFixture("combine_vars_inv");
    DialogScriptHost::init();
    DialogScriptHost::setSceneAndAssets(&fx->scene, fx->am.get());

    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->keyId, 1, *fx->am);
    GameState::dialogVars()["spoke_to_guard"] = "true";

    // Choice aparece solo si tenes la llave Y hablaste con el guardia.
    const std::string expr =
        "inventory.has('items/quest_key.mooditem') and "
        "dialog.get_var('spoke_to_guard') == 'true'";
    CHECK(DialogScriptHost::evaluate(expr));

    // Sin var seteada -> false.
    GameState::dialogVars().erase("spoke_to_guard");
    CHECK_FALSE(DialogScriptHost::evaluate(expr));
}
