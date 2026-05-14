// Tests del QuestSystem (F2H53 Bloque C). Cubre:
// - Lifecycle: start / complete / fail / re-start tras Failed
// - Predicate generation por type (Collect/Talk/Reach/CustomLua)
// - Reward code generation por type (Item/Var/Lua) + escaping
// - Tick: evaluator inyectado dispara auto-completion
// - objectiveComplete: marca manual + auto-complete cuando todos done
// - Hooks (start/complete/fail/evaluator/executor)
// - Tracked quest (HUD)
// - reset() limpia state, clearHooks() limpia hooks

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/quest/QuestAsset.h"
#include "engine/quest/QuestSystem.h"
#include "engine/render/rhi/ITexture.h"

using namespace Mood;
using namespace Mood::Quest;

namespace {

class DummyTextureQS : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory dummyTexFactoryQS() {
    return [](const std::string&) { return std::make_unique<DummyTextureQS>(); };
}

// Helper: crea un AssetManager + carga un quest construido en memoria sin
// pasar por disco. Devuelve {am, questId}. Setea reset() al inicio para
// asegurar aislamiento del singleton entre tests.
struct QuestFixture {
    std::unique_ptr<AssetManager> am;
    QuestAssetId questId = 0;

    QuestFixture() {
        QuestSystem::reset();
        QuestSystem::clearHooks();
    }
    ~QuestFixture() {
        QuestSystem::reset();
        QuestSystem::clearHooks();
    }
};

// Como el AssetManager carga quests SOLO desde disco (VFS), para los tests
// del runtime escribimos un .moodquest en tmp y lo cargamos. Helper:
QuestAssetId saveAndLoadQuest(AssetManager& am, const std::filesystem::path& tmpRoot,
                                const std::string& filename, const Asset& a) {
    namespace fs = std::filesystem;
    fs::create_directories(tmpRoot / "quests");
    a.saveToFile(tmpRoot / "quests" / filename);
    return am.loadQuest("quests/" + filename);
}

// Construye un AssetManager apuntando a un tmpRoot recien creado.
std::unique_ptr<AssetManager> makeAmWithTmpRoot(std::filesystem::path& outRoot,
                                                  const char* tag) {
    namespace fs = std::filesystem;
    outRoot = fs::temp_directory_path() / (std::string("mood_quest_sys_") + tag);
    fs::remove_all(outRoot);
    fs::create_directories(outRoot);
    return std::make_unique<AssetManager>(outRoot.string(), dummyTexFactoryQS());
}

} // namespace

// ============================================================
// Helpers de generacion de codigo Lua
// ============================================================

TEST_CASE("QuestSystem::generatePredicate: Collect con item_path + min_quantity") {
    Objective o;
    o.type         = ObjectiveType::Collect;
    o.item_path    = "items/key.mooditem";
    o.min_quantity = 3;
    CHECK(QuestSystem::generatePredicate(o) ==
          "inventory.count('items/key.mooditem') >= 3");
}

TEST_CASE("QuestSystem::generatePredicate: Collect sin item_path retorna vacio") {
    Objective o;
    o.type         = ObjectiveType::Collect;
    o.min_quantity = 1;
    CHECK(QuestSystem::generatePredicate(o).empty());
}

TEST_CASE("QuestSystem::generatePredicate: Talk y Reach usan dialog.has_var") {
    Objective t;
    t.type     = ObjectiveType::Talk;
    t.var_name = "spoke_to_marcus";
    CHECK(QuestSystem::generatePredicate(t) ==
          "dialog.has_var('spoke_to_marcus')");

    Objective r;
    r.type     = ObjectiveType::Reach;
    r.var_name = "entered_forest";
    CHECK(QuestSystem::generatePredicate(r) ==
          "dialog.has_var('entered_forest')");
}

TEST_CASE("QuestSystem::generatePredicate: CustomLua devuelve el predicate tal cual") {
    Objective o;
    o.type             = ObjectiveType::CustomLua;
    o.custom_predicate = "inventory.has('items/orb') and dialog.has_var('met_oracle')";
    CHECK(QuestSystem::generatePredicate(o) ==
          "inventory.has('items/orb') and dialog.has_var('met_oracle')");
}

TEST_CASE("QuestSystem::generatePredicate: CustomLua vacio retorna vacio") {
    Objective o;
    o.type = ObjectiveType::CustomLua;
    CHECK(QuestSystem::generatePredicate(o).empty());
}

