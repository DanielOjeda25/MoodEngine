// break-A3 — Test del QuestScriptHost: auto-avance de quests sin
// depender de entidades-script.
//
// Antes de break-A3, el evaluator y executor del QuestSystem se
// registraban dentro de `setupQuestBindings` (LuaBindings_Quest),
// capturando &lua de la sol::state de cada entidad-script. Si la
// escena no tenia ninguna entidad con ScriptComponent, ambos hooks
// quedaban en nullptr y los objectives Collect/Talk/Reach jamas se
// auto-completaban — el Quest Log se veia congelado.
//
// Estos tests garantizan que el host independiente (QuestScriptHost
// con sol::state propia) hace al QuestSystem 100% funcional sin
// scripts. Cubrimos:
//   - Quest Collect auto-completa cuando el inventario alcanza
//     min_quantity (vía inventory.count predicate del host).
//   - Quest con Reward SetVar dispara el executor y `dialog.has_var`
//     ve la var post-complete.
//   - Limpieza: reset() del host tira la sol::state y el QuestSystem
//     vuelve a su estado dormido.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/game/state/GameState.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/quest/QuestAsset.h"
#include "engine/quest/QuestScriptHost.h"
#include "engine/quest/QuestSystem.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <memory>

using namespace Mood;
using namespace Mood::Quest;

namespace {

class NullTexA3 : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory nullTexFactoryA3() {
    return [](const std::string&) { return std::make_unique<NullTexA3>(); };
}

struct A3Fixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    std::unique_ptr<Scene> scene;
    ItemAssetId  keyId   = 0;
    QuestAssetId questId = 0;

    A3Fixture(const std::string& tag, int minQty, bool addRewardSetVar) {
        namespace fs = std::filesystem;
        tmpRoot = fs::temp_directory_path() / ("mood_quest_a3_" + tag);
        fs::remove_all(tmpRoot);
        fs::create_directories(tmpRoot / "items");
        fs::create_directories(tmpRoot / "quests");

        // Item asset.
        Mood::Inventory::Asset itemAsset;
        itemAsset.id = "iron_key";
        itemAsset.stack.stackable = true;
        itemAsset.stack.max_stack = 99;
        itemAsset.saveToFile(tmpRoot / "items" / "iron_key.mooditem");

        // Quest asset: 1 objective Collect.
        Asset q;
        q.id           = "collect_keys";
        q.name_literal = "Collect 3 keys";
        Objective o;
        o.type         = ObjectiveType::Collect;
        o.item_path    = "items/iron_key.mooditem";
        o.min_quantity = minQty;
        q.objectives.push_back(o);
        if (addRewardSetVar) {
            Reward r;
            r.type     = RewardType::Var;
            r.var_name = "quest_done";
            r.var_value = "true";
            q.rewards.push_back(r);
        }
        q.saveToFile(tmpRoot / "quests" / "collect_keys.moodquest");

        am = std::make_unique<AssetManager>(tmpRoot.string(), nullTexFactoryA3());
        keyId   = am->loadItem("items/iron_key.mooditem");
        questId = am->loadQuest("quests/collect_keys.moodquest");

        // Scene con player con InventoryComponent FlatList. SIN
        // ScriptComponent en ninguna entity — ese es el corazon de A3.
        scene = std::make_unique<Scene>();
        Entity player = scene->createEntity("Player");
        InventoryComponent inv{};
        inv.state.mode = Mood::Inventory::LayoutMode::FlatList;
        inv.state.config.max_items = 20;
        player.addComponent<InventoryComponent>(inv);

        QuestSystem::reset();
        QuestSystem::clearHooks();
        QuestScriptHost::reset(); // garantiza state fresca
        QuestScriptHost::init();
        QuestScriptHost::setSceneAndAssets(scene.get(), am.get());
    }

