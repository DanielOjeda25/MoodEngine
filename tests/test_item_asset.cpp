// Tests del ItemAsset (F2H51 Bloque B). Cubre: defaults del asset
// vacio, roundtrip JSON preservando todos los campos, missing-field
// defaults, bumped-version rejection, stats key-value libre,
// saveToFile + loadFromFile roundtrip, id-from-stem fallback.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/inventory/ItemAsset.h"
#include "engine/render/rhi/ITexture.h"

#include <fstream>

using namespace Mood;
using namespace Mood::Inventory;

// ============================================================
// Defaults del asset vacio
// ============================================================

TEST_CASE("ItemAsset: default-constructed tiene todos los campos vacios + stack/slot defaults") {
    Asset a;
    CHECK(a.id.empty());
    CHECK(a.name_key.empty());
    CHECK(a.name_literal.empty());
    CHECK(a.description_key.empty());
    CHECK(a.description_literal.empty());
    CHECK(a.icon_path.empty());
    CHECK(a.model_path.empty());
    CHECK(a.tags.empty());
    CHECK(a.stats.empty());
    CHECK(a.stack.stackable == false);
    CHECK(a.stack.max_stack == 1);
    CHECK(a.slot_size.width == 1);
    CHECK(a.slot_size.height == 1);
}

TEST_CASE("ItemAsset: file extension constant") {
    CHECK(std::string(k_fileExtension) == ".mooditem");
}

// ============================================================
// Roundtrip JSON
// ============================================================

TEST_CASE("ItemAsset: empty asset roundtrip preserves emptiness") {
    Asset a;
    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);
    CHECK(b.id.empty());
    CHECK(b.tags.empty());
    CHECK(b.stats.empty());
    CHECK(b.stack.stackable == false);
    CHECK(b.stack.max_stack == 1);
    CHECK(b.slot_size.width == 1);
    CHECK(b.slot_size.height == 1);
}

TEST_CASE("ItemAsset: roundtrip preserves todos los campos") {
    Asset a;
    a.id                  = "iron_sword";
    a.name_key            = "items.iron_sword.name";
    a.name_literal        = "Espada de Hierro";
    a.description_key     = "items.iron_sword.desc";
    a.description_literal = "Una hoja resistente.";
    a.icon_path           = "icons/items/iron_sword.png";
    a.model_path          = "models/items/iron_sword.glb";
    a.tags                = {"weapon", "metal", "common"};
    a.stats               = {{"damage", 12.5f}, {"weight", 3.0f}, {"durability_max", 100.0f}};
    a.stack               = {false, 1};
    a.slot_size           = {1, 2};

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    CHECK(b.id == "iron_sword");
    CHECK(b.name_key == "items.iron_sword.name");
    CHECK(b.name_literal == "Espada de Hierro");
    CHECK(b.description_key == "items.iron_sword.desc");
    CHECK(b.description_literal == "Una hoja resistente.");
    CHECK(b.icon_path == "icons/items/iron_sword.png");
    CHECK(b.model_path == "models/items/iron_sword.glb");
    REQUIRE(b.tags.size() == 3);
    CHECK(b.tags[0] == "weapon");
    CHECK(b.tags[1] == "metal");
    CHECK(b.tags[2] == "common");
    REQUIRE(b.stats.size() == 3);
    CHECK(b.stats.at("damage") == doctest::Approx(12.5f));
    CHECK(b.stats.at("weight") == doctest::Approx(3.0f));
    CHECK(b.stats.at("durability_max") == doctest::Approx(100.0f));
    CHECK(b.stack.stackable == false);
    CHECK(b.stack.max_stack == 1);
    CHECK(b.slot_size.width == 1);
    CHECK(b.slot_size.height == 2);
}

TEST_CASE("ItemAsset: stackable item con max_stack > 1 sobrevive roundtrip") {
    Asset a;
    a.id = "health_potion";
    a.tags = {"consumable", "common"};
    a.stack.stackable = true;
    a.stack.max_stack = 10;

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);
    CHECK(b.stack.stackable == true);
    CHECK(b.stack.max_stack == 10);
}

