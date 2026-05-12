// Tests del ItemPickupSystem (F2H52 Bloque C). Cubre el flow:
//   - Player en trigger + E -> add a InventoryComponent + destroy entity
//     + push pickup notif al HUD + invoca hook on_pickup.
//   - destroyOnPickup=false: pickup queda en el mundo (item infinito).
//   - Inventario lleno -> add falla -> no destruye, no notif, no hook.
//   - Sin player en escena -> no-op (no crash).
//   - Dialog activo -> sistema skipea todo (prioridad al dialog).
//   - Multiples pickups en un trigger compartido (no en v1 — un trigger
//     una pickup component).
//   - Prompt del HUD: aparece cuando player adentro, desaparece cuando
//     sale o cuando ya levanto el item.
//
// El sistema NO necesita SDL ni window — el flanco de E es un bool que
// el test pasa explicitamente. Mismo patron que test_dialog_system.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/game/state/GameState.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/inventory/ItemPickupSystem.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace Mood;
namespace Pickup = Mood::Inventory::ItemPickupSystem;

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

/// Setup minimo: una scene con un player (tag "Player" + Inventory
/// FlatList max=20) y un pickup (Trigger + ItemPickup apuntando a un
/// item creado en disco). El test puede mover playerInside del trigger
/// a true/false manualmente y disparar tick() con eJustPressed.
struct PickupFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
    Scene scene;
    Entity player;
    Entity pickup;
    ItemAssetId swordId = 0;
};

std::unique_ptr<PickupFixture> makeFixture(
    const std::string& tag,
    bool stackable = false,
    int max_stack = 1) {
    namespace fs = std::filesystem;
    auto fx = std::make_unique<PickupFixture>();
    fx->tmpRoot = fs::temp_directory_path() / ("mood_pickup_" + tag);
    fs::remove_all(fx->tmpRoot);
    fs::create_directories(fx->tmpRoot / "items");

    Inventory::Asset sword;
    sword.id = "iron_sword";
    sword.name_literal = "Iron Sword";
    sword.tags = {"weapon"};
    sword.stack.stackable = stackable;
    sword.stack.max_stack = max_stack;
    sword.saveToFile(fx->tmpRoot / "items" / "iron_sword.mooditem");

    fx->am = std::make_unique<AssetManager>(fx->tmpRoot.string(), dummyTexFactory());
    fx->swordId = fx->am->loadItem("items/iron_sword.mooditem");
    REQUIRE(fx->swordId != fx->am->missingItemId());

    fx->player = fx->scene.createEntity("Player");
    auto& tagComp = fx->player.getComponent<TagComponent>();
    tagComp.name = "Player";
    fx->player.addComponent<InventoryComponent>();

    fx->pickup = fx->scene.createEntity("sword_in_world");
    fx->pickup.addComponent<TriggerComponent>();
    ItemPickupComponent ip;
    ip.itemPath = "items/iron_sword.mooditem";
    ip.quantity = 1;
    ip.destroyOnPickup = true;
    fx->pickup.addComponent<ItemPickupComponent>(ip);

    // Estado inicial: reset el cache del sistema + HUD + hooks para
    // evitar contaminacion entre tests.
    Pickup::resetPlayerCache();
    Pickup::clearHooks();
    GameState::reset();
    return fx;
}

} // namespace

// ============================================================
// Flow basico: player + E + add + destroy + hook + notif
// ============================================================

TEST_CASE("ItemPickupSystem: player en trigger + E -> add + destroy + notif + hook") {
    auto fx = makeFixture("basic_pickup");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& inv = fx->player.getComponent<InventoryComponent>();

    // Frame 1: player entra al trigger pero NO presiona E.
    // El prompt queda visible (contenido exacto depende de I18n init —
    // en tests I18n puede no estar cargado y T() devuelve la key; lo
    // que importa es que algun prompt este seteado).
    tr.playerInside = true;
    const bool any1 = Pickup::tick(fx->scene, *fx->am,
                                   /*eJustPressed=*/false,
                                   /*dialogActive=*/false);
    CHECK(!any1);
    CHECK(!GameState::hud().interact_prompt.empty());
    CHECK(inv.state.entries.empty());
    CHECK(fx->scene.entityCount() == 2u);

    // Frame 2: player presiona E -> pickup.
    int hookCalls = 0;
    std::string hookPath;
    int hookQty = 0;
    Pickup::setPickupHook([&](const std::string& p, int q) {
        ++hookCalls;
        hookPath = p;
        hookQty = q;
    });
    const bool any2 = Pickup::tick(fx->scene, *fx->am,
                                   /*eJustPressed=*/true,
                                   /*dialogActive=*/false);
    CHECK(any2);
    CHECK(inv.state.count(fx->swordId) == 1);
    CHECK(GameState::hud().interact_prompt.empty());
    CHECK(GameState::hud().pickup_queue.size() == 1u);
    CHECK(hookCalls == 1);
    CHECK(hookPath == "items/iron_sword.mooditem");
    CHECK(hookQty == 1);
    CHECK(fx->scene.entityCount() == 1u);  // player; pickup destruido
}

// ============================================================
// destroyOnPickup=false: dispenser infinito
// ============================================================

