// Tests de los Lua bindings de la tabla `quest` (F2H53 Bloque E).
// Cubre: start / complete / fail / objective_complete / state / queries /
// snapshot / set_tracked / tracked / on_start / on_complete / on_fail +
// la integracion con el evaluator/executor (tick auto-evalua predicates
// que combinan inventory.* + dialog.has_var).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/game/state/GameState.h"
#include "engine/inventory/InventoryHooks.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/quest/QuestAsset.h"
#include "engine/quest/QuestSystem.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/LuaBindings.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace Mood;
namespace QS = Mood::Quest::QuestSystem;

namespace {

class DummyTex : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory dummyTexFactory() {
    return [](const std::string&) { return std::make_unique<DummyTex>(); };
}

/// @brief Fixture: AssetManager con 1 item + 2 quests (q_fetch con
///        objective Collect, q_combo con 2 objectives: Collect + Talk).
///        Scene con player que tiene InventoryComponent. sol::state con
///        bindings de inventory + quest registrados.
struct QuestLuaFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    Scene scene;
    Entity player;
    Entity scriptEntity;
    ItemAssetId  potionId    = 0;
    QuestAssetId qFetchId    = 0;
    QuestAssetId qComboId    = 0;
    sol::state lua;

    // Limpieza: misma logica que F2H52 (J) — los hooks de Inventory + Quest
    // capturan sol::function de esta `lua`. Si la state muere antes que el
    // hook, el destructor de la sol::function colgada hace lua_unref sobre
    // un VM muerto. Limpieza explicita ANTES de destruir `lua`.
    ~QuestLuaFixture() {
        QS::clearHooks();
        QS::reset();
        Inventory::Hooks::clearAll();
        GameState::dialogVars().clear();
    }
};

/// @brief Registra `dialog.set_var/get_var/has_var/clear_vars` sobre la
///        sol::state dada usando `GameState::dialogVars()` como backing
///        store (mismo storage que el DialogScriptHost). Helper local a
///        este test — el motor "de verdad" usa el `DialogScriptHost` con
///        su propia sol::state, pero en tests queremos los bindings en
///        la misma state donde corren los predicates del QuestSystem.
void registerDialogVarsBindings(sol::state& lua) {
    sol::table d = lua.create_named_table("dialog");
    d.set_function("set_var",
        [](const std::string& k, const std::string& v) {
            GameState::dialogVars()[k] = v;
        });
    d.set_function("get_var",
        [](const std::string& k) -> std::string {
            auto& v = GameState::dialogVars();
            auto it = v.find(k);
            return it != v.end() ? it->second : std::string{};
        });
    d.set_function("has_var",
        [](const std::string& k) {
            return GameState::dialogVars().count(k) > 0;
        });
    d.set_function("clear_vars",
        []() { GameState::dialogVars().clear(); });
}

