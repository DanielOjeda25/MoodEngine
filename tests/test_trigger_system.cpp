// Tests del TriggerSystem (Hito 33 Bloque 3) + ScriptSystem::dispatchEvent.
// Cubren:
//   - el flag `playerInside` se actualiza segun la posicion del char.
//   - playerCharId == 0 fuerza playerInside=false en todos los triggers.
//   - on_trigger_enter / on_trigger_exit se dispatchan al script (verificado
//     via side-effect en GameState::hud()).

#include <doctest/doctest.h>

#include "engine/game/GameState.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/ScriptSystem.h"
#include "systems/TriggerSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Mood;

namespace {

constexpr f32 k_charHalfHeight = 0.5f;
constexpr f32 k_charRadius     = 0.4f;

std::filesystem::path writeTempScript(const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = dir / ("mood_trigger_test_" + std::to_string(stamp) + ".lua");
    std::ofstream f(path, std::ios::binary);
    f << content;
    return path;
}

} // namespace

TEST_CASE("TriggerSystem: playerInside flip false->true cuando char entra al AABB") {
    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.getComponent<TransformComponent>().position = glm::vec3(0.0f);
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});

    PhysicsWorld pw;
    // Char en (0, 0, 0) — exactamente en el centro del trigger.
    const u32 charId = pw.createCharacter(glm::vec3(0.0f),
                                            k_charHalfHeight, k_charRadius);

    ScriptSystem scripts;
    TriggerSystem triggers;

    REQUIRE_FALSE(trig.getComponent<TriggerComponent>().playerInside);
    triggers.update(scene, pw, scripts, charId);
    CHECK(trig.getComponent<TriggerComponent>().playerInside);
}

TEST_CASE("TriggerSystem: playerInside flip true->false cuando char sale") {
    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.getComponent<TransformComponent>().position = glm::vec3(0.0f);
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});

    PhysicsWorld pw;
    const u32 charId = pw.createCharacter(glm::vec3(0.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    triggers.update(scene, pw, scripts, charId);
    REQUIRE(trig.getComponent<TriggerComponent>().playerInside);

    // Teleport bien fuera del AABB.
    pw.setCharacterPosition(charId, glm::vec3(10.0f, 10.0f, 10.0f));
    triggers.update(scene, pw, scripts, charId);
    CHECK_FALSE(trig.getComponent<TriggerComponent>().playerInside);
}

TEST_CASE("TriggerSystem: playerCharId == 0 fuerza playerInside=false") {
    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});
    // Forzamos playerInside=true a mano para verificar que update lo limpie.
    trig.getComponent<TriggerComponent>().playerInside = true;

    PhysicsWorld pw;
    ScriptSystem scripts;
    TriggerSystem triggers;

    triggers.update(scene, pw, scripts, /*playerCharId=*/0u);
    CHECK_FALSE(trig.getComponent<TriggerComponent>().playerInside);
}

