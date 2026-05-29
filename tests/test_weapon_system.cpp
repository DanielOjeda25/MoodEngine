// Tests del WeaponSystem (F4H2 Sub-tarea 3). Cubre las APIs que se
// pueden testear sin PhysicsWorld + AudioDevice: equipWeapon,
// tickSystem timers, canFire / ammoLeft / reload state machine.
//
// El test de end-to-end de fire() (raycast + damage + feedback) se
// hace en runtime con el editor — requiere Jolt + audio devices con
// un setup pesado fuera del scope de unit tests.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/gameplay/weapon/WeaponSystem.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;

namespace {

class StubTexture : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width()  const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return 0; }
};

std::filesystem::path setupTestAssetRoot(const char* tag) {
    auto root = std::filesystem::temp_directory_path()
                / "moodengine_weapon_system_tests" / tag;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "textures", ec);
    std::filesystem::create_directories(root / "audio", ec);
    std::filesystem::create_directories(root / "weapons", ec);
    // Crear stubs minimos para que AssetManager arranque (missing.png + missing.wav).
    {
        std::ofstream out(root / "textures" / "missing.png", std::ios::binary);
        // PNG minimo válido (1x1 white pixel) — 67 bytes.
        const unsigned char png[] = {
            0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,
            0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
            0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xDE,0x00,0x00,0x00,
            0x0C,0x49,0x44,0x41,0x54,0x08,0x99,0x63,0xF8,0xFF,0xFF,0xFF,
            0x3F,0x00,0x05,0xFE,0x02,0xFE,0xA3,0x35,0x81,0x84,0x00,0x00,
            0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,0x82
        };
        out.write(reinterpret_cast<const char*>(png), sizeof(png));
    }
    {
        std::ofstream out(root / "audio" / "missing.wav", std::ios::binary);
        // WAV mínimo "RIFF....WAVEfmt " — 44 bytes header silencio.
        const unsigned char wav[] = {
            'R','I','F','F',0x24,0x00,0x00,0x00,'W','A','V','E',
            'f','m','t',' ',0x10,0x00,0x00,0x00,0x01,0x00,0x01,0x00,
            0x44,0xAC,0x00,0x00,0x88,0x58,0x01,0x00,0x02,0x00,0x10,0x00,
            'd','a','t','a',0x00,0x00,0x00,0x00
        };
        out.write(reinterpret_cast<const char*>(wav), sizeof(wav));
    }
    return root;
}

std::unique_ptr<AssetManager> makeTestAssetManager(const std::filesystem::path& root) {
    AssetManager::TextureFactory texFactory =
        [](const std::string&) { return std::make_unique<StubTexture>(); };
    return std::make_unique<AssetManager>(root.generic_string(),
                                            std::move(texFactory));
}

std::string writeTestWeapon(const std::filesystem::path& root,
                              const std::string& filename,
                              f32 damage, u32 pellets,
                              f32 fireRate, u32 magSize) {
    Weapon::Spec spec;
    spec.displayName = filename;
    spec.damage = damage;
    spec.pellets = pellets;
    spec.fireRatePerSec = fireRate;
    spec.magazineSize = magSize;
    spec.reloadTimeSec = 1.0f;
    const std::string logical = "weapons/" + filename + ".moodweapon";
    spec.saveToFile(root / logical);
    return logical;
}

} // namespace

// ============================================================
// equipWeapon
// ============================================================

TEST_CASE("WeaponSystem: equipWeapon agrega componente con assetId + mag full") {
    auto root = setupTestAssetRoot("equip_basic");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "test", 10.0f, 4u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("shooter");

    CHECK(Weapon::equipWeapon(scene, e, path, *assets));
    REQUIRE(e.hasComponent<WeaponComponent>());
    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.weaponAssetId != 0u);
    CHECK(wc.currentAmmo == 6);
    CHECK(wc.fireTimer == doctest::Approx(0.0f));
    CHECK(wc.reloadTimer == doctest::Approx(0.0f));
}

TEST_CASE("WeaponSystem: equipWeapon con path vacio desequipa") {
    auto root = setupTestAssetRoot("equip_empty");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "t", 10.0f, 1u, 1.0f, 5u);

    Scene scene;
    Entity e = scene.createEntity("shooter");
    Weapon::equipWeapon(scene, e, path, *assets);
    REQUIRE(e.getComponent<WeaponComponent>().weaponAssetId != 0u);

    // Desequipar.
    CHECK(Weapon::equipWeapon(scene, e, "", *assets));
    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.weaponAssetId == 0u);
    CHECK(wc.currentAmmo == -1);
}

TEST_CASE("WeaponSystem: equipWeapon con asset inexistente cae al fallback") {
    auto root = setupTestAssetRoot("equip_missing");
    auto assets = makeTestAssetManager(root);

    Scene scene;
    Entity e = scene.createEntity("shooter");
    // El loadWeapon devuelve missingWeaponId() (0) para path inexistente.
    CHECK(Weapon::equipWeapon(scene, e, "weapons/nonexistent.moodweapon",
                                *assets));
    // weaponAssetId queda en 0 (fallback) — comportamiento data-driven sano.
    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.weaponAssetId == 0u);
}

