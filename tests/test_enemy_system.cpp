// F4H7 — Tests de la state machine del EnemySystem. Cubre:
// estado inicial Idle / Idle→Alert al entrar aggro / fuera de aggro no
// transition / Pain trigger via polling hitFlashTimer / Pain duration vuelve
// a Alert con target / Pain duration vuelve a Idle sin target / Dead state
// terminal / Dead auto-add RigidBody Dynamic / sin player → Idle / sin
// Transform skip.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/Health.h"
#include "engine/gameplay/enemy/EnemySpec.h"
#include "engine/gameplay/enemy/EnemySystem.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <fstream>
#include <memory>

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

std::filesystem::path setupAssetRoot(const char* tag) {
    auto root = std::filesystem::temp_directory_path()
                / "moodengine_enemy_system_tests" / tag;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "textures", ec);
    std::filesystem::create_directories(root / "audio", ec);
    std::filesystem::create_directories(root / "enemies", ec);
    {
        std::ofstream out(root / "textures" / "missing.png", std::ios::binary);
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

std::unique_ptr<AssetManager> makeAssets(const std::filesystem::path& root) {
    AssetManager::TextureFactory tf =
        [](const std::string&) { return std::make_unique<StubTexture>(); };
    return std::make_unique<AssetManager>(root.generic_string(), std::move(tf));
}

Entity makeEnemy(Scene& scene, const std::string& tag, glm::vec3 pos,
                  float hp = 50.0f) {
    Entity e = scene.createEntity(tag);
    auto& tf = e.addComponent<TransformComponent>();
    tf.position = pos;
    HealthComponent hc{};
    hc.current = hp;
    hc.max     = hp;
    e.addComponent<HealthComponent>(hc);
    EnemyComponent ec{};
    e.addComponent<EnemyComponent>(ec);
    return e;
}

Entity makePlayer(Scene& scene, glm::vec3 pos) {
    Entity p = scene.createEntity("player");
    auto& tf = p.addComponent<TransformComponent>();
    tf.position = pos;
    return p;
}

} // namespace

// ============================================================
// Estado inicial
// ============================================================

TEST_CASE("F4H7 EnemyComponent default state = Idle") {
    EnemyComponent ec{};
    CHECK(ec.state == EnemyState::Idle);
    CHECK(ec.stateTime == doctest::Approx(0.0f));
    CHECK(ec.targetEntity == EnemyComponent::k_noTarget);
    CHECK(ec.painsTotal == 0);
}

// ============================================================
// Transitions
// ============================================================

TEST_CASE("F4H7 Idle→Alert cuando player entra aggroRange") {
    auto root = setupAssetRoot("idle_alert");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Spec default tiene aggroRange=12 — player a 5m esta dentro.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    const auto& ec = enemy.getComponent<EnemyComponent>();
    CHECK(ec.state == EnemyState::Alert);
    CHECK(ec.targetEntity != EnemyComponent::k_noTarget);
}

TEST_CASE("F4H7 Idle queda Idle si player fuera de aggroRange") {
    auto root = setupAssetRoot("idle_far");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(30.0f, 0.0f, 0.0f));

    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    const auto& ec = enemy.getComponent<EnemyComponent>();
    CHECK(ec.state == EnemyState::Idle);
}

TEST_CASE("F4H7 Pain trigger cuando hitFlashTimer sube (recibio damage)") {
    auto root = setupAssetRoot("pain");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Tick inicial → Alert (player a 5m, dentro de aggro 12).
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Alert);

    // Simular daño → hitFlashTimer sube.
    Health::applyDamage(scene, enemy, 20.0f);
    REQUIRE(enemy.getComponent<HealthComponent>().hitFlashTimer > 0.0f);

    // Proximo tick detecta el bump y entra Pain.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    const auto& ec = enemy.getComponent<EnemyComponent>();
    CHECK(ec.state == EnemyState::Pain);
    CHECK(ec.painsTotal == 1);
}

TEST_CASE("F4H8 Pain → Chase despues de painDuration con target") {
    auto root = setupAssetRoot("pain_to_chase");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Setup: Alert + Pain.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Health::applyDamage(scene, enemy, 20.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Pain);

    // PainDuration default = 0.3s. Ticks de 0.1s × 4 → cubrimos > 0.3s.
    for (int i = 0; i < 4; ++i) {
        Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);
    }
    const auto& ec = enemy.getComponent<EnemyComponent>();
    // F4H8: Pain con target vuelve a Chase (no Alert) — el A* retoma.
    CHECK(ec.state == EnemyState::Chase);
}

TEST_CASE("F4H7 Dead transition cuando HP llega a 0") {
    auto root = setupAssetRoot("dead");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f), 10.0f);

    // Daño masivo > HP.
    Health::applyDamage(scene, enemy, 999.0f);
    REQUIRE(enemy.getComponent<HealthComponent>().dead);

    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Dead);
}