std::unique_ptr<QuestLuaFixture> makeFixture(const std::string& tag) {
    namespace fs = std::filesystem;
    auto fx = std::make_unique<QuestLuaFixture>();
    fx->tmpRoot = fs::temp_directory_path() / ("mood_quest_lua_" + tag);
    fs::remove_all(fx->tmpRoot);
    fs::create_directories(fx->tmpRoot / "items");
    fs::create_directories(fx->tmpRoot / "quests");

    // -- Items --
    Inventory::Asset potion;
    potion.id = "health_potion";
    potion.name_literal = "Health Potion";
    potion.stack.stackable = true;
    potion.stack.max_stack = 10;
    potion.stats["heal"] = 20.0f;
    potion.saveToFile(fx->tmpRoot / "items" / "health_potion.mooditem");

    // -- Quests --
    Quest::Asset qFetch;
    qFetch.id = "q_fetch";
    qFetch.name_literal = "Fetch 3 Potions";
    qFetch.category = "side";
    {
        Quest::Objective o;
        o.id = "get_potions";
        o.name_literal = "Conseguí 3 pociones";
        o.type = Quest::ObjectiveType::Collect;
        o.item_path = "items/health_potion.mooditem";
        o.min_quantity = 3;
        qFetch.objectives = {o};
    }
    qFetch.saveToFile(fx->tmpRoot / "quests" / "q_fetch.moodquest");

    Quest::Asset qCombo;
    qCombo.id = "q_combo";
    qCombo.name_literal = "Talk + Fetch";
    qCombo.category = "main";
    {
        Quest::Objective o1;
        o1.id = "spoke_to_npc";
        o1.type = Quest::ObjectiveType::Talk;
        o1.var_name = "spoke_marta";
        Quest::Objective o2;
        o2.id = "got_item";
        o2.type = Quest::ObjectiveType::Collect;
        o2.item_path = "items/health_potion.mooditem";
        o2.min_quantity = 1;
        qCombo.objectives = {o1, o2};
    }
    {
        Quest::Reward r;
        r.type = Quest::RewardType::Var;
        r.var_name = "q_combo_done";
        r.var_value = "yes";
        qCombo.rewards = {r};
    }
    qCombo.saveToFile(fx->tmpRoot / "quests" / "q_combo.moodquest");

    fx->am = std::make_unique<AssetManager>(fx->tmpRoot.string(),
                                              dummyTexFactory());
    fx->potionId = fx->am->loadItem("items/health_potion.mooditem");
    fx->qFetchId = fx->am->loadQuest("quests/q_fetch.moodquest");
    fx->qComboId = fx->am->loadQuest("quests/q_combo.moodquest");
    REQUIRE(fx->potionId != fx->am->missingItemId());
    REQUIRE(fx->qFetchId != fx->am->missingQuestId());
    REQUIRE(fx->qComboId != fx->am->missingQuestId());

    fx->player = fx->scene.createEntity("Player");
    fx->player.getComponent<TagComponent>().name = "Player";
    fx->player.addComponent<InventoryComponent>();

    fx->scriptEntity = fx->scene.createEntity("script_owner");

    // Reset estado global ANTES de instalar bindings (el ctor del test
    // anterior podria haber dejado state colgando).
    QS::reset();
    QS::clearHooks();
    Inventory::Hooks::clearAll();
    GameState::dialogVars().clear();

    fx->lua.open_libraries(sol::lib::base);
    // Orden importa: inventory + dialog vars ANTES de quest, para que el
    // evaluator/executor del QuestSystem encuentre los bindings al evaluar
    // predicates tipo `inventory.count(...)` o `dialog.has_var(...)`.
    setupInventoryBindings(fx->lua, &fx->scene, fx->am.get());
    registerDialogVarsBindings(fx->lua);
    setupQuestBindings(fx->lua, fx->am.get());
    return fx;
}

} // namespace

// ============================================================
// Lifecycle basico
// ============================================================

TEST_CASE("quest.start: arranca quest valido + warn si path no resuelve") {
    auto fx = makeFixture("start_basic");
    fx->lua.safe_script("ok = quest.start('quests/q_fetch.moodquest')");
    CHECK(fx->lua["ok"] == true);
    CHECK(QS::isActive(fx->qFetchId));

    // Path inexistente: warn + false.
    fx->lua.safe_script("ko = quest.start('quests/nope.moodquest')");
    CHECK(fx->lua["ko"] == false);
}

TEST_CASE("quest.start: rechaza re-start si ya esta Active") {
    auto fx = makeFixture("start_double");
    fx->lua.safe_script("a = quest.start('quests/q_fetch.moodquest')");
    fx->lua.safe_script("b = quest.start('quests/q_fetch.moodquest')");
    CHECK(fx->lua["a"] == true);
    CHECK(fx->lua["b"] == false);
}

TEST_CASE("quest.complete: aplica reward via executor") {
    auto fx = makeFixture("complete_reward");
    fx->lua.safe_script(R"(
        quest.start('quests/q_combo.moodquest')
        ok = quest.complete('quests/q_combo.moodquest')
        v = dialog.get_var('q_combo_done')
    )");
    CHECK(fx->lua["ok"] == true);
    CHECK(fx->lua["v"] == std::string("yes"));
    CHECK(QS::isComplete(fx->qComboId));
}

TEST_CASE("quest.fail: transiciona a Failed") {
    auto fx = makeFixture("fail_basic");
    fx->lua.safe_script(R"(
        quest.start('quests/q_fetch.moodquest')
        ok = quest.fail('quests/q_fetch.moodquest')
    )");
    CHECK(fx->lua["ok"] == true);
    CHECK(QS::isFailed(fx->qFetchId));
}

