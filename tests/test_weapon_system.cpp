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
    CHECK(wc.slots[wc.activeSlot].weaponAssetId != 0u);
    CHECK(wc.slots[wc.activeSlot].currentAmmo == 6);
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
    REQUIRE(e.getComponent<WeaponComponent>().slots[0].weaponAssetId != 0u);

    // Desequipar.
    CHECK(Weapon::equipWeapon(scene, e, "", *assets));
    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.slots[wc.activeSlot].weaponAssetId == 0u);
    CHECK(wc.slots[wc.activeSlot].currentAmmo == -1);
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
    CHECK(wc.slots[wc.activeSlot].weaponAssetId == 0u);
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
    wc.slots[wc.activeSlot].currentAmmo = 0;
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
    wc.slots[wc.activeSlot].currentAmmo = 3;  // no full -> reload activa

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
    wc.slots[wc.activeSlot].currentAmmo = 1;
    Weapon::reload(scene, e, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(1.0f));

    // Tick parcial — reload sigue activo.
    Weapon::tickSystem(scene, 0.5f, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(0.5f));
    CHECK(wc.slots[wc.activeSlot].currentAmmo == 1);

    // Tick que completa el reload (cross zero).
    Weapon::tickSystem(scene, 0.6f, *assets);
    CHECK(wc.reloadTimer == doctest::Approx(0.0f));
    CHECK(wc.slots[wc.activeSlot].currentAmmo == 6);  // mag llena
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

// ============================================================
// F4H3 — Multi-slot + swap APIs
// ============================================================

TEST_CASE("F4H3 multi-slot: equipWeaponInSlot pone armas en slots distintos") {
    auto root = setupTestAssetRoot("multi_equip_slots");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p1 = writeTestWeapon(root, "w1", 20.0f, 1u, 1.0f, 8u);

    Scene scene;
    Entity e = scene.createEntity("player");

    CHECK(Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets));
    CHECK(Weapon::equipWeaponInSlot(scene, e, 1, p1, *assets));

    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.slots[0].weaponAssetId != 0u);
    CHECK(wc.slots[1].weaponAssetId != 0u);
    CHECK(wc.slots[0].weaponAssetId != wc.slots[1].weaponAssetId);
    CHECK(wc.slots[0].currentAmmo == 5);
    CHECK(wc.slots[1].currentAmmo == 8);
    CHECK(wc.activeSlot == 0u); // por default
}

TEST_CASE("F4H3 multi-slot: equipWeaponInSlot fuera de rango devuelve false") {
    auto root = setupTestAssetRoot("multi_equip_oob");
    auto assets = makeTestAssetManager(root);
    const std::string p = writeTestWeapon(root, "w", 10.0f, 1u, 1.0f, 5u);

    Scene scene;
    Entity e = scene.createEntity("p");
    CHECK_FALSE(Weapon::equipWeaponInSlot(scene, e, 99u, p, *assets));
}

TEST_CASE("F4H3 swap: swapToSlot cambia activeSlot + actualiza lastActiveSlot") {
    auto root = setupTestAssetRoot("swap_to_slot");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p2 = writeTestWeapon(root, "w2", 30.0f, 1u, 1.0f, 3u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 2, p2, *assets);

    CHECK(Weapon::swapToSlot(scene, e, 2));
    const auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.activeSlot == 2u);
    CHECK(wc.lastActiveSlot == 0u);
}

