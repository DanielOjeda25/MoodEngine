// Tests de los Lua bindings de la tabla `inventory` (F2H52 Bloque E).
// Cubre: has / count / entries / sum_stat / add / remove / spawn_pickup.
// Cubre ambas variantes: con entity explicita y con player-implicit
// (scan por TagComponent.name == "Player").

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scripting/bindings/LuaBindings.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace Mood;

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

/// @brief Fixture compartida: scene con player (tag "Player" + Inventory
/// FlatList max=20) + AssetManager con 2 items ("iron_sword" weapon dmg=10,
/// "health_potion" stackable max=10 con stat heal=20).
struct InvLuaFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    Scene scene;
    Entity player;
    Entity scriptEntity;  // self del script (cualquier entity, no tiene que ser player)
    ItemAssetId swordId = 0;
    ItemAssetId potionId = 0;
    sol::state lua;
};

std::unique_ptr<InvLuaFixture> makeFixture(const std::string& tag) {
    namespace fs = std::filesystem;
    auto fx = std::make_unique<InvLuaFixture>();
    fx->tmpRoot = fs::temp_directory_path() / ("mood_inv_lua_" + tag);
    fs::remove_all(fx->tmpRoot);
    fs::create_directories(fx->tmpRoot / "items");

    Inventory::Asset sword;
    sword.id = "iron_sword";
    sword.name_literal = "Iron Sword";
    sword.tags = {"weapon"};
    sword.stack.stackable = false;
    sword.stats["damage"] = 10.0f;
    sword.stats["weight"] = 3.0f;
    sword.saveToFile(fx->tmpRoot / "items" / "iron_sword.mooditem");

    Inventory::Asset potion;
    potion.id = "health_potion";
    potion.name_literal = "Health Potion";
    potion.tags = {"consumable"};
    potion.stack.stackable = true;
    potion.stack.max_stack = 10;
    potion.stats["heal"] = 20.0f;
    potion.stats["weight"] = 0.5f;
    potion.saveToFile(fx->tmpRoot / "items" / "health_potion.mooditem");

    fx->am = std::make_unique<AssetManager>(fx->tmpRoot.string(), dummyTexFactory());
    fx->swordId  = fx->am->loadItem("items/iron_sword.mooditem");
    fx->potionId = fx->am->loadItem("items/health_potion.mooditem");
    REQUIRE(fx->swordId != fx->am->missingItemId());
    REQUIRE(fx->potionId != fx->am->missingItemId());

    fx->player = fx->scene.createEntity("Player");
    fx->player.getComponent<TagComponent>().name = "Player";
    fx->player.addComponent<InventoryComponent>();

    fx->scriptEntity = fx->scene.createEntity("script_owner");

    fx->lua.open_libraries(sol::lib::base);
    setupInventoryBindings(fx->lua, fx->scriptEntity, fx->am.get());
    return fx;
}

} // namespace

// ============================================================
// Lecturas — has / count / entries / sum_stat
// ============================================================

TEST_CASE("inventory.has: explicito + player-implicit") {
    auto fx = makeFixture("has_basic");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->swordId, 1, *fx->am);

    // Explicito con entity.
    fx->lua["target"] = fx->player;
    fx->lua.safe_script(
        "result_explicit = inventory.has(target, 'items/iron_sword.mooditem')");
    CHECK(fx->lua["result_explicit"] == true);

    fx->lua.safe_script(
        "result_explicit_false = inventory.has(target, 'items/health_potion.mooditem')");
    CHECK(fx->lua["result_explicit_false"] == false);

    // Player-implicit (sin pasar entity).
    fx->lua.safe_script(
        "result_implicit = inventory.has('items/iron_sword.mooditem')");
    CHECK(fx->lua["result_implicit"] == true);

    fx->lua.safe_script(
        "result_implicit_false = inventory.has('items/health_potion.mooditem')");
    CHECK(fx->lua["result_implicit_false"] == false);
}

