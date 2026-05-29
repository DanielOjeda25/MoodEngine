// Tests del WeaponSpec (F4H2 Sub-tarea 1). Cubre: defaults del spec
// vacio, roundtrip JSON, missing-field defaults, version mismatch,
// clamps de sanidad (damage<0, pellets<1, fireRate<=0, spread<0),
// saveToFile + loadFromFile roundtrip, file extension.

#include <doctest/doctest.h>

#include "engine/gameplay/weapon/WeaponSpec.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Mood;
using namespace Mood::Weapon;

namespace {

// Helper para crear un temp path unico por test, evitando colisiones
// si se corre la suite en paralelo en CI.
std::filesystem::path makeTempWeaponPath(const char* tag) {
    auto dir = std::filesystem::temp_directory_path() / "moodengine_weapon_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto path = dir / (std::string("weapon_") + tag + ".moodweapon");
    std::error_code rmEc;
    std::filesystem::remove(path, rmEc);
    return path;
}

} // namespace

// ============================================================
// Defaults del spec vacio
// ============================================================

TEST_CASE("WeaponSpec: default-constructed tiene defaults sanos") {
    Spec s;
    CHECK(s.displayName.empty());
    CHECK(s.category == "hitscan");

    CHECK(s.damage == doctest::Approx(10.0f));
    CHECK(s.range == doctest::Approx(50.0f));
    CHECK(s.pellets == 1u);
    CHECK(s.spreadDeg == doctest::Approx(0.0f));

    CHECK(s.fireRatePerSec == doctest::Approx(1.0f));
    CHECK(s.magazineSize == 10u);
    CHECK(s.reloadTimeSec == doctest::Approx(1.0f));

    CHECK(s.viewmodelMesh.empty());
    CHECK(s.viewmodelMaterial.empty());
    CHECK(s.fireSound.empty());
    CHECK(s.impactSound.empty());
    CHECK(s.muzzleVfx.empty());
    CHECK(s.impactVfx.empty());

    CHECK(s.ignoreOwner == true);
}

TEST_CASE("WeaponSpec: file extension constant") {
    CHECK(std::string(k_fileExtension) == ".moodweapon");
}

// ============================================================
// Roundtrip JSON
// ============================================================

TEST_CASE("WeaponSpec: empty spec roundtrip preservation") {
    Spec a;
    nlohmann::json j = a.toJson();
    Spec b = Spec::fromJson(j);

    CHECK(b.displayName == a.displayName);
    CHECK(b.category == a.category);
    CHECK(b.damage == doctest::Approx(a.damage));
    CHECK(b.pellets == a.pellets);
    CHECK(b.fireRatePerSec == doctest::Approx(a.fireRatePerSec));
    CHECK(b.ignoreOwner == a.ignoreOwner);
}

TEST_CASE("WeaponSpec: shotgun-like spec roundtrip preserves todos los campos") {
    Spec a;
    a.displayName       = "Escopeta";
    a.category          = "hitscan";
    a.damage            = 15.0f;
    a.range             = 25.0f;
    a.pellets           = 8u;
    a.spreadDeg         = 6.0f;
    a.fireRatePerSec    = 1.5f;
    a.magazineSize      = 6u;
    a.reloadTimeSec     = 1.8f;
    a.viewmodelMesh     = "viewmodel/shotgun.obj";
    a.viewmodelMaterial = "materials/shotgun.moodmaterial";
    a.fireSound         = "sfx/shotgun_fire.ogg";
    a.impactSound       = "sfx/bullet_impact.ogg";
    a.muzzleVfx         = "vfx/muzzle_flash.moodvfx";
    a.impactVfx         = "vfx/bullet_impact.moodvfx";
    a.ignoreOwner       = false;

    nlohmann::json j = a.toJson();
    Spec b = Spec::fromJson(j);

    CHECK(b.displayName == "Escopeta");
    CHECK(b.category == "hitscan");
    CHECK(b.damage == doctest::Approx(15.0f));
    CHECK(b.range == doctest::Approx(25.0f));
    CHECK(b.pellets == 8u);
    CHECK(b.spreadDeg == doctest::Approx(6.0f));
    CHECK(b.fireRatePerSec == doctest::Approx(1.5f));
    CHECK(b.magazineSize == 6u);
    CHECK(b.reloadTimeSec == doctest::Approx(1.8f));
    CHECK(b.viewmodelMesh == "viewmodel/shotgun.obj");
    CHECK(b.viewmodelMaterial == "materials/shotgun.moodmaterial");
    CHECK(b.fireSound == "sfx/shotgun_fire.ogg");
    CHECK(b.impactSound == "sfx/bullet_impact.ogg");
    CHECK(b.muzzleVfx == "vfx/muzzle_flash.moodvfx");
    CHECK(b.impactVfx == "vfx/bullet_impact.moodvfx");
    CHECK(b.ignoreOwner == false);
}

TEST_CASE("WeaponSpec: pistol-like spec (pellets=1, spread=0) roundtrip") {
    Spec a;
    a.displayName    = "Pistola";
    a.damage         = 25.0f;
    a.range          = 80.0f;
    a.pellets        = 1u;
    a.spreadDeg      = 0.0f;
    a.fireRatePerSec = 5.0f;
    a.magazineSize   = 12u;

    nlohmann::json j = a.toJson();
    Spec b = Spec::fromJson(j);

    CHECK(b.displayName == "Pistola");
    CHECK(b.damage == doctest::Approx(25.0f));
    CHECK(b.pellets == 1u);
    CHECK(b.spreadDeg == doctest::Approx(0.0f));
    CHECK(b.fireRatePerSec == doctest::Approx(5.0f));
}

