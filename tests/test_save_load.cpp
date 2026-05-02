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

TEST_CASE("SaveLoad: round-trip con body snapshots (Hito 41 A)") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";
    SaveLoad::BodySnapshot b1;
    b1.entityTag       = "Caja_01";
    b1.position        = glm::vec3(2.5f, 1.0f, -3.0f);
    b1.rotationQuat    = glm::vec4(0.0f, 0.7071f, 0.0f, 0.7071f);  // 90 Y
    b1.linearVelocity  = glm::vec3(1.0f, 0.0f, 0.5f);
    b1.angularVelocity = glm::vec3(0.0f, 1.5f, 0.0f);
    SaveLoad::BodySnapshot b2;
    b2.entityTag       = "Barril_07";
    b2.position        = glm::vec3(0.0f);
    b2.rotationQuat    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    d.bodies.push_back(b1);
    d.bodies.push_back(b2);

    const auto path = tempSavePath("bodies");
    REQUIRE(SaveLoad::save(d, path));
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->bodies.size() == 2);

    // Buscar por tag (orden no garantizado al leer).
    const SaveLoad::BodySnapshot* sb1 = nullptr;
    const SaveLoad::BodySnapshot* sb2 = nullptr;
    for (const auto& b : loaded->bodies) {
        if (b.entityTag == "Caja_01")    sb1 = &b;
        if (b.entityTag == "Barril_07") sb2 = &b;
    }
    REQUIRE(sb1 != nullptr);
    REQUIRE(sb2 != nullptr);
    CHECK(sb1->position.x == doctest::Approx(2.5f));
    CHECK(sb1->position.z == doctest::Approx(-3.0f));
    CHECK(sb1->rotationQuat.y == doctest::Approx(0.7071f));
    CHECK(sb1->linearVelocity.x == doctest::Approx(1.0f));
    CHECK(sb1->angularVelocity.y == doctest::Approx(1.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: round-trip con script globals filtradas (Hito 41 B)") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";
    SaveLoad::ScriptGlobalsSnapshot sg;
    sg.scriptPath = "assets/scripts/enemy.lua";
    sg.globals["hp"]        = ExposedValue{f32(75.0f)};
    sg.globals["alive"]     = ExposedValue{true};
    sg.globals["name"]      = ExposedValue{std::string("Boss")};
    sg.globals["spawn_pos"] = ExposedValue{glm::vec3(10.0f, 0.0f, -5.0f)};
    d.scriptGlobals.push_back(sg);

    const auto path = tempSavePath("globals");
    REQUIRE(SaveLoad::save(d, path));
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->scriptGlobals.size() == 1);
    const auto& got = loaded->scriptGlobals[0];
    CHECK(got.scriptPath == "assets/scripts/enemy.lua");
    REQUIRE(got.globals.count("hp") == 1);
    REQUIRE(got.globals.count("alive") == 1);
    REQUIRE(got.globals.count("name") == 1);
    REQUIRE(got.globals.count("spawn_pos") == 1);
    CHECK(std::get<f32>(got.globals.at("hp")) == doctest::Approx(75.0f));
    CHECK(std::get<bool>(got.globals.at("alive")) == true);
    CHECK(std::get<std::string>(got.globals.at("name")) == "Boss");
    CHECK(std::get<glm::vec3>(got.globals.at("spawn_pos")).x == doctest::Approx(10.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load v1 file con campos v2 vacios (back-compat Hito 41)") {
    // V1 file = sin bodies ni script_globals. Debe cargar OK.
    const auto path = tempSavePath("v1_compat");
    {
        std::ofstream f(path);
        f << R"({
            "version": 1,
            "map_path": "maps/legacy.moodmap",
            "hud": {"hp": 50, "ammo": 10}
        })";
    }
    const auto r = SaveLoad::load(path);
    REQUIRE(r.has_value());
    CHECK(r->mapPath == "maps/legacy.moodmap");
    CHECK(r->hud.hp == 50);
    CHECK(r->bodies.empty());
    CHECK(r->scriptGlobals.empty());
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