TEST_CASE("TriggerSystem: dispatcha on_trigger_enter al script en flank true") {
    // Script que escribe a hud.hp cuando entra/sale. hud.* es un binding
    // global (LuaBindings) que mapea a GameState::hud() — observable
    // desde C++.
    const auto path = writeTempScript(R"(
        function on_trigger_enter()
            hud.setHp(99)
        end
        function on_trigger_exit()
            hud.setHp(0)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    PhysicsWorld pw;
    const u32 charId = pw.createCharacter(glm::vec3(0.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;

    // ScriptSystem::update carga el script y registra el sol::state. No
    // hace falta que el script tenga onUpdate — solo necesitamos que el
    // state exista en m_states para que dispatchEvent lo encuentre.
    scripts.update(scene, 0.016f);

    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 99);

    pw.setCharacterPosition(charId, glm::vec3(10.0f));
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 0);

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: no dispatch enter si no hay flank change") {
    const auto path = writeTempScript(R"(
        function on_trigger_enter()
            hud.setHp(hud.getHp() + 1)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    PhysicsWorld pw;
    const u32 charId = pw.createCharacter(glm::vec3(0.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    // Tres updates seguidos con el char dentro: solo el primero gatilla
    // on_trigger_enter; los siguientes son no-op (mismo flag).
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 1);

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: dispatcha on_trigger_stay cada frame que el player sigue dentro (Hito 36 C)") {
    // Script con stay que incrementa hud.hp. enter/exit los dejamos
    // como no-op observable (no toca hp) para aislar el conteo de stays.
    const auto path = writeTempScript(R"(
        function on_trigger_stay()
            hud.setHp(hud.getHp() + 1)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    PhysicsWorld pw;
    const u32 charId = pw.createCharacter(glm::vec3(0.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    // Frame 1: char entra (false->true). Dispatch enter, NO stay.
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 0);  // enter no toca hp en este script

    // Frame 2..4: insideNow == true sin flank -> dispatcha stay cada vez.
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 3);

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: detecta dynamic body entrando al AABB (Hito 37 B)") {
    // Body dentro del trigger desde el inicio. Verificamos que
    // on_trigger_body_enter se dispatcha en el primer update.
    const auto path = writeTempScript(R"(
        function on_trigger_body_enter(bodyId)
            hud.setHp(hud.getHp() + 100)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(2.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    Entity box = scene.createEntity("box");
    auto& bt = box.getComponent<TransformComponent>();
    bt.position = glm::vec3(0.0f);  // dentro del AABB del trigger
    box.addComponent<RigidBodyComponent>(RigidBodyComponent{
        RigidBodyComponent::Type::Dynamic, RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f), 1.0f});

    PhysicsWorld pw;
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    triggers.update(scene, pw, scripts, /*playerCharId=*/0);
    CHECK(GameState::hud().hp == 100);

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: dispatcha body_exit al salir del AABB (Hito 37 B)") {
    const auto path = writeTempScript(R"(
        function on_trigger_body_enter(bodyId)
            hud.setHp(hud.getHp() + 1)
        end
        function on_trigger_body_exit(bodyId)
            hud.setHp(hud.getHp() + 1000)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(2.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    Entity box = scene.createEntity("box");
    auto& bt = box.getComponent<TransformComponent>();
    bt.position = glm::vec3(0.0f);  // adentro
    box.addComponent<RigidBodyComponent>(RigidBodyComponent{
        RigidBodyComponent::Type::Dynamic, RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f), 1.0f});

    PhysicsWorld pw;
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    // Frame 1: enter
    triggers.update(scene, pw, scripts, 0);
    CHECK(GameState::hud().hp == 1);
    // Frame 2: mover el body fuera del AABB.
    bt.position = glm::vec3(10.0f, 10.0f, 10.0f);
    triggers.update(scene, pw, scripts, 0);
    CHECK(GameState::hud().hp == 1001);  // 1 (enter) + 1000 (exit)

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: bodies Static no se detectan (Hito 37 B)") {
    const auto path = writeTempScript(R"(
        function on_trigger_body_enter(bodyId)
            hud.setHp(hud.getHp() + 1)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(2.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    // Body STATIC dentro del trigger — debe ser ignorado por el loop
    // body-detection (Static no se mueve, no aporta valor, evita spam).
    Entity wall = scene.createEntity("wall");
    auto& wt = wall.getComponent<TransformComponent>();
    wt.position = glm::vec3(0.0f);
    wall.addComponent<RigidBodyComponent>(RigidBodyComponent{
        RigidBodyComponent::Type::Static, RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f), 0.0f});

    PhysicsWorld pw;
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    triggers.update(scene, pw, scripts, 0);
    CHECK(GameState::hud().hp == 0);  // Static ignorado

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: OBB respeta rotation del Transform (Hito 39 B)") {
    // Trigger rotado 45 deg en Y, halfExtents (0.5, 0.5, 0.5). Sin
    // rotation, el AABB axis-aligned aceptaria un punto en (0.7, 0, 0)
    // como AFUERA (porque |0.7| > 0.5). Con rotation 45 alrededor del Y,
    // el OBB local corresponde a un cubo que cubre +/-0.7 en world X
    // (la diagonal del cubo unit es sqrt(0.5)). En espacio local del
    // trigger, ese punto ~(0.7, 0, 0) world se mapea a ~(0.495, 0, -0.495)
    // local — adentro del [-0.5, +0.5]^3.
    const auto path = writeTempScript(R"(
        function on_trigger_enter()
            hud.setHp(hud.getHp() + 100)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    auto& trigT = trig.getComponent<TransformComponent>();
    trigT.position = glm::vec3(0.0f);
    trigT.rotationEuler = glm::vec3(0.0f, 45.0f, 0.0f);  // 45 deg en Y
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(0.5f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    PhysicsWorld pw;
    // Char en (0.7, 0, 0) world: afuera del AABB axis-aligned standard
    // pero adentro del OBB rotado.
    const u32 charId = pw.createCharacter(glm::vec3(0.7f, 0.0f, 0.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 100);  // OBB lo detecto

    std::filesystem::remove(path);
}

TEST_CASE("TriggerSystem: NO dispatcha stay cuando el player esta fuera (Hito 36 C)") {
    const auto path = writeTempScript(R"(
        function on_trigger_stay()
            hud.setHp(hud.getHp() + 1)
        end
    )");

    Scene scene;
    Entity trig = scene.createEntity("trigger");
    trig.addComponent<TriggerComponent>(TriggerComponent{glm::vec3(1.0f)});
    trig.addComponent<ScriptComponent>(path.generic_string());

    PhysicsWorld pw;
    // Char bien afuera del AABB.
    const u32 charId = pw.createCharacter(glm::vec3(10.0f),
                                            k_charHalfHeight, k_charRadius);
    ScriptSystem scripts;
    TriggerSystem triggers;

    GameState::hud().hp = 0;
    scripts.update(scene, 0.016f);

    // Tres updates con el char afuera: ningun dispatch (insideNow==false
    // y playerInside==false desde el inicio = no flank, no stay).
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    triggers.update(scene, pw, scripts, charId);
    CHECK(GameState::hud().hp == 0);

    std::filesystem::remove(path);
}
