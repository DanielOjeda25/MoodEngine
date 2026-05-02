// Tests del SaveLoad (Hito 38 A): round-trip del `.moodsave` JSON.

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
           ("mood_save_" + tag + "_" + std::to_string(stamp) + ".moodsave");
}

} // namespace

TEST_CASE("SaveLoad: round-trip preserva campos basicos") {
    SaveLoad::SaveData d;
    d.mapPath        = "maps/level1.moodmap";
    d.hud.hp         = 75;
    d.hud.ammo       = 22;
    d.playerPosition = glm::vec3(3.5f, 1.6f, -7.25f);
    d.playerYaw      = 45.0f;
    d.playerPitch    = -10.5f;

    const auto path = tempSavePath("roundtrip");
    REQUIRE(SaveLoad::save(d, path));

    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->mapPath == "maps/level1.moodmap");
    CHECK(loaded->hud.hp == 75);
    CHECK(loaded->hud.ammo == 22);
    CHECK(loaded->playerPosition.x == doctest::Approx(3.5f));
    CHECK(loaded->playerPosition.y == doctest::Approx(1.6f));
    CHECK(loaded->playerPosition.z == doctest::Approx(-7.25f));
    CHECK(loaded->playerYaw   == doctest::Approx(45.0f));
    CHECK(loaded->playerPitch == doctest::Approx(-10.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load de archivo inexistente devuelve nullopt") {
    const auto fake = std::filesystem::temp_directory_path() / "no_existe_save_123.moodsave";
    std::filesystem::remove(fake);
    const auto r = SaveLoad::load(fake);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("SaveLoad: load de JSON malformado devuelve nullopt") {
    const auto path = tempSavePath("malformed");
    {
        std::ofstream f(path);
        f << "esto no es JSON @#$";
    }
    const auto r = SaveLoad::load(path);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load de version futura devuelve nullopt") {
    const auto path = tempSavePath("future_version");
    {
        std::ofstream f(path);
        f << R"({"version": 99, "map_path": "maps/future.moodmap"})";
    }
    const auto r = SaveLoad::load(path);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: campos faltantes caen a defaults sin lanzar") {
    // JSON valido pero solo con version + map_path; el resto debe caer a
    // defaults del SaveData (hp=100, ammo=30, position=0, yaw=-90, pitch=0).
    const auto path = tempSavePath("partial");
    {
        std::ofstream f(path);
        f << R"({"version": 1, "map_path": "maps/partial.moodmap"})";
    }
    const auto r = SaveLoad::load(path);
    REQUIRE(r.has_value());
    CHECK(r->mapPath == "maps/partial.moodmap");
    CHECK(r->hud.hp == 100);     // default de HudState
    CHECK(r->hud.ammo == 30);    // default de HudState
    CHECK(r->playerPosition == glm::vec3(0.0f));
    CHECK(r->playerYaw == doctest::Approx(-90.0f));
    CHECK(r->playerPitch == doctest::Approx(0.0f));
    std::filesystem::remove(path);
}