TEST_CASE("quest.objective_complete: marca uno + queda Active si quedan pendientes") {
    auto fx = makeFixture("oc_partial");
    fx->lua.safe_script(R"(
        quest.start('quests/q_combo.moodquest')
        ok = quest.objective_complete('quests/q_combo.moodquest', 'spoke_to_npc')
    )");
    CHECK(fx->lua["ok"] == true);
    CHECK(QS::isActive(fx->qComboId));
}

TEST_CASE("quest.objective_complete: auto-completa si era el ultimo") {
    auto fx = makeFixture("oc_auto");
    fx->lua.safe_script(R"(
        quest.start('quests/q_combo.moodquest')
        quest.objective_complete('quests/q_combo.moodquest', 'spoke_to_npc')
        quest.objective_complete('quests/q_combo.moodquest', 'got_item')
    )");
    CHECK(QS::isComplete(fx->qComboId));
    // Reward de q_combo aplicada:
    fx->lua.safe_script("v = dialog.get_var('q_combo_done')");
    CHECK(fx->lua["v"] == std::string("yes"));
}

// ============================================================
// Queries
// ============================================================

TEST_CASE("quest.state: mapea State enum a string") {
    auto fx = makeFixture("state_strs");
    fx->lua.safe_script("s0 = quest.state('quests/q_fetch.moodquest')");
    CHECK(fx->lua["s0"] == std::string("not_started"));

    fx->lua.safe_script("quest.start('quests/q_fetch.moodquest')");
    fx->lua.safe_script("s1 = quest.state('quests/q_fetch.moodquest')");
    CHECK(fx->lua["s1"] == std::string("active"));

    fx->lua.safe_script("quest.fail('quests/q_fetch.moodquest')");
    fx->lua.safe_script("s2 = quest.state('quests/q_fetch.moodquest')");
    CHECK(fx->lua["s2"] == std::string("failed"));
}

TEST_CASE("quest.is_active / is_complete / is_failed: paridad con state()") {
    auto fx = makeFixture("is_xxx");
    fx->lua.safe_script(R"(
        a0 = quest.is_active('quests/q_fetch.moodquest')
        quest.start('quests/q_fetch.moodquest')
        a1 = quest.is_active('quests/q_fetch.moodquest')
        quest.complete('quests/q_fetch.moodquest')
        a2 = quest.is_active('quests/q_fetch.moodquest')
        c2 = quest.is_complete('quests/q_fetch.moodquest')
    )");
    CHECK(fx->lua["a0"] == false);
    CHECK(fx->lua["a1"] == true);
    CHECK(fx->lua["a2"] == false);
    CHECK(fx->lua["c2"] == true);
}

TEST_CASE("quest.snapshot: devuelve array de quests activos con paths + estado") {
    auto fx = makeFixture("snapshot");
    fx->lua.safe_script(R"(
        quest.start('quests/q_fetch.moodquest')
        quest.start('quests/q_combo.moodquest')
        quest.objective_complete('quests/q_combo.moodquest', 'spoke_to_npc')
        snap = quest.snapshot()
        n = #snap
        p1 = snap[1].path
        s1 = snap[1].state
        p2 = snap[2].path
        s2 = snap[2].state
        done2 = snap[2].objectives_done
        total2 = snap[2].objectives_total
    )");
    CHECK(fx->lua["n"] == 2);
    CHECK(fx->lua["s1"] == std::string("active"));
    CHECK(fx->lua["s2"] == std::string("active"));
    CHECK(fx->lua["done2"] == 1);
    CHECK(fx->lua["total2"] == 2);
}

// ============================================================
// Tracked
// ============================================================

TEST_CASE("quest.set_tracked + tracked: path roundtrip + clear") {
    auto fx = makeFixture("track");
    fx->lua.safe_script(R"(
        t0 = quest.tracked()
        quest.set_tracked('quests/q_fetch.moodquest')
        t1 = quest.tracked()
        quest.set_tracked(nil)
        t2 = quest.tracked()
        quest.set_tracked('quests/q_combo.moodquest')
        quest.set_tracked('')   -- desactivar via string vacio
        t3 = quest.tracked()
    )");
    CHECK(fx->lua["t0"] == std::string(""));
    CHECK(fx->lua["t1"] == std::string("quests/q_fetch.moodquest"));
    CHECK(fx->lua["t2"] == std::string(""));
    CHECK(fx->lua["t3"] == std::string(""));
}