TEST_CASE("F4H7 Dead auto-add RigidBodyComponent Dynamic") {
    auto root = setupAssetRoot("dead_rb");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f), 10.0f);
    REQUIRE_FALSE(enemy.hasComponent<RigidBodyComponent>());

    Health::applyDamage(scene, enemy, 999.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    REQUIRE(enemy.hasComponent<RigidBodyComponent>());
    const auto& rb = enemy.getComponent<RigidBodyComponent>();
    CHECK(rb.type == RigidBodyComponent::Type::Dynamic);
}

TEST_CASE("F4H7 Dead state es terminal (no transiciona otra vez)") {
    auto root = setupAssetRoot("dead_terminal");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f), 10.0f);

    Health::applyDamage(scene, enemy, 999.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Dead);

    // Multiples ticks: queda en Dead siempre.
    for (int i = 0; i < 10; ++i) {
        Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);
    }
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Dead);
}

TEST_CASE("F4H7 Dead no double-add RigidBody") {
    auto root = setupAssetRoot("dead_nodupe");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f), 10.0f);

    Health::applyDamage(scene, enemy, 999.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.hasComponent<RigidBodyComponent>());

    // Multiples ticks: solo 1 RB.
    for (int i = 0; i < 5; ++i) {
        Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);
    }
    int rbCount = 0;
    scene.registry().view<RigidBodyComponent>().each(
        [&](entt::entity, RigidBodyComponent&) { rbCount += 1; });
    CHECK(rbCount == 1);
}

TEST_CASE("F4H7 sin player el enemy queda Idle") {
    auto root = setupAssetRoot("no_player");
    auto assets = makeAssets(root);
    Scene scene;
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(0.0f));

    Entity invalidPlayer;  // default-constructed = invalid
    Enemy::tickSystem(scene, 0.016f, invalidPlayer, nullptr, *assets);

    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Idle);
}

TEST_CASE("F4H7 Alert→Idle cuando player se aleja > aggroRange * 1.5") {
    auto root = setupAssetRoot("alert_idle");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Tick inicial → Alert.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Alert);

    // Player se aleja a 30m (>= 12 * 1.5 = 18).
    player.getComponent<TransformComponent>().position = glm::vec3(30.0f, 0.0f, 0.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Idle);
}

TEST_CASE("F4H7 Alert→Attack cuando player entra attackRange") {
    auto root = setupAssetRoot("alert_attack");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(1.5f, 0.0f, 0.0f));

    // attackRange default = 2m, player a 1.5m → Alert + Attack en mismo tick
    // sequence.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Alert);

    // Proximo tick: Alert → Attack (dist=1.5 <= attackRange=2).
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Attack);
}

TEST_CASE("F4H7 stateTime incrementa cada tick") {
    auto root = setupAssetRoot("statetime");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(30.0f, 0.0f, 0.0f));

    Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.1f, player, nullptr, *assets);

    const auto& ec = enemy.getComponent<EnemyComponent>();
    // Idle estable, stateTime = 3 ticks de 0.1s = 0.3s.
    CHECK(ec.stateTime == doctest::Approx(0.3f).epsilon(0.01f));
}

TEST_CASE("F4H7 multiples enemies independent state machines") {
    auto root = setupAssetRoot("multi");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity near  = makeEnemy(scene, "near",  glm::vec3(5.0f, 0.0f, 0.0f));
    Entity far_  = makeEnemy(scene, "far",   glm::vec3(30.0f, 0.0f, 0.0f));

    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    // Near entra Alert; far queda Idle (out of aggro).
    CHECK(near.getComponent<EnemyComponent>().state  == EnemyState::Alert);
    CHECK(far_.getComponent<EnemyComponent>().state  == EnemyState::Idle);
}

// =============================================================
// F4H8 — NavAgent integration (Chase + Attack tracking)
// =============================================================

TEST_CASE("F4H8 Alert→Chase cuando dist > attackRange") {
    auto root = setupAssetRoot("alert_chase");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Tick 1: Idle → Alert.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Alert);

    // Tick 2: F4H8 (D5) — Alert dentro aggro pero fuera attack → Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);
}

TEST_CASE("F4H8 Chase agrega NavAgentComponent + target/speed/active") {
    auto root = setupAssetRoot("chase_navagent");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));
    REQUIRE_FALSE(enemy.hasComponent<NavAgentComponent>());

    // 2 ticks: Idle → Alert → Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);

    REQUIRE(enemy.hasComponent<NavAgentComponent>());
    const auto& nav = enemy.getComponent<NavAgentComponent>();
    CHECK(nav.active == true);
    CHECK(nav.target.x == doctest::Approx(0.0f));
    CHECK(nav.target.z == doctest::Approx(0.0f));
    CHECK(nav.speed   == doctest::Approx(4.0f));  // spec.moveSpeed default
}