// ============================================================
// Missing fields → defaults
// ============================================================

TEST_CASE("WeaponSpec: JSON sin campos opcionales usa defaults sanos") {
    nlohmann::json j = {{"_version", Spec::k_schemaVersion}};
    Spec s = Spec::fromJson(j);

    // Mismos defaults que el constructor por defecto.
    CHECK(s.displayName.empty());
    CHECK(s.category == "hitscan");
    CHECK(s.damage == doctest::Approx(10.0f));
    CHECK(s.pellets == 1u);
    CHECK(s.fireRatePerSec == doctest::Approx(1.0f));
    CHECK(s.ignoreOwner == true);
}

TEST_CASE("WeaponSpec: JSON con solo subset de campos preserva solo esos") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"displayName", "Solo Daño"},
        {"damage", 42.0f},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.displayName == "Solo Daño");
    CHECK(s.damage == doctest::Approx(42.0f));
    // Resto defaults
    CHECK(s.range == doctest::Approx(50.0f));
    CHECK(s.pellets == 1u);
}

// ============================================================
// Version mismatch
// ============================================================

TEST_CASE("WeaponSpec: schema version != k_schemaVersion devuelve spec vacio") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion + 99u},
        {"damage", 999.0f},
    };
    Spec s = Spec::fromJson(j);
    // El damage del JSON debe ignorarse — fromJson devuelve defaults.
    CHECK(s.damage == doctest::Approx(10.0f));
}

TEST_CASE("WeaponSpec: JSON no-objeto devuelve spec vacio") {
    nlohmann::json j = nlohmann::json::array();
    Spec s = Spec::fromJson(j);
    CHECK(s.damage == doctest::Approx(10.0f));
}

// ============================================================
// Clamps de sanidad
// ============================================================

TEST_CASE("WeaponSpec: damage < 0 se clampea a 0") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"damage", -10.0f},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.damage == doctest::Approx(0.0f));
}

TEST_CASE("WeaponSpec: pellets == 0 se clampea a 1") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"pellets", 0u},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.pellets == 1u);
}

TEST_CASE("WeaponSpec: fireRate <= 0 se clampea a positivo pequeño") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"fireRatePerSec", 0.0f},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.fireRatePerSec > 0.0f);

    nlohmann::json j2 = {
        {"_version", Spec::k_schemaVersion},
        {"fireRatePerSec", -2.0f},
    };
    Spec s2 = Spec::fromJson(j2);
    CHECK(s2.fireRatePerSec > 0.0f);
}

TEST_CASE("WeaponSpec: spreadDeg fuera de rango se clampea [0, 89]") {
    nlohmann::json jNeg = {
        {"_version", Spec::k_schemaVersion},
        {"spreadDeg", -5.0f},
    };
    Spec a = Spec::fromJson(jNeg);
    CHECK(a.spreadDeg == doctest::Approx(0.0f));

    nlohmann::json jBig = {
        {"_version", Spec::k_schemaVersion},
        {"spreadDeg", 180.0f},
    };
    Spec b = Spec::fromJson(jBig);
    CHECK(b.spreadDeg <= 89.0f);
}

TEST_CASE("WeaponSpec: magazineSize < 1 se clampea a 1") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"magazineSize", 0u},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.magazineSize == 1u);
}

TEST_CASE("WeaponSpec: reloadTime < 0 se clampea a 0") {
    nlohmann::json j = {
        {"_version", Spec::k_schemaVersion},
        {"reloadTimeSec", -3.0f},
    };
    Spec s = Spec::fromJson(j);
    CHECK(s.reloadTimeSec == doctest::Approx(0.0f));
}

// ============================================================
// saveToFile + loadFromFile
// ============================================================

TEST_CASE("WeaponSpec: saveToFile + loadFromFile roundtrip") {
    auto path = makeTempWeaponPath("io_roundtrip");

    Spec a;
    a.displayName = "Test";
    a.damage = 33.0f;
    a.pellets = 4u;
    a.spreadDeg = 8.0f;
    a.fireSound = "sfx/test.ogg";

    REQUIRE(a.saveToFile(path));
    auto loaded = Spec::loadFromFile(path);
    REQUIRE(loaded.has_value());

    CHECK(loaded->displayName == "Test");
    CHECK(loaded->damage == doctest::Approx(33.0f));
    CHECK(loaded->pellets == 4u);
    CHECK(loaded->spreadDeg == doctest::Approx(8.0f));
    CHECK(loaded->fireSound == "sfx/test.ogg");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("WeaponSpec: loadFromFile en archivo inexistente devuelve nullopt") {
    auto path = makeTempWeaponPath("inexistente_nunca_se_crea");
    auto loaded = Spec::loadFromFile(path);
    CHECK(!loaded.has_value());
}

TEST_CASE("WeaponSpec: loadFromFile en archivo con JSON invalido devuelve nullopt") {
    auto path = makeTempWeaponPath("malformed_json");
    {
        std::ofstream out(path);
        out << "not a json at all";
    }
    auto loaded = Spec::loadFromFile(path);
    CHECK(!loaded.has_value());

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