TEST_CASE("QuestSystem::generatePredicate: escape de single quote en item_path") {
    // Defensivo: si el path tiene comilla simple, no rompemos la string Lua.
    Objective o;
    o.type         = ObjectiveType::Collect;
    o.item_path    = "items/devil's_horn.mooditem";  // apostrofe
    o.min_quantity = 1;
    CHECK(QuestSystem::generatePredicate(o) ==
          "inventory.count('items/devil\\'s_horn.mooditem') >= 1");
}

TEST_CASE("QuestSystem::generateRewardCode: Item con quantity") {
    Reward r;
    r.type      = RewardType::Item;
    r.item_path = "items/gold.mooditem";
    r.quantity  = 50;
    CHECK(QuestSystem::generateRewardCode(r) ==
          "inventory.add('items/gold.mooditem', 50)");
}

TEST_CASE("QuestSystem::generateRewardCode: Var con key + value") {
    Reward r;
    r.type      = RewardType::Var;
    r.var_name  = "quest_done";
    r.var_value = "yes";
    CHECK(QuestSystem::generateRewardCode(r) ==
          "dialog.set_var('quest_done', 'yes')");
}

TEST_CASE("QuestSystem::generateRewardCode: Lua devuelve el script tal cual") {
    Reward r;
    r.type       = RewardType::Lua;
    r.lua_script = "player_xp = player_xp + 100";
    CHECK(QuestSystem::generateRewardCode(r) ==
          "player_xp = player_xp + 100");
}

TEST_CASE("QuestSystem::generateRewardCode: campos vacios producen string vacio") {
    Reward item;
    item.type = RewardType::Item;
    CHECK(QuestSystem::generateRewardCode(item).empty());

    Reward var;
    var.type = RewardType::Var;
    CHECK(QuestSystem::generateRewardCode(var).empty());

    Reward lua;
    lua.type = RewardType::Lua;
    CHECK(QuestSystem::generateRewardCode(lua).empty());
}

// ============================================================
// Lifecycle: start / complete / fail
// ============================================================

TEST_CASE("QuestSystem::start: arranca un quest valido y lo deja Active") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "start_basic");

    Asset q;
    q.id = "test_start";
    Objective o;
    o.id = "step";
    o.type = ObjectiveType::CustomLua;
    o.custom_predicate = "false";  // nunca satisfacible
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "test_start.moodquest", q);
    REQUIRE(qid != am->missingQuestId());

    CHECK(QuestSystem::start(qid, *am) == true);
    CHECK(QuestSystem::isActive(qid));
    CHECK_FALSE(QuestSystem::isComplete(qid));
    CHECK_FALSE(QuestSystem::isFailed(qid));
    CHECK(QuestSystem::stateOf(qid) == QuestSystem::State::Active);
    CHECK(QuestSystem::snapshot().size() == 1u);

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::start: missingQuestId rechazado") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "start_missing");

    CHECK(QuestSystem::start(am->missingQuestId(), *am) == false);
    CHECK(QuestSystem::snapshot().empty());

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::start: quest sin objectives rechazado") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "start_empty_obj");

    Asset q;
    q.id = "empty";
    // sin objectives

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "empty.moodquest", q);
    CHECK(QuestSystem::start(qid, *am) == false);
    CHECK(QuestSystem::snapshot().empty());

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::start: re-start de Active rechazado") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "start_restart");

    Asset q;
    q.id = "double";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "double.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));
    CHECK(QuestSystem::start(qid, *am) == false);  // re-start bloqueado

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::complete: aplica rewards via executor") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "complete_rewards");

    Asset q;
    q.id = "rewards_test";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);
    Reward r1; r1.type = RewardType::Item;  r1.item_path = "items/gold.mooditem"; r1.quantity = 50;
    Reward r2; r2.type = RewardType::Var;   r2.var_name = "completed"; r2.var_value = "yes";
    q.rewards.push_back(r1);
    q.rewards.push_back(r2);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "rewards_test.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    std::vector<std::string> executed;
    QuestSystem::setExecutor([&](const std::string& code) {
        executed.push_back(code);
    });

    CHECK(QuestSystem::complete(qid, *am));
    CHECK(QuestSystem::isComplete(qid));
    REQUIRE(executed.size() == 2u);
    CHECK(executed[0] == "inventory.add('items/gold.mooditem', 50)");
    CHECK(executed[1] == "dialog.set_var('completed', 'yes')");

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::fail: NO aplica rewards") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "fail_no_rewards");

    Asset q;
    q.id = "fail_test";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);
    Reward r; r.type = RewardType::Item; r.item_path = "items/gold.mooditem"; r.quantity = 50;
    q.rewards.push_back(r);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "fail_test.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    int executorCalls = 0;
    QuestSystem::setExecutor([&](const std::string&) { ++executorCalls; });

    CHECK(QuestSystem::fail(qid));
    CHECK(QuestSystem::isFailed(qid));
    CHECK(executorCalls == 0);  // nada se ejecuta tras fail

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::fail: re-start tras Failed permitido (retry)") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "fail_retry");

    Asset q;
    q.id = "retry";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "retry.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));
    REQUIRE(QuestSystem::fail(qid));
    CHECK(QuestSystem::start(qid, *am));  // retry OK
    CHECK(QuestSystem::isActive(qid));

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::complete sobre quest Complete rechazado") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "double_complete");

    Asset q;
    q.id = "dc";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "dc.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));
    REQUIRE(QuestSystem::complete(qid, *am));
    CHECK(QuestSystem::complete(qid, *am) == false);  // ya Complete

    std::filesystem::remove_all(tmpRoot);
}

