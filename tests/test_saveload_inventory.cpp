// break-A1+A2 — Tests del save-game para inventario runtime + dialog vars.
//
// Antes de break-A1+A2:
//   - Items recogidos en Play (pickup, inventory.add, quest rewards)
//     se PERDIAN al cargar (`SaveData` no tenia campo de inventario).
//   - `dialog.set_var` y `dialog.has_var` se perdian igual (no se
//     persistia GameState::dialogVars).
//
// Estos tests cubren el contrato testeable a nivel JSON:
//   - SaveData con inventories+dialogVars roundtripea OK.
//   - Saves pre-v4 (sin esos campos) cargan con vectors vacios.
//   - Item entries con qty=0 / itemPath vacio se filtran al cargar.
//   - dialog_vars como string_map sobreviven roundtrip.
//
// El cableado de captureCurrentState / applyLoadedSave a la Scene vive
// en PlayerApplication y requiere AssetManager + Scene + Lua — eso se
// valida en vivo (no hay test de integracion del Player todavia).

#include <doctest/doctest.h>

#include "engine/saving/SaveLoad.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Mood;

namespace {

std::filesystem::path tempSavePath(const std::string& tag) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("mood_save_inv_" + tag + "_" + std::to_string(stamp) + ".moodsave");
}

} // namespace

TEST_CASE("break-A1: roundtrip de inventories preserva tag + entries") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";

    SaveLoad::InventorySnapshot inv;
    inv.entityTag = "Player";
    inv.entries.push_back({"items/iron_sword.mooditem", 1, 0});
    inv.entries.push_back({"items/health_potion.mooditem", 5, 1});
    inv.entries.push_back({"items/gold_coin.mooditem", 99, -1}); // flat list
    d.inventories.push_back(std::move(inv));

    SaveLoad::InventorySnapshot chest;
    chest.entityTag = "ChestA";
    chest.entries.push_back({"items/iron_key.mooditem", 1, -1});
    d.inventories.push_back(std::move(chest));

    const auto path = tempSavePath("inventories");
    REQUIRE(SaveLoad::save(d, path));

    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->inventories.size() == 2);

    CHECK(loaded->inventories[0].entityTag == "Player");
    REQUIRE(loaded->inventories[0].entries.size() == 3);
    CHECK(loaded->inventories[0].entries[0].itemPath == "items/iron_sword.mooditem");
    CHECK(loaded->inventories[0].entries[0].quantity == 1);
    CHECK(loaded->inventories[0].entries[0].slotIndex == 0);
    CHECK(loaded->inventories[0].entries[1].itemPath == "items/health_potion.mooditem");
    CHECK(loaded->inventories[0].entries[1].quantity == 5);
    CHECK(loaded->inventories[0].entries[1].slotIndex == 1);
    CHECK(loaded->inventories[0].entries[2].itemPath == "items/gold_coin.mooditem");
    CHECK(loaded->inventories[0].entries[2].quantity == 99);
    CHECK(loaded->inventories[0].entries[2].slotIndex == -1);

    CHECK(loaded->inventories[1].entityTag == "ChestA");
    REQUIRE(loaded->inventories[1].entries.size() == 1);
    CHECK(loaded->inventories[1].entries[0].itemPath == "items/iron_key.mooditem");

    std::filesystem::remove(path);
}

TEST_CASE("break-A1: entries con qty<=0 o itemPath vacio se filtran al cargar") {
    // Simulamos un .moodsave v4 con basura en el array de entries —
    // el loader debe filtrar (defensivo, no abortar).
    const auto path = tempSavePath("filter");
    {
        std::ofstream f(path);
        f << R"({
          "version": 4,
          "map_path": "maps/test.moodmap",
          "inventories": [{
            "tag": "Player",
            "entries": [
              { "item_path": "items/ok.mooditem", "quantity": 3, "slot_index": 0 },
              { "item_path": "",                   "quantity": 1, "slot_index": 1 },
              { "item_path": "items/zero.mooditem","quantity": 0, "slot_index": 2 },
              { "item_path": "items/neg.mooditem", "quantity": -5,"slot_index": 3 }
            ]
          }]
        })";
    }
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->inventories.size() == 1);
    REQUIRE(loaded->inventories[0].entries.size() == 1);
    CHECK(loaded->inventories[0].entries[0].itemPath == "items/ok.mooditem");
    CHECK(loaded->inventories[0].entries[0].quantity == 3);
    std::filesystem::remove(path);
}

TEST_CASE("break-A1: inventories con tag vacio se descartan al cargar") {
    const auto path = tempSavePath("notag");
    {
        std::ofstream f(path);
        f << R"({
          "version": 4,
          "inventories": [
            { "tag": "", "entries": [{ "item_path": "items/x.mooditem", "quantity": 1, "slot_index": 0 }] },
            { "tag": "Player", "entries": [{ "item_path": "items/y.mooditem", "quantity": 2, "slot_index": 0 }] }
          ]
        })";
    }
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->inventories.size() == 1);
    CHECK(loaded->inventories[0].entityTag == "Player");
    std::filesystem::remove(path);
}

TEST_CASE("break-A2: roundtrip de dialog_vars preserva pares string->string") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";
    d.dialogVars["npc_alice_met"]    = "true";
    d.dialogVars["quest_intro_seen"] = "1";
    d.dialogVars["player_name"]      = "Daniel";

    const auto path = tempSavePath("dialogvars");
    REQUIRE(SaveLoad::save(d, path));

    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->dialogVars.size() == 3);
    CHECK(loaded->dialogVars.at("npc_alice_met")    == "true");
    CHECK(loaded->dialogVars.at("quest_intro_seen") == "1");
    CHECK(loaded->dialogVars.at("player_name")      == "Daniel");
    std::filesystem::remove(path);
}

TEST_CASE("break-A1+A2: saves v3 (pre-A1/A2) cargan con vectors vacios") {
    // Backwards compat: el upgrader v3->v4 es no-op (defaults a vacio).
    const auto path = tempSavePath("v3");
    {
        std::ofstream f(path);
        f << R"({
          "version": 3,
          "map_path": "maps/legacy.moodmap",
          "hud": { "hp": 80, "ammo": 12 }
        })";
    }
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->mapPath == "maps/legacy.moodmap");
    CHECK(loaded->hud.hp == 80);
    CHECK(loaded->inventories.empty());
    CHECK(loaded->dialogVars.empty());
    std::filesystem::remove(path);
}

TEST_CASE("break-A1+A2: save vacio NO emite los campos opcionales al JSON") {
    // Higiene del JSON: saves sin inventario / sin dialog_vars no deben
    // contener esos campos (mantiene archivos chicos para gameplay basico).
    SaveLoad::SaveData d;
    d.mapPath = "maps/basic.moodmap";

    const auto path = tempSavePath("emptyfields");
    REQUIRE(SaveLoad::save(d, path));

    // Scope para forzar cierre del ifstream antes del remove (Windows).
    std::string contents;
    {
        std::ifstream f(path);
        REQUIRE(f.is_open());
        contents.assign(std::istreambuf_iterator<char>(f),
                          std::istreambuf_iterator<char>());
    }
    CHECK(contents.find("inventories") == std::string::npos);
    CHECK(contents.find("dialog_vars") == std::string::npos);

    std::filesystem::remove(path);
}
