// Tests del InventoryState (F2H51 Bloque D). Cubre los 3 layout modes
// (FlatList / Grid2D / EquipmentSlots) sobre add/remove/has/count/
// placeAt/moveSlot/clear. Usa un AssetManager con factory mock + items
// creados en /tmp para que loadItem resuelva contra el VFS.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/InventoryState.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/render/rhi/ITexture.h"

#include <filesystem>

using namespace Mood;
using namespace Mood::Inventory;

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

/// Helper: prepara un directorio temporal con N .mooditem y devuelve un
/// AssetManager rooted ahi. Los items se generan con configuracion
/// custom (id, tags, stackable, max_stack).
struct ItemSpec {
    std::string id;
    std::vector<std::string> tags;
    bool stackable = false;
    int max_stack = 1;
};

/// @returns directorio tmp + AssetManager apuntando ahi. Cada item se
/// guarda en `<tmp>/items/<id>.mooditem`. El llamador puede usar
/// `am.loadItem("items/<id>.mooditem")` para obtener el ItemAssetId.
struct InvFixture {
    std::filesystem::path tmpRoot;
    std::unique_ptr<AssetManager> am;
};

InvFixture makeFixture(const std::vector<ItemSpec>& specs, const std::string& tag) {
    namespace fs = std::filesystem;
    InvFixture fx;
    fx.tmpRoot = fs::temp_directory_path() / ("mood_inv_" + tag);
    fs::remove_all(fx.tmpRoot);
    fs::create_directories(fx.tmpRoot / "items");
    for (const ItemSpec& s : specs) {
        Asset a;
        a.id = s.id;
        a.tags = s.tags;
        a.stack.stackable = s.stackable;
        a.stack.max_stack = s.max_stack;
        a.saveToFile(fx.tmpRoot / "items" / (s.id + ".mooditem"));
    }
    fx.am = std::make_unique<AssetManager>(fx.tmpRoot.string(), dummyTexFactory());
    return fx;
}

} // namespace

// ============================================================
// Lecturas puras: has / count / slotCapacity
// ============================================================

TEST_CASE("InventoryState: estado default es FlatList vacio con capacity 20") {
    State s;
    CHECK(s.mode == LayoutMode::FlatList);
    CHECK(s.entries.empty());
    CHECK(s.slotCapacity() == 20);
}

TEST_CASE("InventoryState: slotCapacity depende del mode") {
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 4;
    s.config.grid_height = 6;
    CHECK(s.slotCapacity() == 24);

    s.mode = LayoutMode::EquipmentSlots;
    s.config.equipment_slots = {{"head", "armor"}, {"weapon", ""}};
    CHECK(s.slotCapacity() == 2);
}

TEST_CASE("InventoryState: has/count sobre entries vacios") {
    State s;
    CHECK(s.count(1) == 0);
    CHECK_FALSE(s.has(1));
    CHECK_FALSE(s.has(1, 5));
}

// ============================================================
// add — FlatList
// ============================================================

TEST_CASE("InventoryState: FlatList add non-stackable crea entry por unidad") {
    auto fx = makeFixture({{"sword", {"weapon"}, false, 1}}, "flat_nonstack");
    const ItemAssetId sword = fx.am->loadItem("items/sword.mooditem");

    State s;
    s.mode = LayoutMode::FlatList;
    s.config.max_items = 10;
    REQUIRE(s.add(sword, 3, *fx.am));
    CHECK(s.entries.size() == 3);
    CHECK(s.count(sword) == 3);
    for (const Entry& e : s.entries) {
        CHECK(e.itemId == sword);
        CHECK(e.quantity == 1);
    }
}

TEST_CASE("InventoryState: FlatList add stackable respeta max_stack y crea entries adicionales") {
    auto fx = makeFixture({{"potion", {"consumable"}, true, 10}}, "flat_stack");
    const ItemAssetId potion = fx.am->loadItem("items/potion.mooditem");

    State s;
    s.mode = LayoutMode::FlatList;
    s.config.max_items = 5;
    // Agregar 25: max_stack=10 -> 10 + 10 + 5 en 3 entries.
    REQUIRE(s.add(potion, 25, *fx.am));
    REQUIRE(s.entries.size() == 3);
    CHECK(s.entries[0].quantity == 10);
    CHECK(s.entries[1].quantity == 10);
    CHECK(s.entries[2].quantity == 5);
    CHECK(s.count(potion) == 25);
}

TEST_CASE("InventoryState: FlatList add stackable llena primero entries existentes con espacio") {
    auto fx = makeFixture({{"potion", {}, true, 10}}, "flat_fill");
    const ItemAssetId potion = fx.am->loadItem("items/potion.mooditem");

    State s;
    s.config.max_items = 3;
    REQUIRE(s.add(potion, 7, *fx.am));   // 1 entry con quantity=7
    REQUIRE(s.entries.size() == 1);
    REQUIRE(s.add(potion, 5, *fx.am));   // llena hasta 10 + nuevo entry con 2
    REQUIRE(s.entries.size() == 2);
    CHECK(s.entries[0].quantity == 10);
    CHECK(s.entries[1].quantity == 2);
}