TEST_CASE("F4H3 swap: swapToSlot al mismo slot devuelve false (no-op)") {
    auto root = setupTestAssetRoot("swap_noop");
    auto assets = makeTestAssetManager(root);
    const std::string p = writeTestWeapon(root, "w", 10.0f, 1u, 1.0f, 5u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeapon(scene, e, p, *assets); // slot 0

    CHECK_FALSE(Weapon::swapToSlot(scene, e, 0));
}

TEST_CASE("F4H3 swap: swapToSlot fuera de rango devuelve false") {
    auto root = setupTestAssetRoot("swap_oob");
    auto assets = makeTestAssetManager(root);
    Scene scene;
    Entity e = scene.createEntity("player");
    e.addComponent<WeaponComponent>();
    CHECK_FALSE(Weapon::swapToSlot(scene, e, 99u));
}

TEST_CASE("F4H3 swap: swapToSlot resetea fire+reload timers") {
    auto root = setupTestAssetRoot("swap_resets_timers");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p1 = writeTestWeapon(root, "w1", 20.0f, 1u, 1.0f, 5u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 1, p1, *assets);

    auto& wc = e.getComponent<WeaponComponent>();
    wc.fireTimer = 0.5f;
    wc.reloadTimer = 1.0f;

    CHECK(Weapon::swapToSlot(scene, e, 1));
    CHECK(wc.fireTimer == doctest::Approx(0.0f));
    CHECK(wc.reloadTimer == doctest::Approx(0.0f));
}

TEST_CASE("F4H3 swap: swapNext cicla y skipea slots vacios") {
    auto root = setupTestAssetRoot("swap_next_skip");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p2 = writeTestWeapon(root, "w2", 30.0f, 1u, 1.0f, 3u);

    // Slot 0 + 2 armados, 1 + 3 vacios.
    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 2, p2, *assets);

    auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.activeSlot == 0u);

    // Next 0 → 2 (skip 1 vacio).
    CHECK(Weapon::swapNext(scene, e));
    CHECK(wc.activeSlot == 2u);

    // Next 2 → 0 (skip 3 vacio + wrap).
    CHECK(Weapon::swapNext(scene, e));
    CHECK(wc.activeSlot == 0u);
}

TEST_CASE("F4H3 swap: swapNext sin otro slot armado devuelve false") {
    auto root = setupTestAssetRoot("swap_next_alone");
    auto assets = makeTestAssetManager(root);
    const std::string p = writeTestWeapon(root, "w", 10.0f, 1u, 1.0f, 5u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeapon(scene, e, p, *assets); // solo slot 0
    CHECK_FALSE(Weapon::swapNext(scene, e));
}

TEST_CASE("F4H3 swap: swapPrev cicla al reves y skipea vacios") {
    auto root = setupTestAssetRoot("swap_prev");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p3 = writeTestWeapon(root, "w3", 40.0f, 1u, 1.0f, 2u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 3, p3, *assets);

    // Prev 0 → 3 (wrap).
    CHECK(Weapon::swapPrev(scene, e));
    CHECK(e.getComponent<WeaponComponent>().activeSlot == 3u);
}

TEST_CASE("F4H3 swap: swapLast hace toggle activeSlot <-> lastActiveSlot") {
    auto root = setupTestAssetRoot("swap_last");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p1 = writeTestWeapon(root, "w1", 20.0f, 1u, 1.0f, 8u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 1, p1, *assets);

    // Swap a slot 1, last=0.
    Weapon::swapToSlot(scene, e, 1);
    auto& wc = e.getComponent<WeaponComponent>();
    CHECK(wc.activeSlot == 1u);
    CHECK(wc.lastActiveSlot == 0u);

    // swapLast toggle 1->0.
    CHECK(Weapon::swapLast(scene, e));
    CHECK(wc.activeSlot == 0u);
    CHECK(wc.lastActiveSlot == 1u);

    // swapLast again 0->1.
    CHECK(Weapon::swapLast(scene, e));
    CHECK(wc.activeSlot == 1u);
}

TEST_CASE("F4H3 swap: swapLast sin lastActiveSlot distinto devuelve false") {
    Scene scene;
    Entity e = scene.createEntity("player");
    e.addComponent<WeaponComponent>(); // activeSlot=lastActiveSlot=0
    CHECK_FALSE(Weapon::swapLast(scene, e));
}

TEST_CASE("F4H3 fire: usa el slot activo (no slot 0 hardcoded)") {
    auto root = setupTestAssetRoot("fire_active_slot");
    auto assets = makeTestAssetManager(root);
    const std::string p0 = writeTestWeapon(root, "w0", 10.0f, 1u, 1.0f, 5u);
    const std::string p1 = writeTestWeapon(root, "w1", 20.0f, 1u, 1.0f, 8u);

    Scene scene;
    Entity e = scene.createEntity("player");
    Weapon::equipWeaponInSlot(scene, e, 0, p0, *assets);
    Weapon::equipWeaponInSlot(scene, e, 1, p1, *assets);
    Weapon::swapToSlot(scene, e, 1);

    // Sin physics no podemos fire() — pero canFire/ammoLeft sí consulta
    // el slot activo. ammoLeft del slot 1 = 8 (no 5 del slot 0).
    CHECK(Weapon::ammoLeft(scene, e, *assets) == 8);
    CHECK(Weapon::canFire(scene, e, *assets));
}