// ============================================================
// Tick + evaluator
// ============================================================

TEST_CASE("QuestSystem::tick: evaluator inyectado dispara auto-complete") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "tick_auto");

    Asset q;
    q.id = "tick_test";
    Objective o1; o1.id = "o1"; o1.type = ObjectiveType::Collect;
        o1.item_path = "items/key.mooditem"; o1.min_quantity = 1;
    Objective o2; o2.id = "o2"; o2.type = ObjectiveType::Talk; o2.var_name = "spoke";
    q.objectives.push_back(o1);
    q.objectives.push_back(o2);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "tick_test.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    // Evaluator que simula:
    //   - "inventory.count(...)" = true en primer tick
    //   - "dialog.has_var(...)"  = false en primer tick, true en segundo
    bool spokeYet = false;
    QuestSystem::setEvaluator([&](const std::string& expr) {
        if (expr.find("inventory.count") != std::string::npos) return true;
        if (expr.find("dialog.has_var") != std::string::npos)  return spokeYet;
        return false;
    });

    QuestSystem::tick(*am);
    CHECK(QuestSystem::isActive(qid));  // un objective done, otro no
    REQUIRE(QuestSystem::snapshot().size() == 1u);
    CHECK(QuestSystem::snapshot()[0].objectives[0].completed == true);
    CHECK(QuestSystem::snapshot()[0].objectives[1].completed == false);

    // Simulamos que ahora hablo con el NPC.
    spokeYet = true;
    QuestSystem::tick(*am);
    CHECK(QuestSystem::isComplete(qid));  // auto-complete

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::tick: sin evaluator NO completa automaticamente") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "tick_no_eval");

    Asset q;
    q.id = "noeval";
    Objective o; o.id = "x"; o.type = ObjectiveType::Collect;
        o.item_path = "items/x"; o.min_quantity = 1;
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "noeval.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    // No setEvaluator. tick no debe romper.
    QuestSystem::tick(*am);
    CHECK(QuestSystem::isActive(qid));  // sigue activo

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::tick: predicate vacio (CustomLua sin script) nunca completa") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "tick_empty_pred");

    Asset q;
    q.id = "ep";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua;
    // custom_predicate vacio
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "ep.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    // Evaluator que dice true a TODO — pero como el predicate es vacio,
    // el sistema lo skipea sin llamar al evaluator.
    int evalCalls = 0;
    QuestSystem::setEvaluator([&](const std::string&) { ++evalCalls; return true; });

    QuestSystem::tick(*am);
    CHECK(QuestSystem::isActive(qid));
    CHECK(evalCalls == 0);  // no se llamo (predicate vacio se skipea)

    std::filesystem::remove_all(tmpRoot);
}

// ============================================================
// objectiveComplete manual
// ============================================================

