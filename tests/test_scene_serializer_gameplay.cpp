// Tests de SceneSerializer — split AUDIT-3 (familia: gameplay components).
// DialogComponent (F2H48), ItemPickupComponent (F2H52),
// AnimatorComponent (F2H50), InventoryComponent (F2H51).
// Comparte helpers con test_scene_serializer.cpp via
// test_scene_serializer_helpers.h.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"
#include "test_scene_serializer_helpers.h"

#include <filesystem>
#include <utility>
#include <vector>

using namespace Mood;
using Mood::SceneSerializerTests::nullFactory;
using Mood::SceneSerializerTests::tempPath;

// ============================================================
// F2H48.1: DialogComponent round-trip
// ============================================================

TEST_CASE("SceneSerializer: round-trip de DialogComponent (F2H48.1)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_guard");
        DialogComponent dc{};
        dc.dialogPath           = "dialogs/intro.mooddialog";
        dc.autoStartOnInteract  = false;
        dc.cachedDialogId       = 99;
        npc.addComponent<DialogComponent>(dc);
        npc.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    CHECK(se.tag == "NPC_guard");
    REQUIRE(se.dialog.has_value());
    CHECK(se.dialog->dialogPath == "dialogs/intro.mooddialog");
    CHECK_FALSE(se.dialog->autoStartOnInteract);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: DialogComponent con path vacio NO se persiste (F2H48.1)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_sin_path");
        npc.addComponent<DialogComponent>();
        npc.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_empty_path.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    CHECK_FALSE(loaded->entities[0].dialog.has_value());

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: autoStartOnInteract default=true se persiste igual (F2H48.1)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_default");
        DialogComponent dc{};
        dc.dialogPath = "dialogs/x.mooddialog";
        npc.addComponent<DialogComponent>(dc);
        npc.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
    }
    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_default_flag.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities[0].dialog.has_value());
    CHECK(loaded->entities[0].dialog->autoStartOnInteract);
    std::filesystem::remove(path);
}

// ============================================================
// F2H52: ItemPickupComponent round-trip
// ============================================================