TEST_CASE("InventoryState: FlatList add rechaza atomicamente cuando excede max_items") {
    auto fx = makeFixture({{"sword", {}, false, 1}}, "flat_overflow");
    const ItemAssetId sword = fx.am->loadItem("items/sword.mooditem");

    State s;
    s.config.max_items = 3;
    REQUIRE(s.add(sword, 3, *fx.am));   // ocupa los 3 entries
    CHECK(s.entries.size() == 3);
    CHECK_FALSE(s.add(sword, 1, *fx.am)); // 4to no entra
    CHECK(s.entries.size() == 3);        // sin mutar (atomic)
}

TEST_CASE("InventoryState: add con qty<=0 o id 0 rechaza sin mutar") {
    auto fx = makeFixture({{"x", {}, false, 1}}, "guards");
    const ItemAssetId x = fx.am->loadItem("items/x.mooditem");
    State s;
    CHECK_FALSE(s.add(x, 0, *fx.am));
    CHECK_FALSE(s.add(x, -5, *fx.am));
    CHECK_FALSE(s.add(0, 3, *fx.am));   // id 0 = item vacio
    CHECK(s.entries.empty());
}

// ============================================================
// remove (atomic)
// ============================================================

TEST_CASE("InventoryState: remove substrae quantity y borra entries que llegan a 0") {
    auto fx = makeFixture({{"potion", {}, true, 10}}, "remove_basic");
    const ItemAssetId potion = fx.am->loadItem("items/potion.mooditem");
    State s;
    REQUIRE(s.add(potion, 15, *fx.am));  // 10 + 5
    REQUIRE(s.entries.size() == 2);
    REQUIRE(s.remove(potion, 12));
    CHECK(s.count(potion) == 3);
    // Primer entry pasa de 10 a 0 (borrado), segundo de 5 a 3.
    REQUIRE(s.entries.size() == 1);
    CHECK(s.entries[0].quantity == 3);
}

TEST_CASE("InventoryState: remove con qty mayor al total rechaza atomicamente") {
    auto fx = makeFixture({{"potion", {}, true, 10}}, "remove_overflow");
    const ItemAssetId potion = fx.am->loadItem("items/potion.mooditem");
    State s;
    REQUIRE(s.add(potion, 7, *fx.am));
    CHECK_FALSE(s.remove(potion, 10));   // no hay 10, rechazo
    CHECK(s.count(potion) == 7);          // sin mutar
}

TEST_CASE("InventoryState: remove con qty<=0 rechaza") {
    State s;
    CHECK_FALSE(s.remove(1, 0));
    CHECK_FALSE(s.remove(1, -3));
}

// ============================================================
// Grid2D — add con slot_index automatico
// ============================================================