TEST_CASE("QuestSystem::objectiveComplete: marca uno y deja el resto") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "objc_partial");

    Asset q;
    q.id = "objc";
    Objective o1; o1.id = "a"; o1.type = ObjectiveType::CustomLua; o1.custom_predicate = "false";
    Objective o2; o2.id = "b"; o2.type = ObjectiveType::CustomLua; o2.custom_predicate = "false";
    q.objectives.push_back(o1);
    q.objectives.push_back(o2);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "objc.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    CHECK(QuestSystem::objectiveComplete(qid, "a", *am));
    CHECK(QuestSystem::isActive(qid));  // sigue activo, falta "b"
    CHECK(QuestSystem::snapshot()[0].objectives[0].completed);
    CHECK_FALSE(QuestSystem::snapshot()[0].objectives[1].completed);

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::objectiveComplete: marca el ultimo y auto-completa") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "objc_autocomplete");

    Asset q;
    q.id = "ac";
    Objective o; o.id = "only_one"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "ac.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    CHECK(QuestSystem::objectiveComplete(qid, "only_one", *am));
    CHECK(QuestSystem::isComplete(qid));  // auto-complete

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::objectiveComplete: objective id desconocido rechazado") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "objc_unknown");

    Asset q;
    q.id = "uk";
    Objective o; o.id = "real_id"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);

    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "uk.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    CHECK_FALSE(QuestSystem::objectiveComplete(qid, "garbage_id", *am));
    CHECK(QuestSystem::isActive(qid));

    std::filesystem::remove_all(tmpRoot);
}

// ============================================================
// Hooks de notificacion
// ============================================================

TEST_CASE("QuestSystem hooks: start / complete / fail disparan callbacks") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "hooks_basic");

    Asset q;
    q.id = "h";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);
    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "h.moodquest", q);

    QuestAssetId startedId = 0;
    QuestAssetId completedId = 0;
    QuestAssetId failedId = 0;
    QuestSystem::setStartHook   ([&](QuestAssetId id) { startedId = id; });
    QuestSystem::setCompleteHook([&](QuestAssetId id) { completedId = id; });
    QuestSystem::setFailHook    ([&](QuestAssetId id) { failedId = id; });

    REQUIRE(QuestSystem::start(qid, *am));
    CHECK(startedId == qid);

    REQUIRE(QuestSystem::complete(qid, *am));
    CHECK(completedId == qid);

    // failedId NO se setea — solo se llamo complete, no fail.
    CHECK(failedId == 0);

    std::filesystem::remove_all(tmpRoot);
}

// ============================================================
// Tracked quest
// ============================================================

TEST_CASE("QuestSystem::tracked: default 0; setTracked persiste") {
    QuestFixture fx;
    CHECK(QuestSystem::tracked() == 0u);
    QuestSystem::setTracked(42u);
    CHECK(QuestSystem::tracked() == 42u);
}

TEST_CASE("QuestSystem::reset: limpia activos + tracked, NO hooks") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "reset_test");

    Asset q;
    q.id = "r";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);
    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "r.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));
    QuestSystem::setTracked(qid);

    int hookCount = 0;
    QuestSystem::setStartHook([&](QuestAssetId) { ++hookCount; });

    QuestSystem::reset();
    CHECK(QuestSystem::snapshot().empty());
    CHECK(QuestSystem::tracked() == 0u);

    // El hook sigue vivo: si volvemos a start, se dispara.
    REQUIRE(QuestSystem::start(qid, *am));
    CHECK(hookCount == 1);

    std::filesystem::remove_all(tmpRoot);
}

TEST_CASE("QuestSystem::clearHooks: limpia hooks, NO activos") {
    QuestFixture fx;
    std::filesystem::path tmpRoot;
    auto am = makeAmWithTmpRoot(tmpRoot, "clearhooks");

    Asset q;
    q.id = "ch";
    Objective o; o.id = "x"; o.type = ObjectiveType::CustomLua; o.custom_predicate = "false";
    q.objectives.push_back(o);
    const QuestAssetId qid = saveAndLoadQuest(*am, tmpRoot, "ch.moodquest", q);
    REQUIRE(QuestSystem::start(qid, *am));

    int hookCount = 0;
    QuestSystem::setCompleteHook([&](QuestAssetId) { ++hookCount; });

    QuestSystem::clearHooks();

    // Active queda; hook eliminado: complete NO dispara el callback.
    CHECK(QuestSystem::isActive(qid));
    REQUIRE(QuestSystem::complete(qid, *am));
    CHECK(hookCount == 0);

    std::filesystem::remove_all(tmpRoot);
}