// ============================================================
// Hooks
// ============================================================

TEST_CASE("quest.on_start / on_complete / on_fail: callbacks reciben path") {
    auto fx = makeFixture("hooks");
    fx->lua.safe_script(R"(
        started_with = ""
        completed_with = ""
        failed_with = ""
        quest.on_start(function(p) started_with = p end)
        quest.on_complete(function(p) completed_with = p end)
        quest.on_fail(function(p) failed_with = p end)

        quest.start('quests/q_fetch.moodquest')
        quest.complete('quests/q_fetch.moodquest')

        quest.start('quests/q_combo.moodquest')
        quest.fail('quests/q_combo.moodquest')
    )");
    CHECK(fx->lua["started_with"] == std::string("quests/q_combo.moodquest"));
    CHECK(fx->lua["completed_with"] == std::string("quests/q_fetch.moodquest"));
    CHECK(fx->lua["failed_with"] == std::string("quests/q_combo.moodquest"));
}

// ============================================================
// Integracion runtime — evaluator/executor evaluan predicates via Lua
// ============================================================

TEST_CASE("tick: auto-completa Collect cuando count >= min_quantity") {
    auto fx = makeFixture("tick_collect");
    fx->lua.safe_script("quest.start('quests/q_fetch.moodquest')");
    REQUIRE(QS::isActive(fx->qFetchId));

    // Sin items: tick no completa.
    QS::tick(*fx->am);
    CHECK(QS::isActive(fx->qFetchId));

    // 2 items: insuficiente (q_fetch requiere 3).
    fx->lua.safe_script("inventory.add('items/health_potion.mooditem', 2)");
    QS::tick(*fx->am);
    CHECK(QS::isActive(fx->qFetchId));

    // 1 mas -> 3, completa.
    fx->lua.safe_script("inventory.add('items/health_potion.mooditem', 1)");
    QS::tick(*fx->am);
    CHECK(QS::isComplete(fx->qFetchId));
}

TEST_CASE("tick: combina Talk (dialog.has_var) + Collect en mismo quest") {
    auto fx = makeFixture("tick_combo");
    fx->lua.safe_script("quest.start('quests/q_combo.moodquest')");

    // Sin var ni items.
    QS::tick(*fx->am);
    CHECK(QS::isActive(fx->qComboId));

    // Solo var (objective Talk done).
    fx->lua.safe_script("dialog.set_var('spoke_marta', 'yes')");
    QS::tick(*fx->am);
    CHECK(QS::isActive(fx->qComboId));

    // Item -> completa.
    fx->lua.safe_script("inventory.add('items/health_potion.mooditem', 1)");
    QS::tick(*fx->am);
    CHECK(QS::isComplete(fx->qComboId));
    // Reward aplicada:
    fx->lua.safe_script("v = dialog.get_var('q_combo_done')");
    CHECK(fx->lua["v"] == std::string("yes"));
}

TEST_CASE("evaluator: predicate con syntax error -> warn + objective queda incompleto") {
    auto fx = makeFixture("eval_syntax_err");
    // Inyectar un quest con custom_lua roto.
    namespace fs = std::filesystem;
    Quest::Asset broken;
    broken.id = "q_broken";
    broken.name_literal = "Broken predicate";
    Quest::Objective o;
    o.id = "syntax_err";
    o.type = Quest::ObjectiveType::CustomLua;
    o.custom_predicate = "this is not valid lua )(";
    broken.objectives = {o};
    broken.saveToFile(fx->tmpRoot / "quests" / "q_broken.moodquest");

    fx->am->loadQuest("quests/q_broken.moodquest");
    fx->lua.safe_script("ok = quest.start('quests/q_broken.moodquest')");
    CHECK(fx->lua["ok"] == true);

    // Ticks: evaluator falla cada tick (warn logueado) pero el quest sigue
    // Active — no auto-completa ni hace crash.
    QS::tick(*fx->am);
    QS::tick(*fx->am);
    CHECK(QS::isActive(fx->am->loadQuest("quests/q_broken.moodquest")));
}

TEST_CASE("snapshot: ignora quests reseteados (post quest.reset()-like state)") {
    auto fx = makeFixture("snap_empty");
    fx->lua.safe_script("snap = quest.snapshot(); n = #snap");
    CHECK(fx->lua["n"] == 0);
}