    ~A3Fixture() {
        QuestSystem::reset();
        QuestSystem::clearHooks();
        QuestScriptHost::setSceneAndAssets(nullptr, nullptr);
        QuestScriptHost::reset();
        Mood::GameState::dialogVars().clear();
        std::filesystem::remove_all(tmpRoot);
    }

    Mood::Inventory::State& playerInventory() {
        Mood::Inventory::State* out = nullptr;
        scene->forEach<TagComponent, InventoryComponent>(
            [&](Entity, TagComponent& tag, InventoryComponent& inv) {
                if (tag.name == "Player") out = &inv.state;
            });
        REQUIRE(out != nullptr);
        return *out;
    }
};

} // namespace

TEST_CASE("break-A3: Quest Collect auto-completa con QuestScriptHost (cero scripts)") {
    A3Fixture fx("collect_no_script", /*minQty*/ 3, /*reward*/ false);
    REQUIRE(QuestScriptHost::isInitialized());
    REQUIRE(fx.questId != fx.am->missingQuestId());

    // Inicio: quest Active sin items.
    QuestSystem::start(fx.questId, *fx.am);
    REQUIRE(QuestSystem::isActive(fx.questId));

    // Tick con inventario vacio -> sigue Active (predicate da false).
    QuestSystem::tick(*fx.am);
    CHECK(QuestSystem::isActive(fx.questId));
    CHECK_FALSE(QuestSystem::isComplete(fx.questId));

    // Agregar 2 items -> aun no alcanza min_quantity=3.
    REQUIRE(fx.playerInventory().add(fx.keyId, 2, *fx.am));
    QuestSystem::tick(*fx.am);
    CHECK(QuestSystem::isActive(fx.questId));

    // Sumar 1 mas -> objective predicate pasa, quest completa.
    REQUIRE(fx.playerInventory().add(fx.keyId, 1, *fx.am));
    QuestSystem::tick(*fx.am);
    CHECK(QuestSystem::isComplete(fx.questId));
}

TEST_CASE("break-A3: Reward Var dispara el executor del host -> dialog.has_var true") {
    A3Fixture fx("reward_var", /*minQty*/ 1, /*reward*/ true);
    REQUIRE(QuestScriptHost::isInitialized());
    CHECK_FALSE(Mood::GameState::dialogVars().count("quest_done") > 0);

    QuestSystem::start(fx.questId, *fx.am);
    REQUIRE(fx.playerInventory().add(fx.keyId, 1, *fx.am));
    QuestSystem::tick(*fx.am);

    REQUIRE(QuestSystem::isComplete(fx.questId));
    // El executor del host corrio el reward y `dialog.set_var(...)`
    // se aplico al storage de GameState::dialogVars.
    CHECK(Mood::GameState::dialogVars().count("quest_done") == 1);
    CHECK(Mood::GameState::dialogVars().at("quest_done") == "true");
}

TEST_CASE("break-A3: reset() del host libera la state pero no rompe QuestSystem") {
    A3Fixture fx("reset_lifetime", /*minQty*/ 1, /*reward*/ false);
    REQUIRE(QuestScriptHost::isInitialized());

    // Reset apaga la state — los hooks que QuestSystem tenia siguen
    // siendo std::function validos (apuntan a evaluate/execute del
    // host), pero llamarlos despues reconstruira la state vacia
    // (perdiendo bindings de inventory) — eso ESTA OK para un test
    // que solo verifica que no se cuelga.
    QuestScriptHost::reset();
    CHECK_FALSE(QuestScriptHost::isInitialized());

    // tick post-reset no debe crashear. El evaluator del host se
    // re-inicializa lazy en `evaluate(...)` con state fresca + bindings
    // (sin scene/assets -> inventory.count siempre 0 -> objective sigue
    // Active). No assertamos comportamiento aqui, solo no-crash.
    QuestSystem::start(fx.questId, *fx.am);
    QuestSystem::tick(*fx.am);
    CHECK(QuestSystem::isActive(fx.questId));
}
