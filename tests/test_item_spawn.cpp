// Tests de `Inventory::spawnPickupInWorld` (F2H52 H5). El helper que
// crea entity-pickups en el mundo y es compartido entre:
//   - `inventory.spawn_pickup(...)` Lua binding (Bloque E).
//   - HUD "Tirar" del context menu del inventory_panel (Bloque H5).
//
// Cubre:
//   - Creacion exitosa: entity con Transform + MeshRenderer + Trigger
//     + ItemPickup en la posicion + cantidad indicadas.
//   - Defaults: quantity sin pasar -> 1.
//   - Edge cases: scene null, assets null, item inexistente -> false,
//     no se crea nada.
//   - itemPath se persiste tal cual en ItemPickupComponent (para que
//     el flow de levantar resuelva el mismo item).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/inventory/ItemSpawn.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

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

/// Setup: AssetManager + Scene + un item de prueba en disco. El test
/// puede llamar spawnPickupInWorld directamente.
struct SpawnFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    Scene scene;
};

std::unique_ptr<SpawnFixture> makeFixture(const std::string& tag) {
    namespace fs = std::filesystem;
    auto fx = std::make_unique<SpawnFixture>();
    fx->tmpRoot = fs::temp_directory_path() / ("mood_spawn_" + tag);
    fs::remove_all(fx->tmpRoot);
    fs::create_directories(fx->tmpRoot / "items");

    Inventory::Asset potion;
    potion.id = "health_potion";
    potion.name_literal = "Health Potion";
    potion.tags = {"consumable"};
    potion.stack.stackable = true;
    potion.stack.max_stack = 99;
    potion.saveToFile(fx->tmpRoot / "items" / "health_potion.mooditem");

    fx->am = std::make_unique<AssetManager>(fx->tmpRoot.string(),
                                              dummyTexFactory());
    return fx;
}

} // namespace

TEST_CASE("ItemSpawn: spawn item valido -> entity con Transform+MeshRenderer+Trigger+ItemPickup") {
    auto fx = makeFixture("basic_spawn");
    const size_t before = fx->scene.entityCount();

    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, fx->am.get(),
        "items/health_potion.mooditem",
        glm::vec3(3.5f, 2.0f, -1.0f),
        1);

    CHECK(ok);
    CHECK(fx->scene.entityCount() == before + 1u);

    // Encontrar la entity recien creada (la unica con ItemPickupComponent).
    Entity spawned;
    fx->scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent&) {
        spawned = e;
    });
    REQUIRE(spawned);

    CHECK(spawned.hasComponent<TransformComponent>());
    CHECK(spawned.hasComponent<MeshRendererComponent>());
    CHECK(spawned.hasComponent<TriggerComponent>());
    CHECK(spawned.hasComponent<ItemPickupComponent>());

    const auto& t = spawned.getComponent<TransformComponent>();
    CHECK(t.position.x == doctest::Approx(3.5f));
    CHECK(t.position.y == doctest::Approx(2.0f));
    CHECK(t.position.z == doctest::Approx(-1.0f));

    const auto& ip = spawned.getComponent<ItemPickupComponent>();
    CHECK(ip.itemPath == "items/health_potion.mooditem");
    CHECK(ip.quantity == 1);
}

TEST_CASE("ItemSpawn: quantity custom se persiste en ItemPickupComponent") {
    auto fx = makeFixture("qty_spawn");
    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, fx->am.get(),
        "items/health_potion.mooditem",
        glm::vec3(0.0f),
        7);
    REQUIRE(ok);

    Entity spawned;
    fx->scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent&) {
        spawned = e;
    });
    REQUIRE(spawned);
    CHECK(spawned.getComponent<ItemPickupComponent>().quantity == 7);
}

TEST_CASE("ItemSpawn: scene null -> false sin crash") {
    auto fx = makeFixture("null_scene");
    const bool ok = Inventory::spawnPickupInWorld(
        nullptr, fx->am.get(),
        "items/health_potion.mooditem",
        glm::vec3(0.0f),
        1);
    CHECK(!ok);
}

TEST_CASE("ItemSpawn: assets null -> false sin crash") {
    auto fx = makeFixture("null_assets");
    const size_t before = fx->scene.entityCount();
    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, nullptr,
        "items/health_potion.mooditem",
        glm::vec3(0.0f),
        1);
    CHECK(!ok);
    CHECK(fx->scene.entityCount() == before);
}

TEST_CASE("ItemSpawn: item inexistente -> false, no entity creada") {
    auto fx = makeFixture("bad_item");
    const size_t before = fx->scene.entityCount();
    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, fx->am.get(),
        "items/does_not_exist.mooditem",
        glm::vec3(0.0f),
        1);
    CHECK(!ok);
    CHECK(fx->scene.entityCount() == before);
}

TEST_CASE("ItemSpawn: quantity 0 o negativo -> clamp a 1") {
    auto fx = makeFixture("clamp_qty");
    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, fx->am.get(),
        "items/health_potion.mooditem",
        glm::vec3(0.0f),
        0);
    REQUIRE(ok);

    Entity spawned;
    fx->scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent&) {
        spawned = e;
    });
    REQUIRE(spawned);
    CHECK(spawned.getComponent<ItemPickupComponent>().quantity == 1);
}

TEST_CASE("ItemSpawn: Trigger del pickup tiene halfExtents 0.5 (caja 1m³)") {
    auto fx = makeFixture("trigger_size");
    const bool ok = Inventory::spawnPickupInWorld(
        &fx->scene, fx->am.get(),
        "items/health_potion.mooditem",
        glm::vec3(0.0f),
        1);
    REQUIRE(ok);

    Entity spawned;
    fx->scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent&) {
        spawned = e;
    });
    REQUIRE(spawned);
    const auto& trig = spawned.getComponent<TriggerComponent>();
    CHECK(trig.halfExtents.x == doctest::Approx(0.5f));
    CHECK(trig.halfExtents.y == doctest::Approx(0.5f));
    CHECK(trig.halfExtents.z == doctest::Approx(0.5f));
}

TEST_CASE("ItemSpawn: multiples spawns producen entities independientes") {
    auto fx = makeFixture("multi_spawn");
    REQUIRE(Inventory::spawnPickupInWorld(&fx->scene, fx->am.get(),
        "items/health_potion.mooditem", glm::vec3(1, 0, 0), 1));
    REQUIRE(Inventory::spawnPickupInWorld(&fx->scene, fx->am.get(),
        "items/health_potion.mooditem", glm::vec3(2, 0, 0), 2));
    REQUIRE(Inventory::spawnPickupInWorld(&fx->scene, fx->am.get(),
        "items/health_potion.mooditem", glm::vec3(3, 0, 0), 3));

    int n = 0;
    fx->scene.forEach<ItemPickupComponent>([&](Entity, ItemPickupComponent&) {
        ++n;
    });
    CHECK(n == 3);
}