TEST_CASE("ItemPickupSystem: destroyOnPickup=false deja la entity en el mundo") {
    auto fx = makeFixture("infinite_pickup", /*stackable=*/true, /*max=*/10);
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& ip = fx->pickup.getComponent<ItemPickupComponent>();
    ip.destroyOnPickup = false;
    ip.quantity = 1;
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = true;
    Pickup::tick(fx->scene, *fx->am, /*eJustPressed=*/true, false);
    Pickup::tick(fx->scene, *fx->am, /*eJustPressed=*/true, false);
    Pickup::tick(fx->scene, *fx->am, /*eJustPressed=*/true, false);

    CHECK(inv.state.count(fx->swordId) == 3);
    CHECK(fx->scene.entityCount() == 2u);  // pickup sigue ahi
}

// ============================================================
// Inventario lleno: add falla -> no destroy, no notif
// ============================================================

TEST_CASE("ItemPickupSystem: add fail (capacity) no destruye ni notifica") {
    auto fx = makeFixture("inv_full");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& inv = fx->player.getComponent<InventoryComponent>();
    inv.state.config.max_items = 0;  // FlatList sin capacity

    tr.playerInside = true;
    int hookCalls = 0;
    Pickup::setPickupHook([&](const std::string&, int) { ++hookCalls; });
    const bool any = Pickup::tick(fx->scene, *fx->am,
                                   /*eJustPressed=*/true, false);
    CHECK(!any);
    CHECK(inv.state.entries.empty());
    CHECK(GameState::hud().pickup_queue.empty());
    CHECK(hookCalls == 0);
    CHECK(fx->scene.entityCount() == 2u);
    // Prompt sigue visible (player puede tirar algo y reintentar).
    CHECK(!GameState::hud().interact_prompt.empty());
}

// ============================================================
// Player fuera del trigger: prompt limpio, no pickup
// ============================================================

TEST_CASE("ItemPickupSystem: sin playerInside no hay prompt ni pickup") {
    auto fx = makeFixture("player_outside");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = false;
    Pickup::tick(fx->scene, *fx->am, /*eJustPressed=*/true, false);
    CHECK(inv.state.entries.empty());
    CHECK(GameState::hud().interact_prompt.empty());
    CHECK(fx->scene.entityCount() == 2u);
}

TEST_CASE("ItemPickupSystem: salir del trigger limpia el prompt seteado") {
    auto fx = makeFixture("prompt_clear_on_exit");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();

    tr.playerInside = true;
    Pickup::tick(fx->scene, *fx->am, false, false);
    CHECK(!GameState::hud().interact_prompt.empty());

    tr.playerInside = false;
    Pickup::tick(fx->scene, *fx->am, false, false);
    CHECK(GameState::hud().interact_prompt.empty());
}

// ============================================================
// Sin player en escena: defensive no-op
// ============================================================

TEST_CASE("ItemPickupSystem: sin player en escena no crashea") {
    auto fx = makeFixture("no_player");
    fx->scene.destroyEntity(fx->player);
    Pickup::resetPlayerCache();
    auto& tr = fx->pickup.getComponent<TriggerComponent>();

    tr.playerInside = true;
    // No tira excepcion ni asserta.
    const bool any = Pickup::tick(fx->scene, *fx->am, true, false);
    CHECK(!any);
    CHECK(fx->scene.entityCount() == 1u);  // pickup sigue
}

// ============================================================
// Dialog activo: sistema skipea todo (E le pertenece al dialog)
// ============================================================

TEST_CASE("ItemPickupSystem: dialog activo no consume E ni muestra prompt") {
    auto fx = makeFixture("dialog_priority");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = true;
    const bool any = Pickup::tick(fx->scene, *fx->am,
                                   /*eJustPressed=*/true,
                                   /*dialogActive=*/true);
    CHECK(!any);
    CHECK(inv.state.entries.empty());
    CHECK(GameState::hud().interact_prompt.empty());
    CHECK(fx->scene.entityCount() == 2u);
}

// ============================================================
// ItemPickupComponent con itemPath vacio: ignorado
// ============================================================

TEST_CASE("ItemPickupSystem: itemPath vacio es ignorado (defensive)") {
    auto fx = makeFixture("empty_path");
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& ip = fx->pickup.getComponent<ItemPickupComponent>();
    ip.itemPath.clear();
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = true;
    Pickup::tick(fx->scene, *fx->am, true, false);
    CHECK(inv.state.entries.empty());
    CHECK(GameState::hud().interact_prompt.empty());
    CHECK(fx->scene.entityCount() == 2u);  // no se destruye
}

// ============================================================
// Quantity custom: pickup de N unidades en un add
// ============================================================

TEST_CASE("ItemPickupSystem: quantity custom respeta stackable del item") {
    auto fx = makeFixture("custom_qty", /*stackable=*/true, /*max=*/10);
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& ip = fx->pickup.getComponent<ItemPickupComponent>();
    ip.quantity = 5;
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = true;
    Pickup::tick(fx->scene, *fx->am, true, false);
    CHECK(inv.state.count(fx->swordId) == 5);
}

// ============================================================
// Hook no registrado: pickup funciona igual (engine-grade)
// ============================================================

TEST_CASE("ItemPickupSystem: sin hook registrado el pickup sigue funcionando") {
    auto fx = makeFixture("no_hook");
    Pickup::clearHooks();
    auto& tr = fx->pickup.getComponent<TriggerComponent>();
    auto& inv = fx->player.getComponent<InventoryComponent>();

    tr.playerInside = true;
    const bool any = Pickup::tick(fx->scene, *fx->am, true, false);
    CHECK(any);
    CHECK(inv.state.count(fx->swordId) == 1);
}