TEST_CASE("SceneSerializer: round-trip de ItemPickupComponent (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("HealthPotion_drop");
        ItemPickupComponent ip{};
        ip.itemPath        = "items/health_potion.mooditem";
        ip.quantity        = 3;
        ip.destroyOnPickup = false;
        ip.cachedItemId    = 42;
        pickup.addComponent<ItemPickupComponent>(ip);
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    CHECK(se.tag == "HealthPotion_drop");
    REQUIRE(se.itemPickup.has_value());
    CHECK(se.itemPickup->itemPath == "items/health_potion.mooditem");
    CHECK(se.itemPickup->quantity == 3);
    CHECK_FALSE(se.itemPickup->destroyOnPickup);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: ItemPickupComponent vacio NO se serializa (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("EmptyPickup");
        ItemPickupComponent ip{};
        pickup.addComponent<ItemPickupComponent>(ip);
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_empty.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    CHECK_FALSE(loaded->entities[0].itemPickup.has_value());

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: ItemPickupComponent defaults sensatos al reload (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("MinimalPickup");
        ItemPickupComponent ip{};
        ip.itemPath = "items/key.mooditem";
        pickup.addComponent<ItemPickupComponent>(ip);
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_defaults.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities[0].itemPickup.has_value());
    CHECK(loaded->entities[0].itemPickup->itemPath == "items/key.mooditem");
    CHECK(loaded->entities[0].itemPickup->quantity == 1);
    CHECK(loaded->entities[0].itemPickup->destroyOnPickup == true);

    std::filesystem::remove(path);
}

// ============================================================
// F2H50: AnimatorComponent persistence
// ============================================================

TEST_CASE("SceneSerializer: round-trip de AnimatorComponent (F2H50)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_anim");
        npc.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
        AnimatorComponent anim{};
        anim.clipName = "walk";
        anim.speed    = 1.5f;
        anim.playing  = false;
        anim.loop     = false;
        anim.externalClips.emplace_back(
            "idle", assets.loadAnimationClip("characters/player/anim_idle.fbx"));
        anim.externalClips.emplace_back(
            "walk", assets.loadAnimationClip("characters/player/anim_walk.fbx"));
        anim.externalClips.emplace_back(
            "wave", assets.loadAnimationClip("characters/player/anim_wave.fbx"));
        npc.addComponent<AnimatorComponent>(std::move(anim));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("animator_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.animator.has_value());
    CHECK(se.animator->clipName == "walk");
    CHECK(se.animator->speed == doctest::Approx(1.5f));
    CHECK_FALSE(se.animator->playing);
    CHECK_FALSE(se.animator->loop);
    REQUIRE(se.animator->externalClips.size() == 3u);
    CHECK(se.animator->externalClips[0].alias == "idle");
    CHECK(se.animator->externalClips[0].path == "characters/player/anim_idle.fbx");
    CHECK(se.animator->externalClips[1].alias == "walk");
    CHECK(se.animator->externalClips[1].path == "characters/player/anim_walk.fbx");
    CHECK(se.animator->externalClips[2].alias == "wave");
    CHECK(se.animator->externalClips[2].path == "characters/player/anim_wave.fbx");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: AnimatorComponent sin externalClips se persiste igual (F2H50)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity e = scene.createEntity("Anim_no_clips");
        e.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
        AnimatorComponent anim{};
        anim.clipName = "embedded_clip";
        anim.playing  = true;
        anim.loop     = true;
        e.addComponent<AnimatorComponent>(std::move(anim));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("animator_no_external_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.animator.has_value());
    CHECK(se.animator->clipName == "embedded_clip");
    CHECK(se.animator->externalClips.empty());

    std::filesystem::remove(path);
}

// ============================================================
// F2H51: InventoryComponent persistence (3 layout modes)
// ============================================================

TEST_CASE("SceneSerializer: round-trip de InventoryComponent FlatList (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword  = assets.loadItem("items/iron_sword.mooditem");
    const ItemAssetId potion = assets.loadItem("items/health_potion.mooditem");
    REQUIRE(sword != assets.missingItemId());
    REQUIRE(potion != assets.missingItemId());

    Scene scene;
    {
        Entity player = scene.createEntity("Player_inv");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::FlatList;
        inv.state.config.max_items = 24;
        REQUIRE(inv.state.add(sword, 1, assets));
        REQUIRE(inv.state.add(potion, 5, assets));
        player.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_flatlist_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "flat_list");
    CHECK(se.inventory->maxItems == 24);
    REQUIRE(se.inventory->entries.size() == 2u);
    CHECK(se.inventory->entries[0].itemPath == "items/iron_sword.mooditem");
    CHECK(se.inventory->entries[0].quantity == 1);
    CHECK(se.inventory->entries[1].itemPath == "items/health_potion.mooditem");
    CHECK(se.inventory->entries[1].quantity == 5);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de InventoryComponent Grid2D (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword = assets.loadItem("items/iron_sword.mooditem");

    Scene scene;
    {
        Entity chest = scene.createEntity("Chest_inv");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::Grid2D;
        inv.state.config.grid_width  = 3;
        inv.state.config.grid_height = 5;
        REQUIRE(inv.state.placeAt(sword, 1, 7, assets));
        chest.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_grid_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "grid_2d");
    CHECK(se.inventory->gridWidth == 3);
    CHECK(se.inventory->gridHeight == 5);
    REQUIRE(se.inventory->entries.size() == 1u);
    CHECK(se.inventory->entries[0].itemPath == "items/iron_sword.mooditem");
    CHECK(se.inventory->entries[0].slotIndex == 7);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de InventoryComponent EquipmentSlots (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword = assets.loadItem("items/iron_sword.mooditem");

    Scene scene;
    {
        Entity hero = scene.createEntity("Hero_equip");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::EquipmentSlots;
        inv.state.config.equipment_slots = {
            {"weapon_main", "weapon"},
            {"head",        "armor"},
            {"any",         ""},
        };
        REQUIRE(inv.state.placeAt(sword, 1, 0, assets));
        hero.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_equip_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "equipment_slots");
    REQUIRE(se.inventory->equipmentSlots.size() == 3u);
    CHECK(se.inventory->equipmentSlots[0].name == "weapon_main");
    CHECK(se.inventory->equipmentSlots[0].tagFilter == "weapon");
    CHECK(se.inventory->equipmentSlots[2].tagFilter.empty());
    REQUIRE(se.inventory->entries.size() == 1u);
    CHECK(se.inventory->entries[0].slotIndex == 0);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: entidad sin InventoryComponent no persiste el campo (F2H51)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    {
        Entity e = scene.createEntity("plain");
        e.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
    }
    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_absent.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->entities[0].inventory.has_value());
    std::filesystem::remove(path);
}