TEST_CASE("InventoryState: Grid2D add asigna slot_index al primer slot libre") {
    auto fx = makeFixture({{"sword", {"weapon"}, false, 1}}, "grid_assign");
    const ItemAssetId sword = fx.am->loadItem("items/sword.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;  // 4 slots
    REQUIRE(s.add(sword, 3, *fx.am));
    REQUIRE(s.entries.size() == 3);
    CHECK(s.entries[0].slot_index == 0);
    CHECK(s.entries[1].slot_index == 1);
    CHECK(s.entries[2].slot_index == 2);
}

TEST_CASE("InventoryState: Grid2D add rechaza cuando capacity insufficient") {
    auto fx = makeFixture({{"sword", {}, false, 1}}, "grid_overflow");
    const ItemAssetId sword = fx.am->loadItem("items/sword.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 1;
    s.config.grid_height = 2;  // 2 slots
    REQUIRE(s.add(sword, 2, *fx.am));
    CHECK_FALSE(s.add(sword, 1, *fx.am));  // 3ro no entra
    CHECK(s.entries.size() == 2);
}

// ============================================================
// placeAt — Grid2D y EquipmentSlots
// ============================================================

TEST_CASE("InventoryState: placeAt en Grid2D coloca en slot especificado") {
    auto fx = makeFixture({{"sword", {}, false, 1}}, "placeat_grid");
    const ItemAssetId sword = fx.am->loadItem("items/sword.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    REQUIRE(s.placeAt(sword, 1, 3, *fx.am));   // slot 3 (esquina)
    REQUIRE(s.entries.size() == 1);
    CHECK(s.entries[0].slot_index == 3);
}

TEST_CASE("InventoryState: placeAt rechaza slot ocupado por item distinto") {
    auto fx = makeFixture({
        {"a", {}, false, 1},
        {"b", {}, false, 1},
    }, "placeat_collision");
    const ItemAssetId a = fx.am->loadItem("items/a.mooditem");
    const ItemAssetId b = fx.am->loadItem("items/b.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    REQUIRE(s.placeAt(a, 1, 0, *fx.am));
    CHECK_FALSE(s.placeAt(b, 1, 0, *fx.am));   // mismo slot, item distinto
    CHECK(s.entries.size() == 1);
    CHECK(s.entries[0].itemId == a);
}

TEST_CASE("InventoryState: placeAt rechaza slot fuera de rango y FlatList mode") {
    auto fx = makeFixture({{"x", {}, false, 1}}, "placeat_oob");
    const ItemAssetId x = fx.am->loadItem("items/x.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    CHECK_FALSE(s.placeAt(x, 1, -1, *fx.am));
    CHECK_FALSE(s.placeAt(x, 1, 99, *fx.am));
    // FlatList mode rechaza placeAt entero.
    s.mode = LayoutMode::FlatList;
    CHECK_FALSE(s.placeAt(x, 1, 0, *fx.am));
}

TEST_CASE("InventoryState: EquipmentSlots respeta tag_filter del slot") {
    auto fx = makeFixture({
        {"sword",  {"weapon"}, false, 1},
        {"helmet", {"armor"},  false, 1},
    }, "equip_tagfilter");
    const ItemAssetId sword  = fx.am->loadItem("items/sword.mooditem");
    const ItemAssetId helmet = fx.am->loadItem("items/helmet.mooditem");
    State s;
    s.mode = LayoutMode::EquipmentSlots;
    s.config.equipment_slots = {
        {"weapon_slot", "weapon"},
        {"head_slot",   "armor"},
    };
    // Sword va a weapon slot OK.
    REQUIRE(s.placeAt(sword, 1, 0, *fx.am));
    // Helmet rechaza slot 0 (tag filter "weapon"), pero acepta slot 1.
    CHECK_FALSE(s.placeAt(helmet, 1, 0, *fx.am));
    REQUIRE(s.placeAt(helmet, 1, 1, *fx.am));
    CHECK(s.entries.size() == 2);
}

TEST_CASE("InventoryState: EquipmentSlots con tag_filter vacio acepta cualquier item") {
    auto fx = makeFixture({{"random", {"misc"}, false, 1}}, "equip_anyfilter");
    const ItemAssetId rnd = fx.am->loadItem("items/random.mooditem");
    State s;
    s.mode = LayoutMode::EquipmentSlots;
    s.config.equipment_slots = {{"any_slot", ""}};  // sin filtro
    REQUIRE(s.placeAt(rnd, 1, 0, *fx.am));
    CHECK(s.entries.size() == 1);
}

// ============================================================
// moveSlot
// ============================================================

TEST_CASE("InventoryState: moveSlot cambia slot_index entre slots libres") {
    auto fx = makeFixture({{"x", {}, false, 1}}, "move_basic");
    const ItemAssetId x = fx.am->loadItem("items/x.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    REQUIRE(s.placeAt(x, 1, 0, *fx.am));
    REQUIRE(s.moveSlot(0, 3));
    CHECK(s.entries[0].slot_index == 3);
}

TEST_CASE("InventoryState: moveSlot rechaza destino ocupado (no swap en v1)") {
    auto fx = makeFixture({
        {"a", {}, false, 1},
        {"b", {}, false, 1},
    }, "move_collision");
    const ItemAssetId a = fx.am->loadItem("items/a.mooditem");
    const ItemAssetId b = fx.am->loadItem("items/b.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    REQUIRE(s.placeAt(a, 1, 0, *fx.am));
    REQUIRE(s.placeAt(b, 1, 1, *fx.am));
    CHECK_FALSE(s.moveSlot(0, 1));
}

TEST_CASE("InventoryState: moveSlot con origen vacio rechaza") {
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 2;
    s.config.grid_height = 2;
    CHECK_FALSE(s.moveSlot(0, 1));
}

// ============================================================
// clear
// ============================================================

TEST_CASE("InventoryState: clear borra entries pero preserva mode/config") {
    auto fx = makeFixture({{"x", {}, false, 1}}, "clear");
    const ItemAssetId x = fx.am->loadItem("items/x.mooditem");
    State s;
    s.mode = LayoutMode::Grid2D;
    s.config.grid_width = 3;
    s.config.grid_height = 3;
    REQUIRE(s.add(x, 5, *fx.am));
    s.clear();
    CHECK(s.entries.empty());
    CHECK(s.mode == LayoutMode::Grid2D);
    CHECK(s.config.grid_width == 3);
    CHECK(s.slotCapacity() == 9);
}
