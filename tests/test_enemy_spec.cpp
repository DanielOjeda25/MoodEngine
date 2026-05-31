// F4H7 — Tests del EnemySpec (.moodenemy). Cubre defaults, roundtrip JSON,
// schema version mismatch, clamps de sanidad (HP < 1, aggro < 0, speed cap,
// painDuration min), saveToFile/loadFromFile, file inexistente, JSON malformado.

#include <doctest/doctest.h>

#include "engine/gameplay/enemy/EnemySpec.h"

#include <filesystem>
#include <fstream>

using namespace Mood;
using namespace Mood::Enemy;

namespace {

std::filesystem::path makeTempEnemyPath(const char* tag) {
    auto dir = std::filesystem::temp_directory_path() / "moodengine_enemy_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto path = dir / (std::string("enemy_") + tag + ".moodenemy");
    std::error_code rmEc;
    std::filesystem::remove(path, rmEc);
    return path;
}

} // namespace

TEST_CASE("EnemySpec: defaults sanos") {
    Spec s;
    CHECK(s.displayName.empty());
    CHECK(s.health         == doctest::Approx(50.0f));
    CHECK(s.aggroRange     == doctest::Approx(12.0f));
    CHECK(s.attackRange    == doctest::Approx(2.0f));
    CHECK(s.moveSpeed      == doctest::Approx(4.0f));
    CHECK(s.damage         == doctest::Approx(15.0f));
    CHECK(s.attackCooldown == doctest::Approx(1.0f));
    CHECK(s.painThreshold  == doctest::Approx(10.0f));
    CHECK(s.painDuration   == doctest::Approx(0.3f));
}

TEST_CASE("EnemySpec: roundtrip JSON grunt completo") {
    Spec a;
    a.displayName    = "Grunt";
    a.health         = 75.0f;
    a.aggroRange     = 20.0f;
    a.attackRange    = 3.0f;
    a.moveSpeed      = 5.5f;
    a.damage         = 20.0f;
    a.attackCooldown = 1.5f;
    a.painThreshold  = 15.0f;
    a.painDuration   = 0.4f;
    a.viewmodelMesh  = "meshes/grunt.glb";
    a.hitSound       = "sfx/grunt_hit.ogg";
    a.deathSound     = "sfx/grunt_death.ogg";

    const auto j = a.toJson();
    const Spec b = Spec::fromJson(j);
    CHECK(b.displayName    == a.displayName);
    CHECK(b.health         == doctest::Approx(a.health));
    CHECK(b.aggroRange     == doctest::Approx(a.aggroRange));
    CHECK(b.attackRange    == doctest::Approx(a.attackRange));
    CHECK(b.moveSpeed      == doctest::Approx(a.moveSpeed));
    CHECK(b.damage         == doctest::Approx(a.damage));
    CHECK(b.attackCooldown == doctest::Approx(a.attackCooldown));
    CHECK(b.painThreshold  == doctest::Approx(a.painThreshold));
    CHECK(b.painDuration   == doctest::Approx(a.painDuration));
    CHECK(b.viewmodelMesh  == a.viewmodelMesh);
    CHECK(b.hitSound       == a.hitSound);
    CHECK(b.deathSound     == a.deathSound);
}

TEST_CASE("EnemySpec: missing-field cae a defaults") {
    nlohmann::json j;
    j["_version"] = Spec::k_schemaVersion;
    j["displayName"] = "MinSpec";
    // Sin ningun otro campo.
    const Spec s = Spec::fromJson(j);
    CHECK(s.displayName    == "MinSpec");
    CHECK(s.health         == doctest::Approx(50.0f));
    CHECK(s.aggroRange     == doctest::Approx(12.0f));
    CHECK(s.attackRange    == doctest::Approx(2.0f));
    CHECK(s.moveSpeed      == doctest::Approx(4.0f));
    CHECK(s.damage         == doctest::Approx(15.0f));
    CHECK(s.attackCooldown == doctest::Approx(1.0f));
    CHECK(s.painThreshold  == doctest::Approx(10.0f));
    CHECK(s.painDuration   == doctest::Approx(0.3f));
}

TEST_CASE("EnemySpec: schema version mismatch retorna defaults") {
    nlohmann::json j;
    j["_version"] = 99u;
    j["displayName"] = "BadVersion";
    j["health"] = 9000.0f;
    const Spec s = Spec::fromJson(j);
    CHECK(s.displayName.empty());
    CHECK(s.health == doctest::Approx(50.0f));
}

TEST_CASE("EnemySpec: clamps de sanidad") {
    nlohmann::json j;
    j["_version"]       = Spec::k_schemaVersion;
    j["health"]         = -5.0f;
    j["aggroRange"]     = -2.0f;
    j["attackRange"]    = -1.0f;
    j["moveSpeed"]      = 999.0f;     // sobre cap 50
    j["damage"]         = -100.0f;
    j["attackCooldown"] = 0.001f;     // bajo min 0.05
    j["painThreshold"]  = -5.0f;
    j["painDuration"]   = 0.0001f;    // bajo min 0.05

    const Spec s = Spec::fromJson(j);
    CHECK(s.health         >= 1.0f);
    CHECK(s.aggroRange     >= 0.01f);
    CHECK(s.attackRange    >= 0.01f);
    CHECK(s.moveSpeed      <= 50.0f);
    CHECK(s.moveSpeed      >= 0.0f);
    CHECK(s.damage         >= 0.0f);
    CHECK(s.attackCooldown >= 0.05f);
    CHECK(s.painThreshold  >= 0.0f);
    CHECK(s.painDuration   >= 0.05f);
}

TEST_CASE("EnemySpec: saveToFile + loadFromFile roundtrip") {
    Spec a;
    a.displayName = "FileRoundtrip";
    a.health      = 80.0f;
    a.aggroRange  = 15.0f;
    a.moveSpeed   = 6.0f;

    const auto path = makeTempEnemyPath("roundtrip");
    REQUIRE(a.saveToFile(path));

    const auto loaded = Spec::loadFromFile(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->displayName == a.displayName);
    CHECK(loaded->health      == doctest::Approx(a.health));
    CHECK(loaded->aggroRange  == doctest::Approx(a.aggroRange));
    CHECK(loaded->moveSpeed   == doctest::Approx(a.moveSpeed));
}

TEST_CASE("EnemySpec: loadFromFile archivo inexistente retorna nullopt") {
    const auto path = makeTempEnemyPath("nonexistent");
    const auto loaded = Spec::loadFromFile(path);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("EnemySpec: loadFromFile JSON malformado retorna nullopt") {
    const auto path = makeTempEnemyPath("malformed");
    {
        std::ofstream out(path);
        out << "{ this isn't JSON at all }";
    }
    const auto loaded = Spec::loadFromFile(path);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("EnemySpec: file extension constante") {
    CHECK(std::string(k_fileExtension) == ".moodenemy");
}