TEST_CASE("ItemAsset: stats keys arbitrarios (motor no interpreta semantica)") {
    Asset a;
    a.id = "weird_item";
    // El dev del juego decide los keys; el motor no asume nada.
    a.stats = {
        {"mana_regen_per_sec", 1.5f},
        {"crit_chance_pct",    7.5f},
        {"luck_modifier",      -0.25f},
    };
    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);
    CHECK(b.stats.size() == 3);
    CHECK(b.stats.at("mana_regen_per_sec") == doctest::Approx(1.5f));
    CHECK(b.stats.at("crit_chance_pct") == doctest::Approx(7.5f));
    CHECK(b.stats.at("luck_modifier") == doctest::Approx(-0.25f));
}

// ============================================================
// Missing-field defaults + version rejection
// ============================================================

TEST_CASE("ItemAsset: fromJson rejects wrong schema version") {
    nlohmann::json j;
    j["_version"] = 999u;
    j["id"] = "should_not_load";
    Asset a = Asset::fromJson(j);
    CHECK(a.id.empty());  // rechazado -> asset vacio
}

TEST_CASE("ItemAsset: fromJson rejects non-object JSON") {
    nlohmann::json arr = nlohmann::json::array();
    Asset a = Asset::fromJson(arr);
    CHECK(a.id.empty());
}

TEST_CASE("ItemAsset: fromJson con campos faltantes usa defaults sensatos") {
    nlohmann::json j;
    j["_version"] = Asset::k_schemaVersion;
    j["id"] = "minimal_item";
    // NO incluyo: tags, stats, stack, slot_size, name_*, description_*, paths.
    Asset a = Asset::fromJson(j);
    CHECK(a.id == "minimal_item");
    CHECK(a.name_key.empty());
    CHECK(a.tags.empty());
    CHECK(a.stats.empty());
    CHECK(a.stack.stackable == false);
    CHECK(a.stack.max_stack == 1);
    CHECK(a.slot_size.width == 1);
    CHECK(a.slot_size.height == 1);
}

TEST_CASE("ItemAsset: fromJson con tags array de tipos mixtos solo conserva strings") {
    nlohmann::json j;
    j["_version"] = Asset::k_schemaVersion;
    j["id"] = "robust_parse";
    j["tags"] = nlohmann::json::array();
    j["tags"].push_back("valid");
    j["tags"].push_back(42);            // int — debe ignorarse
    j["tags"].push_back("also_valid");
    j["tags"].push_back(nullptr);       // null — debe ignorarse
    Asset a = Asset::fromJson(j);
    REQUIRE(a.tags.size() == 2);
    CHECK(a.tags[0] == "valid");
    CHECK(a.tags[1] == "also_valid");
}

TEST_CASE("ItemAsset: fromJson con stats de tipos mixtos solo conserva numericos") {
    nlohmann::json j;
    j["_version"] = Asset::k_schemaVersion;
    j["id"] = "robust_stats";
    j["stats"] = nlohmann::json::object();
    j["stats"]["damage"] = 10.0;
    j["stats"]["broken_string"] = "not_a_number";  // string — debe ignorarse
    j["stats"]["broken_null"] = nullptr;            // null — debe ignorarse
    j["stats"]["weight"] = 2;                        // int promueve a float
    Asset a = Asset::fromJson(j);
    CHECK(a.stats.size() == 2);
    CHECK(a.stats.at("damage") == doctest::Approx(10.0f));
    CHECK(a.stats.at("weight") == doctest::Approx(2.0f));
    CHECK(a.stats.count("broken_string") == 0);
    CHECK(a.stats.count("broken_null") == 0);
}

// ============================================================
// I/O de disco (loadFromFile / saveToFile)
// ============================================================