TEST_CASE("inventory.has: min_qty controla el umbral") {
    auto fx = makeFixture("has_minqty");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->potionId, 5, *fx->am);

    fx->lua.safe_script("r3 = inventory.has('items/health_potion.mooditem', 3)");
    fx->lua.safe_script("r5 = inventory.has('items/health_potion.mooditem', 5)");
    fx->lua.safe_script("r6 = inventory.has('items/health_potion.mooditem', 6)");
    CHECK(fx->lua["r3"] == true);
    CHECK(fx->lua["r5"] == true);
    CHECK(fx->lua["r6"] == false);
}

TEST_CASE("inventory.count: explicito + player-implicit") {
    auto fx = makeFixture("count_basic");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->potionId, 7, *fx->am);

    fx->lua["target"] = fx->player;
    fx->lua.safe_script(
        "c_explicit = inventory.count(target, 'items/health_potion.mooditem')");
    CHECK(fx->lua["c_explicit"] == 7);

    fx->lua.safe_script(
        "c_implicit = inventory.count('items/health_potion.mooditem')");
    CHECK(fx->lua["c_implicit"] == 7);

    // Item que no tiene -> 0.
    fx->lua.safe_script("c_zero = inventory.count('items/iron_sword.mooditem')");
    CHECK(fx->lua["c_zero"] == 0);
}

TEST_CASE("inventory.entries: devuelve tabla con item_path + qty + slot_index") {
    auto fx = makeFixture("entries_basic");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->swordId, 1, *fx->am);
    inv.state.add(fx->potionId, 3, *fx->am);

    fx->lua.safe_script(R"(
        rows = inventory.entries()
        n = #rows
        p1 = rows[1].item_path
        q1 = rows[1].qty
        p2 = rows[2].item_path
        q2 = rows[2].qty
    )");
    CHECK(fx->lua["n"] == 2);
    CHECK(fx->lua["p1"].get<std::string>() == "items/iron_sword.mooditem");
    CHECK(fx->lua["q1"] == 1);
    CHECK(fx->lua["p2"].get<std::string>() == "items/health_potion.mooditem");
    CHECK(fx->lua["q2"] == 3);
}

TEST_CASE("inventory.sum_stat: suma stat * quantity por entry") {
    auto fx = makeFixture("sum_stat");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->swordId, 1, *fx->am);     // weight=3.0
    inv.state.add(fx->potionId, 5, *fx->am);    // weight=0.5 * 5 = 2.5

    fx->lua.safe_script("w = inventory.sum_stat('weight')");
    CHECK(fx->lua["w"].get<float>() == doctest::Approx(5.5f));

    fx->lua.safe_script("dmg = inventory.sum_stat('damage')");
    CHECK(fx->lua["dmg"].get<float>() == doctest::Approx(10.0f));

    // Stat inexistente -> 0.
    fx->lua.safe_script("none = inventory.sum_stat('foo')");
    CHECK(fx->lua["none"].get<float>() == doctest::Approx(0.0f));
}

// ============================================================
// Mutaciones — add / remove
// ============================================================

TEST_CASE("inventory.add: aumenta el inventory + retorna bool") {
    auto fx = makeFixture("add_basic");
    auto& inv = fx->player.getComponent<InventoryComponent>();

    fx->lua.safe_script("r = inventory.add('items/health_potion.mooditem', 3)");
    CHECK(fx->lua["r"] == true);
    CHECK(inv.state.count(fx->potionId) == 3);

    // Explicito con entity.
    fx->lua["target"] = fx->player;
    fx->lua.safe_script("r2 = inventory.add(target, 'items/iron_sword.mooditem', 1)");
    CHECK(fx->lua["r2"] == true);
    CHECK(inv.state.count(fx->swordId) == 1);
}

