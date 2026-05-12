// Tests del singleton GameState y de la tabla `hud` registrada en
// LuaBindings (Hito 20 Bloque 5).
//
// GameState es proceso-global, asi que cada test resetea al final para
// no filtrar estado a tests siguientes. doctest corre los TEST_CASE en
// orden de aparicion en el archivo, pero igual el reset cubre el caso
// futuro de paralelizacion / shuffle.

#include <doctest/doctest.h>

#include "engine/game/state/GameState.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/scripting/ScriptSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Mood;

namespace {

std::filesystem::path writeTempScript(const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = dir / ("mood_test_hud_" + std::to_string(stamp) + ".lua");
    std::ofstream f(path, std::ios::binary);
    f << content;
    return path;
}

} // namespace

TEST_CASE("GameState: defaults razonables") {
    GameState::reset();
    CHECK(GameState::hud().hp   == 100);
    CHECK(GameState::hud().ammo == 30);
    CHECK(GameState::paused()   == false);
}

TEST_CASE("GameState: mutacion directa C++ persiste") {
    GameState::reset();
    GameState::hud().hp     = 42;
    GameState::hud().ammo   = 7;
    GameState::paused()     = true;

    CHECK(GameState::hud().hp   == 42);
    CHECK(GameState::hud().ammo == 7);
    CHECK(GameState::paused()   == true);

    GameState::reset();
}

TEST_CASE("GameState: reset() vuelve a defaults") {
    GameState::hud().hp   = 1;
    GameState::hud().ammo = 1;
    GameState::paused()   = true;

    GameState::reset();

    CHECK(GameState::hud().hp   == 100);
    CHECK(GameState::hud().ammo == 30);
    CHECK(GameState::paused()   == false);
}

TEST_CASE("Lua hud.setHp / setAmmo / setPaused mutan GameState") {
    GameState::reset();

    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            hud.setHp(55)
            hud.setAmmo(13)
            hud.setPaused(true)
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("hud_writer");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    CHECK(GameState::hud().hp   == 55);
    CHECK(GameState::hud().ammo == 13);
    CHECK(GameState::paused()   == true);

    std::filesystem::remove(path);
    GameState::reset();
}

TEST_CASE("Lua hud.getHp / getAmmo / getPaused leen GameState") {
    GameState::reset();
    GameState::hud().hp   = 88;
    GameState::hud().ammo = 99;
    GameState::paused()   = true;

    // El script copia lo leido de C++ a la posicion del Transform como
    // canal de exfiltracion verificable desde el host (no hay un binding
    // de "leer al host" mas directo).
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = hud.getHp()
            self.transform.position.y = hud.getAmmo()
            self.transform.position.z = hud.getPaused() and 1.0 or 0.0
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("hud_reader");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    const auto& tf = e.getComponent<TransformComponent>();
    CHECK(tf.position.x == doctest::Approx(88.0f));
    CHECK(tf.position.y == doctest::Approx(99.0f));
    CHECK(tf.position.z == doctest::Approx(1.0f));

    std::filesystem::remove(path);
    GameState::reset();
}

TEST_CASE("Lua hud: dos scripts comparten el mismo GameState") {
    // Cada entidad tiene su propio sol::state aislado, pero la tabla
    // `hud` se mapea a la misma instancia singleton. Verificamos que un
    // script ve el efecto del otro.
    GameState::reset();

    const auto pathWriter = writeTempScript(R"(
        function onUpdate(self, dt)
            hud.setHp(33)
        end
    )");
    const auto pathReader = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = hud.getHp()
        end
    )");

    Scene scene;
    Entity writer = scene.createEntity("writer");
    writer.addComponent<ScriptComponent>(pathWriter.generic_string());
    Entity reader = scene.createEntity("reader");
    reader.addComponent<ScriptComponent>(pathReader.generic_string());

    ScriptSystem sys;
    // Dos updates: el orden de iteracion del registry de EnTT no esta
    // garantizado. Si en el primer frame el reader corre antes del
    // writer, lee el HP default. Tras un segundo update, el writer ya
    // setteo HP=33 en el frame anterior, asi que el reader lo ve
    // estable.
    sys.update(scene, 0.016f);
    sys.update(scene, 0.016f);

    CHECK(reader.getComponent<TransformComponent>().position.x == doctest::Approx(33.0f));

    std::filesystem::remove(pathWriter);
    std::filesystem::remove(pathReader);
    GameState::reset();
}

// ============================================================
// F2H52 Bloque I: hud.open_container / close_container
// ============================================================

#include "engine/scripting/bindings/LuaBindings.h"

TEST_CASE("hud.open_container: setea container_open + container_target + enable widget") {
    GameState::reset();
    Scene scene;
    Entity chest = scene.createEntity("chest");
    chest.addComponent<InventoryComponent>();

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    setupLuaBindings(lua, chest);

    REQUIRE_FALSE(GameState::hud().container_open);
    REQUIRE_FALSE(GameState::hud().isWidgetEnabled("inventory_panel"));

    lua["target"] = chest;
    lua.safe_script("hud.open_container(target)");

    CHECK(GameState::hud().container_open);
    CHECK(GameState::hud().container_target ==
          static_cast<u32>(chest.handle()));
    CHECK(GameState::hud().isWidgetEnabled("inventory_panel"));

    GameState::reset();
}

TEST_CASE("hud.close_container: limpia container_open sin afectar widget") {
    GameState::reset();
    Scene scene;
    Entity chest = scene.createEntity("chest");
    chest.addComponent<InventoryComponent>();

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    setupLuaBindings(lua, chest);

    lua["target"] = chest;
    lua.safe_script("hud.open_container(target)");
    REQUIRE(GameState::hud().container_open);

    lua.safe_script("hud.close_container()");
    CHECK_FALSE(GameState::hud().container_open);
    // close_container NO desactiva el widget (el dev puede preferir
    // dejarlo abierto en single-mode tras cerrar el cofre).
    CHECK(GameState::hud().isWidgetEnabled("inventory_panel"));

    GameState::reset();
}

TEST_CASE("hud.has_container + hud.container_entity") {
    GameState::reset();
    Scene scene;
    Entity chest = scene.createEntity("chest");
    chest.addComponent<InventoryComponent>();

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    setupLuaBindings(lua, chest);

    lua.safe_script(R"(
        v_has_pre = hud.has_container()
        v_ent_pre = hud.container_entity()  -- nil sin container
    )");
    CHECK(lua["v_has_pre"].get<bool>() == false);
    CHECK(lua["v_ent_pre"].get<sol::optional<u32>>().has_value() == false);

    lua["target"] = chest;
    lua.safe_script(R"(
        hud.open_container(target)
        v_has_post = hud.has_container()
        v_ent_post = hud.container_entity()
    )");
    CHECK(lua["v_has_post"].get<bool>() == true);
    CHECK(lua["v_ent_post"].get<u32>() == static_cast<u32>(chest.handle()));

    GameState::reset();
}

TEST_CASE("GameState::reset() limpia container_open + container_target") {
    GameState::hud().container_open   = true;
    GameState::hud().container_target = 42u;
    GameState::reset();
    CHECK_FALSE(GameState::hud().container_open);
    CHECK(GameState::hud().container_target == 0u);
}