TEST_CASE("ItemAsset: saveToFile + loadFromFile roundtrip preserva todo") {
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "mood_item_io_test";
    fs::create_directories(tmpDir);
    const fs::path filePath = tmpDir / "iron_sword.mooditem";
    fs::remove(filePath);

    Asset a;
    a.id = "iron_sword";
    a.name_literal = "Iron Sword";
    a.tags = {"weapon", "metal"};
    a.stats = {{"damage", 15.0f}};
    a.stack.stackable = false;
    a.slot_size = {1, 2};

    REQUIRE(a.saveToFile(filePath));
    REQUIRE(fs::exists(filePath));

    auto loaded = Asset::loadFromFile(filePath);
    REQUIRE(loaded.has_value());
    CHECK(loaded->id == "iron_sword");
    CHECK(loaded->name_literal == "Iron Sword");
    REQUIRE(loaded->tags.size() == 2);
    CHECK(loaded->tags[0] == "weapon");
    CHECK(loaded->stats.at("damage") == doctest::Approx(15.0f));
    CHECK(loaded->slot_size.height == 2);

    fs::remove(filePath);
}

TEST_CASE("ItemAsset: loadFromFile retorna nullopt para archivo inexistente") {
    namespace fs = std::filesystem;
    const fs::path noPath = fs::temp_directory_path() / "no_existe_12345.mooditem";
    auto loaded = Asset::loadFromFile(noPath);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("ItemAsset: loadFromFile usa stem si id vacio en JSON") {
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "mood_item_io_stem";
    fs::create_directories(tmpDir);
    const fs::path filePath = tmpDir / "auto_named_item.mooditem";

    Asset a;
    // NO seteo id. Solo guardo.
    REQUIRE(a.saveToFile(filePath));
    auto loaded = Asset::loadFromFile(filePath);
    REQUIRE(loaded.has_value());
    CHECK(loaded->id == "auto_named_item");

    fs::remove(filePath);
}

TEST_CASE("ItemAsset: loadFromFile rechaza JSON con sintaxis invalida") {
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "mood_item_io_bad";
    fs::create_directories(tmpDir);
    const fs::path filePath = tmpDir / "broken.mooditem";
    {
        std::ofstream out(filePath);
        out << "{not valid json";
    }
    auto loaded = Asset::loadFromFile(filePath);
    CHECK_FALSE(loaded.has_value());
    fs::remove(filePath);
}

// ============================================================
// AssetManager integration (F2H51 Bloque C — loadItem/getItem/etc)
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
} // namespace

TEST_CASE("AssetManager: arranca con slot 0 item vacio + missingItemId()==0") {
    AssetManager am("assets", dummyTexFactory());
    CHECK(am.missingItemId() == 0u);
    CHECK(am.itemCount() == 1u);
    CHECK(am.itemPathOf(0u) == "__empty_item");
    const Inventory::Asset* empty = am.getItem(0u);
    REQUIRE(empty != nullptr);
    CHECK(empty->id.empty());
    CHECK(empty->tags.empty());
}

TEST_CASE("AssetManager: loadItem con path no resoluble cae al slot 0") {
    AssetManager am("assets", dummyTexFactory());
    const ItemAssetId id = am.loadItem("items/no_existe_12345.mooditem");
    CHECK(id == am.missingItemId());
    // Segunda llamada con mismo path: hit en cache, sigue devolviendo 0.
    CHECK(am.loadItem("items/no_existe_12345.mooditem") == am.missingItemId());
    CHECK(am.itemCount() == 1u);  // no se agrego ningun slot nuevo
}

TEST_CASE("AssetManager: getItem con id fuera de rango cae al slot 0 (nunca null)") {
    AssetManager am("assets", dummyTexFactory());
    const Inventory::Asset* a = am.getItem(99999u);
    REQUIRE(a != nullptr);
    CHECK(a->id.empty());
    CHECK(am.itemPathOf(99999u) == "__empty_item");
}
