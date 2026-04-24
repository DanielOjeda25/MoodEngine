// Tests de ScriptSystem + LuaBindings (Hito 8 Bloque 7).
// Escriben scripts a archivos temporales y corren el sistema sin GL ni
// editor. Cubren:
//   - carga inicial y mutacion de TransformComponent desde Lua.
//   - hot-reload: un cambio de mtime dispara re-ejecucion del .lua.

#include <doctest/doctest.h>

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "systems/ScriptSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace Mood;

namespace {

std::filesystem::path writeTempScript(const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = dir / ("mood_test_" + std::to_string(stamp) + ".lua");
    std::ofstream f(path, std::ios::binary);
    f << content;
    return path;
}

} // namespace

TEST_CASE("ScriptSystem: onUpdate muta TransformComponent de self") {
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = 7.5
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("victim");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(7.5f));

    std::filesystem::remove(path);
}

TEST_CASE("ScriptSystem: hot-reload re-ejecuta al cambiar mtime") {
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = 1.0
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("victim");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));

    // Sleep para que la resolucion del filesystem reporte un mtime nuevo.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << R"(
            function onUpdate(self, dt)
                self.transform.position.x = 2.0
            end
        )";
    }

    // dt >= 0.5s dispara el throttle del check de mtime en este mismo update.
    sys.update(scene, 1.0f);
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));

    std::filesystem::remove(path);
}

TEST_CASE("ScriptSystem: script con error guarda lastError y no crashea") {
    const auto path = writeTempScript("esto no es lua valido @#$");

    Scene scene;
    Entity e = scene.createEntity("broken");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    const auto& sc = e.getComponent<ScriptComponent>();
    CHECK_FALSE(sc.loaded);
    CHECK_FALSE(sc.lastError.empty());

    std::filesystem::remove(path);
}