TEST_CASE("F4H8 Attack mantiene NavAgent active (tracking continuo)") {
    auto root = setupAssetRoot("attack_tracking");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(1.5f, 0.0f, 0.0f));

    // Tick 1: Idle → Alert.
    // Tick 2: Alert → Attack (dist 1.5 <= attackRange 2).
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Attack);

    // F4H8 D1: NavAgent agregado + active=true en Attack.
    REQUIRE(enemy.hasComponent<NavAgentComponent>());
    CHECK(enemy.getComponent<NavAgentComponent>().active == true);
}

TEST_CASE("F4H8 Chase actualiza target cada tick si player se mueve") {
    auto root = setupAssetRoot("target_updates");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // 2 ticks → Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.hasComponent<NavAgentComponent>());
    const auto firstTarget = enemy.getComponent<NavAgentComponent>().target;

    // Player se mueve.
    player.getComponent<TransformComponent>().position = glm::vec3(2.0f, 0.0f, 3.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    const auto secondTarget = enemy.getComponent<NavAgentComponent>().target;
    CHECK(secondTarget.x == doctest::Approx(2.0f));
    CHECK(secondTarget.z == doctest::Approx(3.0f));
    CHECK(firstTarget.x != doctest::Approx(secondTarget.x));
}

TEST_CASE("F4H8 Pain desactiva NavAgent") {
    auto root = setupAssetRoot("pain_deactivate");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Setup Chase con NavAgent activo.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);
    REQUIRE(enemy.getComponent<NavAgentComponent>().active == true);

    // Damage → Pain.
    Health::applyDamage(scene, enemy, 20.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Pain);

    // F4H8: NavAgent desactivado en Pain.
    CHECK(enemy.getComponent<NavAgentComponent>().active == false);
}

TEST_CASE("F4H8 Dead desactiva NavAgent (evita pelea con Dynamic RB)") {
    auto root = setupAssetRoot("dead_deactivate_nav");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f), 10.0f);

    // Setup Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);
    REQUIRE(enemy.getComponent<NavAgentComponent>().active == true);

    // Kill.
    Health::applyDamage(scene, enemy, 999.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Dead);

    // F4H8 (D8): NavAgent desactivado para no pelear con Dynamic RB.
    CHECK(enemy.getComponent<NavAgentComponent>().active == false);
}

TEST_CASE("F4H8 Idle desactiva NavAgent (player se aleja)") {
    auto root = setupAssetRoot("idle_deactivate_nav");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Setup Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<NavAgentComponent>().active == true);

    // Player se aleja fuera de aggro * 1.5.
    player.getComponent<TransformComponent>().position = glm::vec3(50.0f, 0.0f, 0.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Idle);
    CHECK(enemy.getComponent<NavAgentComponent>().active == false);
}

TEST_CASE("F4H8 NO double-add NavAgent en transitions Chase↔Idle↔Chase") {
    auto root = setupAssetRoot("no_dupe_nav");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(5.0f, 0.0f, 0.0f));

    // Chase.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.hasComponent<NavAgentComponent>());

    // Player se aleja → Idle.
    player.getComponent<TransformComponent>().position = glm::vec3(50.0f, 0.0f, 0.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Idle);

    // Player vuelve cerca → Alert → Chase.
    player.getComponent<TransformComponent>().position = glm::vec3(0.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);

    // Verificar que solo hay 1 NavAgent en toda la scene.
    int navCount = 0;
    scene.registry().view<NavAgentComponent>().each(
        [&](entt::entity, NavAgentComponent&) { navCount += 1; });
    CHECK(navCount == 1);
}

TEST_CASE("F4H8 Attack→Chase si player se aleja > attackRange * 1.5") {
    auto root = setupAssetRoot("attack_chase");
    auto assets = makeAssets(root);
    Scene scene;
    Entity player = makePlayer(scene, glm::vec3(0.0f));
    Entity enemy = makeEnemy(scene, "e1", glm::vec3(1.5f, 0.0f, 0.0f));

    // Setup Attack.
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);
    REQUIRE(enemy.getComponent<EnemyComponent>().state == EnemyState::Attack);

    // Player se aleja a 5m (> attackRange*1.5 = 3m, < aggro 12).
    player.getComponent<TransformComponent>().position = glm::vec3(5.0f, 0.0f, 0.0f);
    Enemy::tickSystem(scene, 0.016f, player, nullptr, *assets);

    // F4H8: Attack → Chase (no Alert) — el A* persigue.
    CHECK(enemy.getComponent<EnemyComponent>().state == EnemyState::Chase);
}