// ============================================================
// canFire / ammoLeft state queries
// ============================================================

TEST_CASE("WeaponSystem: canFire false sin WeaponComponent") {
    auto root = setupTestAssetRoot("canfire_nocomp");
    auto assets = makeTestAssetManager(root);

    Scene scene;
    Entity e = scene.createEntity("noweapon");
    CHECK_FALSE(Weapon::canFire(scene, e, *assets));
    CHECK(Weapon::ammoLeft(scene, e, *assets) == 0);
}

TEST_CASE("WeaponSystem: canFire false con weaponAssetId == 0 (sin arma equipada)") {
    auto root = setupTestAssetRoot("canfire_emptyslot");
    auto assets = makeTestAssetManager(root);

    Scene scene;
    Entity e = scene.createEntity("e");
    e.addComponent<WeaponComponent>();  // default weaponAssetId = 0
    CHECK_FALSE(Weapon::canFire(scene, e, *assets));
}

TEST_CASE("WeaponSystem: canFire true al equipar con mag full") {
    auto root = setupTestAssetRoot("canfire_full");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 10u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    CHECK(Weapon::canFire(scene, e, *assets));
    CHECK(Weapon::ammoLeft(scene, e, *assets) == 10);
}

TEST_CASE("WeaponSystem: canFire false con cooldown activo") {
    auto root = setupTestAssetRoot("canfire_cooldown");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 10u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.fireTimer = 0.5f;
    CHECK_FALSE(Weapon::canFire(scene, e, *assets));
}

TEST_CASE("WeaponSystem: canFire false con reloadTimer activo") {
    auto root = setupTestAssetRoot("canfire_reload");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 10u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.reloadTimer = 1.0f;
    CHECK_FALSE(Weapon::canFire(scene, e, *assets));
}

TEST_CASE("WeaponSystem: canFire false con ammo == 0") {
    auto root = setupTestAssetRoot("canfire_noammo");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 10u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.currentAmmo = 0;
    CHECK_FALSE(Weapon::canFire(scene, e, *assets));
}

// ============================================================
// reload state machine
// ============================================================

TEST_CASE("WeaponSystem: reload arranca timer al spec.reloadTimeSec") {
    auto root = setupTestAssetRoot("reload_start");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);

    auto& wc = e.getComponent<WeaponComponent>();
    wc.currentAmmo = 3;  // no full -> reload activa

    CHECK(Weapon::reload(scene, e, *assets));
    CHECK(wc.reloadTimer == doctest::Approx(1.0f));
}

TEST_CASE("WeaponSystem: reload no-op si mag full") {
    auto root = setupTestAssetRoot("reload_full");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    // currentAmmo == magSize tras equip.
    CHECK_FALSE(Weapon::reload(scene, e, *assets));
}

TEST_CASE("WeaponSystem: reload no-op sin arma equipada") {
    auto root = setupTestAssetRoot("reload_noweapon");
    auto assets = makeTestAssetManager(root);

    Scene scene;
    Entity e = scene.createEntity("s");
    e.addComponent<WeaponComponent>();
    CHECK_FALSE(Weapon::reload(scene, e, *assets));
}

// ============================================================
// tickSystem timer decrements
// ============================================================

TEST_CASE("WeaponSystem: tickSystem decae fireTimer") {
    auto root = setupTestAssetRoot("tick_fire_timer");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.fireTimer = 0.5f;

    Weapon::tickSystem(scene, 0.2f, *assets);
    CHECK(wc.fireTimer == doctest::Approx(0.3f));

    Weapon::tickSystem(scene, 0.5f, *assets);
    CHECK(wc.fireTimer == doctest::Approx(0.0f));
}

TEST_CASE("WeaponSystem: tickSystem completa reload y rellena mag") {
    auto root = setupTestAssetRoot("tick_reload_complete");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.currentAmmo = 1;
    Weapon::reload(scene, e, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(1.0f));

    // Tick parcial — reload sigue activo.
    Weapon::tickSystem(scene, 0.5f, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(0.5f));
    CHECK(wc.currentAmmo == 1);

    // Tick que completa el reload (cross zero).
    Weapon::tickSystem(scene, 0.6f, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(0.0f));
    CHECK(wc.currentAmmo == 6);  // mag llena
}

TEST_CASE("WeaponSystem: tickSystem auto-clears firing flag") {
    auto root = setupTestAssetRoot("tick_firing_flag");
    auto assets = makeTestAssetManager(root);
    const std::string path = writeTestWeapon(root, "p", 5.0f, 1u, 2.0f, 6u);

    Scene scene;
    Entity e = scene.createEntity("s");
    Weapon::equipWeapon(scene, e, path, *assets);
    auto& wc = e.getComponent<WeaponComponent>();
    wc.firing = true;

    Weapon::tickSystem(scene, 0.1f, *assets);
    CHECK_FALSE(wc.firing);
}