TEST_CASE("inventory.remove: substrae el qty + falla si no hay suficiente") {
    auto fx = makeFixture("remove_basic");
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.add(fx->potionId, 5, *fx->am);

    fx->lua.safe_script("r = inventory.remove('items/health_potion.mooditem', 2)");
    CHECK(fx->lua["r"] == true);
    CHECK(inv.state.count(fx->potionId) == 3);

    // Pedir mas del que hay -> fail atomico, no muta.
    fx->lua.safe_script("r2 = inventory.remove('items/health_potion.mooditem', 10)");
    CHECK(fx->lua["r2"] == false);
    CHECK(inv.state.count(fx->potionId) == 3);
}

// ============================================================
// Spawn pickup
// ============================================================

TEST_CASE("inventory.spawn_pickup: crea entity con todos los componentes") {
    auto fx = makeFixture("spawn_basic");
    const auto preCount = fx->scene.entityCount();

    fx->lua.safe_script(
        "r = inventory.spawn_pickup('items/iron_sword.mooditem', 2.0, 0.5, 3.0)");
    CHECK(fx->lua["r"] == true);
    CHECK(fx->scene.entityCount() == preCount + 1u);

    // Verificar componentes del entity nuevo (buscando por Pickup en el tag).
    bool found = false;
    fx->scene.forEach<TagComponent, TransformComponent, MeshRendererComponent,
                       TriggerComponent, ItemPickupComponent>(
        [&](Entity e, TagComponent& tag, TransformComponent& t,
            MeshRendererComponent&, TriggerComponent& trig,
            ItemPickupComponent& ip) {
            if (tag.name.find("Pickup_") == 0) {
                found = true;
                CHECK(t.position.x == doctest::Approx(2.0f));
                CHECK(t.position.y == doctest::Approx(0.5f));
                CHECK(t.position.z == doctest::Approx(3.0f));
                CHECK(trig.halfExtents.x == doctest::Approx(0.5f));
                CHECK(ip.itemPath == "items/iron_sword.mooditem");
                CHECK(ip.quantity == 1);
                CHECK(ip.destroyOnPickup == true);
            }
        });
    CHECK(found);
}

TEST_CASE("inventory.spawn_pickup: quantity custom") {
    auto fx = makeFixture("spawn_qty");
    fx->lua.safe_script(
        "inventory.spawn_pickup('items/health_potion.mooditem', 0.0, 0.0, 0.0, 5)");
    bool found = false;
    fx->scene.forEach<ItemPickupComponent>([&](Entity, ItemPickupComponent& ip) {
        if (ip.itemPath == "items/health_potion.mooditem") {
            found = true;
            CHECK(ip.quantity == 5);
        }
    });
    CHECK(found);
}

TEST_CASE("inventory.spawn_pickup: path invalido -> retorna false sin crear nada") {
    auto fx = makeFixture("spawn_invalid");
    const auto preCount = fx->scene.entityCount();
    fx->lua.safe_script(
        "r = inventory.spawn_pickup('items/inexistente.mooditem', 0, 0, 0)");
    CHECK(fx->lua["r"] == false);
    CHECK(fx->scene.entityCount() == preCount);
}

// ============================================================
// Defensiva — no crash sin scene / sin player
// ============================================================

TEST_CASE("inventory.has: sin player en scene -> false + log warn (no crash)") {
    auto fx = makeFixture("no_player");
    fx->scene.destroyEntity(fx->player);

    fx->lua.safe_script("r = inventory.has('items/iron_sword.mooditem')");
    CHECK(fx->lua["r"] == false);
}

TEST_CASE("inventory.add: sin player en scene -> false (no crash)") {
    auto fx = makeFixture("no_player_add");
    fx->scene.destroyEntity(fx->player);

    fx->lua.safe_script("r = inventory.add('items/iron_sword.mooditem', 1)");
    CHECK(fx->lua["r"] == false);
}

TEST_CASE("inventory.count: item path inexistente -> 0") {
    auto fx = makeFixture("count_invalid");
    fx->lua.safe_script("c = inventory.count('items/inexistente.mooditem')");
    CHECK(fx->lua["c"] == 0);
}
